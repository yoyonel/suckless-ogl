import os
import subprocess
import threading
import time
import warnings
from pathlib import Path
from queue import Empty, Queue

import typer
from rich.console import Console

# Masque le warning Python 3.12 lié au code source de pyautogui
warnings.filterwarnings("ignore", category=SyntaxWarning, module="pyautogui")
import pyautogui  # noqa: E402

app = typer.Typer(help="CLI de test End-to-End pour suckless-ogl")
console = Console()


def verify_binary_integrity(binary_path: Path) -> bool:
    """Vérifie l'existence et les symboles du binaire."""
    if not binary_path.is_file():
        console.print(
            f"[bold red]Erreur :[/bold red] Le binaire {binary_path} n'existe pas."
        )
        return False

    console.print(
        f"[cyan]Vérification de l'intégrité du binaire : {binary_path}[/cyan]"
    )
    try:
        nm_result = subprocess.run(
            ["nm", str(binary_path)], capture_output=True, text=True, check=True
        )
        if "__tsan" not in nm_result.stdout:
            console.print("[bold red]Erreur :[/bold red] Aucun symbole __tsan trouvé.")
            return False
        console.print("[green]✓ Symboles TSan présents.[/green]")
    except subprocess.CalledProcessError:
        console.print("[bold red]Erreur lors de l'exécution de 'nm'.[/bold red]")
        return False
    except FileNotFoundError:
        console.print(
            "[yellow]Attention : Commande 'nm' introuvable. Skipping...[/yellow]"
        )

    try:
        readelf_result = subprocess.run(
            ["readelf", "-S", str(binary_path)],
            capture_output=True,
            text=True,
            check=True,
        )
        if "debug" not in readelf_result.stdout:
            console.print(
                "[bold red]Erreur :[/bold red] Aucune section 'debug' trouvée. (Strippé ?)[/bold red]"
            )
            return False
        console.print("[green]✓ Sections de débogage présentes.[/green]")
    except subprocess.CalledProcessError:
        console.print("[bold red]Erreur lors de l'exécution de 'readelf'.[/bold red]")
        return False
    except FileNotFoundError:
        console.print(
            "[yellow]Attention : Commande 'readelf' introuvable. Skipping...[/yellow]"
        )

    return True


def enqueue_output(out_stream, queue: Queue):
    """Lit le flux ligne par ligne et le pousse dans la Queue (exécuté dans un Thread)."""
    for line in iter(out_stream.readline, ""):
        queue.put(line)
    out_stream.close()


def wait_for_log(
    process: subprocess.Popen, log_queue: Queue, pattern: str, timeout: float
) -> int:
    """
    Attend un pattern spécifique de manière non bloquante.
    Retourne 0 (Succès), 1 (Crash), 2 (Timeout / Cache Hit).
    """
    start_time = time.monotonic()
    while time.monotonic() - start_time < timeout:
        if process.poll() is not None:
            return 1  # Crash
        try:
            line = log_queue.get_nowait()
            line_clean = line.strip()
            if line_clean:
                # Filtrer les logs trop bruyants si besoin, ici on affiche tout en grisé
                console.print(f"[dim]{line_clean}[/dim]")

            if pattern in line_clean:
                return 0  # Pattern trouvé
        except Empty:
            time.sleep(0.01)  # Soulage le CPU en attendant la prochaine ligne

    return 2  # Timeout


@app.command()
def run_test(
    binary_path: Path = typer.Argument(
        Path("build-tsan/app"),
        help="Chemin vers le binaire du moteur à tester.",
    ),
    tsan_suppressions: Path = typer.Option(
        Path("tsan_suppressions.txt"),
        "--tsan-suppressions",
        "-s",
        help="Fichier de suppressions pour ThreadSanitizer.",
    ),
    expected_log: str = typer.Option(
        "Finished loading & converting:",
        "--expected-log",
        "-l",
        help="Pattern de log confirmant le chargement d'un asset asynchrone.",
    ),
    iterations: int = typer.Option(
        30,
        "--iterations",
        "-i",
        help="Nombre de changements d'Environment Maps à effectuer.",
    ),
    delay_ms: int = typer.Option(
        200,
        "--delay",
        "-d",
        help="Délai en millisecondes après l'envoi de la touche.",
    ),
    timeout_sec: int = typer.Option(
        8,
        "--timeout",
        "-t",
        help="Timeout maximum (secondes) par itération.",
    ),
    skip_checks: bool = typer.Option(
        False,
        "--skip-checks",
        help="Désactive la vérification des symboles (nm/readelf).",
    ),
):
    """
    Lance le moteur et effectue un stress test asynchrone sur le chargement des Env Maps.
    """
    if not skip_checks and not verify_binary_integrity(binary_path):
        raise typer.Exit(code=1)

    console.print(
        f"\n[bold blue]Démarrage de l'application :[/bold blue] {binary_path}"
    )

    # Injection propre des variables d'environnement
    app_env = os.environ.copy()
    tsan_options = "halt_on_error=0:history_size=7"

    if tsan_suppressions.is_file():
        tsan_options += f":suppressions={tsan_suppressions.absolute()}"
        console.print(
            f"[green]✓ Fichier de suppression TSan appliqué :[/green] {tsan_suppressions}"
        )
    else:
        console.print(
            f"[yellow]⚠ Fichier de suppression '{tsan_suppressions}' introuvable. TSan sera bruyant.[/yellow]"
        )

    app_env["TSAN_OPTIONS"] = tsan_options

    # Lancement avec stdbuf pour bypasser le block-buffering C standard
    process = subprocess.Popen(
        ["stdbuf", "-oL", "-eL", str(binary_path)],
        env=app_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    if process.stdout is None:
        console.print("[bold red]Impossible d'attacher le flux de sortie.[/bold red]")
        raise typer.Exit(code=1)

    console.print(f"[dim]Processus instancié (PID: {process.pid})[/dim]\n")

    # Démarrage du Thread de lecture des logs
    log_queue = Queue()
    log_thread = threading.Thread(
        target=enqueue_output, args=(process.stdout, log_queue)
    )
    log_thread.daemon = True
    log_thread.start()

    console.print(
        "[yellow][INIT] En attente du premier chargement de la scène...[/yellow]"
    )
    status = wait_for_log(process, log_queue, expected_log, timeout=15.0)
    if status != 0:
        console.print(
            "[bold red]Échec : La scène initiale n'a pas pu charger à temps.[/bold red]"
        )
        process.kill()
        raise typer.Exit(code=1)

    console.print(
        "[bold green]Scène initiale prête. Début du stress test ![/bold green]\n"
    )

    # =========================================================================
    # BOUCLE DE STRESS TEST (Page_Down / Page_Up)
    # =========================================================================
    success_count = 0
    cache_count = 0

    for i in range(1, iterations + 1):
        if process.poll() is not None:
            console.print(f"\n[bold red]Crash inattendu à l'itération {i} ![/bold red]")
            break

        # Alternance pour cycler dans le buffer du moteur
        # key_to_press = "pagedown" if i % 2 != 0 else "pageup"
        key_to_press = "pagedown"

        # Envoi de la touche via pyautogui
        pyautogui.press(key_to_press)
        time.sleep(delay_ms / 1000.0)

        status = wait_for_log(
            process, log_queue, expected_log, timeout=float(timeout_sec)
        )

        if status == 1:
            console.print(
                f"\n[bold red]Crash détecté lors du chargement (Itération {i})[/bold red]"
            )
            break
        elif status == 2:
            cache_count += 1
            if i % 5 == 0 or i == iterations:
                console.print(
                    f"[yellow][CACHE][/yellow] Itération {i}/{iterations} ({key_to_press}) - Map déjà en VRAM/RAM"
                )
        else:
            success_count += 1
            if i % 5 == 0 or i == iterations:
                console.print(
                    f"[green][OK][/green]    Itération {i}/{iterations} ({key_to_press}) - Chargement asynchrone réussi"
                )

    # =========================================================================
    # CLÔTURE DU TEST
    # =========================================================================
    console.print(
        "\n[bold magenta]Test terminé. Envoi de la commande de sortie (Touche: Echap)...[/bold magenta]"
    )
    pyautogui.press("escape")

    try:
        process.wait(timeout=5.0)
        console.print(
            f"\n[bold cyan]Bilan : {success_count} Loads, {cache_count} Cache Hits[/bold cyan]"
        )
        if process.returncode == 0:
            console.print(
                "[bold green]Application fermée proprement (Code 0).[/bold green]"
            )
        else:
            console.print(
                f"[bold yellow]Application fermée avec le code d'erreur : {process.returncode}[/bold yellow]"
            )
            raise typer.Exit(code=process.returncode)

    except subprocess.TimeoutExpired:
        console.print(
            "[bold red]L'application refuse de se fermer. Force kill (SIGKILL)...[/bold red]"
        )
        process.kill()
        raise typer.Exit(code=1)


if __name__ == "__main__":
    app()
