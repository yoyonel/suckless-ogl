# Intégration Steam & Proton : Déploiement et Debugging

**Horodatage :** 2026-06-26 17:10:09 CEST
**Stack :** Moteur C11 Custom (OpenGL), Linux, Flatpak Steam, Proton Experimental, Justfile.

Cette documentation consolide les pratiques et les contournements nécessaires pour packager, tester et profiler de manière fiable une application native Win64 sous un environnement Linux via Steam Flatpak et Proton.

---

## 1. Architecture "Suckless" du Binaire (CWD & Logs)

La majorité des crashs silencieux sous Proton sont causés par une mauvaise gestion du répertoire de travail courant (CWD) par l'interface Steam, ou par la suppression des flux de sortie `stdout`/`stderr` lors de la compilation avec `-mwindows` (qui désactive la console).

Pour qu'un moteur soit "increvable" quel que soit son lanceur (CLI, Raccourci Bureau, Steam), il doit autogérer son CWD et sécuriser ses propres logs au démarrage.

Dans notre projet, cette logique est déportée dans les utilitaires de plateforme pour simplifier le point d'entrée de l'application.

**Implémentation dans `src/platform/platform_fs.c` :**

```c
#include "platform/platform_fs.h"
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #include <windows.h>
    #define CHDIR _chdir
    #define ACCESS _access
    #define F_OK 0
#else
    #include <unistd.h>
    #define CHDIR chdir
    #define ACCESS access
    #define F_OK 0
#endif

void platform_setup_working_dir(const char* exec_path) {
    if (!exec_path || strlen(exec_path) == 0) {
        return;
    }

    // 1. Si les ressources indispensables sont déjà présentes, on ne change rien
    if (ACCESS("shaders", F_OK) == 0 && ACCESS("assets", F_OK) == 0) {
        return;
    }

    // 2. Résolution du chemin absolu de l'exécutable
    char path[1024];
    strncpy(path, exec_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    // Recherche du dernier séparateur de dossier (Windows ou Unix)
    char* last_slash = strrchr(path, '\\');
    if (!last_slash) {
        last_slash = strrchr(path, '/');
    }

    if (last_slash) {
        *last_slash = '\0';
        CHDIR(path);
    }

    // 3. Remonter jusqu'à 4 niveaux parents si nécessaire (utile en mode dev)
    for (int i = 0; i < 4; ++i) {
        if (ACCESS("shaders", F_OK) == 0 && ACCESS("assets", F_OK) == 0) {
            break;
        }
        CHDIR("..");
    }
}
```

**Point d'entrée sécurisé dans `src/main.c` :**

```c
#include "platform/platform_fs.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    // 1. Sécuriser le dossier de travail et localiser les ressources
    if (argc > 0) {
        platform_setup_working_dir(argv[0]);
    }

    // 2. Préservation des flux de logs (Crucial avec -mwindows)
    // Steam supprime l'accès au terminal. Ces logs se retrouveront
    // à côté de l'exécutable et garantiront une visibilité sur les erreurs.
    FILE* dummy_err = freopen("suckless_crash.log", "w", stderr);
    FILE* dummy_out = freopen("suckless_output.log", "w", stdout);
    (void)dummy_err;
    (void)dummy_out;

    // Initialisation et boucle de rendu du moteur
    // ...
    return EXIT_SUCCESS;
}
```

---

## 2. Compilation et Validation Automatisée CLI (Justfile)

Pour compiler et valider une release sans passer manuellement par l'interface graphique de Steam, une commande de validation automatisée et agnostique de l'environnement est fournie. La cible `run-package-win` appelle un script d'assistance (`scripts/run_proton.sh`) qui détecte automatiquement si Steam tourne en version Flatpak ou en version native, configure l'environnement, s'assure de l'existence du préfixe Proton, et lance l'application via Proton.

Lors des tests locaux de la release, le préfixe de compatibilité Proton est créé dans le dossier `test-dist/proton_pfx` (ignoré par git) pour garder l'environnement de test autonome. Pour la cible directe `run-proton`, le préfixe est défini dans le dossier `compatdata` de Steam pour éviter de polluer l'espace de travail.

### Commandes Utiles

- **Configuration de la compilation croisée** :
  ```bash
  just configure-win
  ```
- **Compilation** :
  ```bash
  just build-win
  ```
- **Packaging de la version de distribution** (archive compressée atomiquement avec Zstandard) :
  ```bash
  just package-win
  ```
- **Validation automatisée de la release** :
  ```bash
  just run-package-win
  ```

**Définition des cibles de test sous Proton dans le `Justfile` :**

```justfile
steam_root := env_var("HOME") + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"
proton_path := steam_root + "/steamapps/common/Proton - Experimental/proton"
proton_prefix := steam_root + "/steamapps/compatdata/suckless-ogl"

# Lancement direct du binaire en cours de développement sous Proton (Flatpak uniquement)
run-proton: build-win
    @echo "Création du préfixe Proton..."
    @mkdir -p "{{ proton_prefix }}"
    @echo "Lancement via Steam Flatpak..."
    @flatpak run \
        --filesystem="{{ justfile_directory() }}" \
        --env=STEAM_COMPAT_CLIENT_INSTALL_PATH="{{ steam_root }}" \
        --env=STEAM_COMPAT_DATA_PATH="{{ proton_prefix }}" \
        --command=python3 \
        com.valvesoftware.Steam \
        "{{ proton_path }}" run "{{ justfile_directory() }}/{{ build_win_dir }}/app.exe"

# Validation automatisée (Agnostique)
run-package-win: package-win
    @echo "Lancement de l'environnement de test automatisé..."
    @scripts/run_proton.sh "{{ release_dir }}" "{{ release_name }}" "{{ test_dist_dir }}" "{{ justfile_directory() }}"
```

**Script d'enveloppement pour la détection de l'environnement Steam (`scripts/run_proton.sh`) :**

```bash
#!/usr/bin/env bash
# scripts/run_proton.sh
set -e

# Arguments passés par le Justfile
RELEASE_DIR="$1"
RELEASE_NAME="$2"
TEST_DIST_DIR="$3"
PROJECT_ROOT="$4"

EXTRACTED_APP="${TEST_DIST_DIR}/${RELEASE_NAME}/app.exe"
PROTON_PFX="${TEST_DIST_DIR}/proton_pfx"

echo "==> 1. Nettoyage et extraction de l'archive de release..."
rm -rf "${TEST_DIST_DIR}"
mkdir -p "${TEST_DIST_DIR}"
tar -I 'zstd' -xf "${RELEASE_DIR}/../${RELEASE_NAME}.tar.zst" -C "${TEST_DIST_DIR}"

# S'assurer que le dossier du préfixe Proton existe avant de lancer Proton (Flatpak ou Natif)
mkdir -p "${PROTON_PFX}"

echo "==> 2. Détection de l'environnement Steam..."

# Test A : Présence d'un environnement Flatpak
if command -v flatpak &>/dev/null && flatpak info com.valvesoftware.Steam &>/dev/null; then
    echo "    [i] Environnement détecté : Steam Flatpak (Sandbox)"
    STEAM_ROOT="${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam"
    PROTON_PATH="${STEAM_ROOT}/steamapps/common/Proton - Experimental/proton"

    echo "==> 3. Lancement du moteur via Flatpak..."
    cd "${TEST_DIST_DIR}/${RELEASE_NAME}"
    flatpak run \
        --filesystem="${PROJECT_ROOT}" \
        --env=STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT}" \
        --env=STEAM_COMPAT_DATA_PATH="${PROTON_PFX}" \
        --command=python3 \
        com.valvesoftware.Steam \
        "${PROTON_PATH}" run "${EXTRACTED_APP}"

# Test B : Présence d'un environnement Natif (Bazzite, Fedora, Arch...)
elif [ -d "${HOME}/.local/share/Steam" ]; then
    echo "    [i] Environnement détecté : Steam Natif"
    STEAM_ROOT="${HOME}/.local/share/Steam"
    PROTON_PATH="${STEAM_ROOT}/steamapps/common/Proton - Experimental/proton"

    echo "==> 3. Lancement du moteur via Proton Natif..."

    cd "${TEST_DIST_DIR}/${RELEASE_NAME}"
    STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT}" \
        STEAM_COMPAT_DATA_PATH="${PROTON_PFX}" \
        "${PROTON_PATH}" run "${EXTRACTED_APP}"

else
    echo "    [x] Erreur : Aucune installation de Steam trouvée (ni Flatpak, ni Native)."
    exit 1
fi
```

---

## 3. Ajout GUI Steam & Le Piège du Portail Flatpak

Lors de l'ajout du jeu non-Steam via l'interface graphique du client Steam (Bouton "Ajouter un jeu"), il est impératif de comprendre la gestion des permissions FUSE de Flatpak.

### Le "Trou Noir" FUSE (Erreur 193)
Ne jamais utiliser les raccourcis récents ou le dossier virtuel `Documents/` dans l'explorateur de fichiers Steam. Cela passe par le *XDG Desktop Portal* qui monte le fichier cible en lecture seule dans `/run/user/1000/doc/...`.

Cela provoque deux problèmes majeurs :
1. Proton ne peut pas voir le dossier réel contenant les shaders et les assets, ce qui cause un plantage immédiat au démarrage.
2. L'accès en lecture à l'exécutable peut être tronqué à **0 octet**, ce qui déclenche le code d'erreur Win32 `193` (`ERROR_BAD_EXE_FORMAT`).

### La Méthode Fiable
1. Dans l'explorateur Steam, naviguez physiquement depuis la racine `/` jusqu'au chemin absolu sous votre dossier utilisateur :
   `/home/USER/Prog/.../test-dist/suckless-ogl-windows-v0.1.0/app.exe`
2. Ouvrez les **Propriétés > Compatibilité** du raccourci dans Steam et cochez **Forcer l'utilisation d'un outil de compatibilité**, puis sélectionnez **Proton Experimental** (ou Proton 9.x).
3. Dans **Propriétés > Raccourci**, assurez-vous que les guillemets `" "` encadrent correctement les chemins absolus dans les champs **Cible** et **Démarrer dans**.

---

## 4. Profiling Avancé et Overlay (MangoHud / OpenGL)

Contrairement aux applications Vulkan, MangoHud ne s'injecte pas automatiquement dans les processus OpenGL via de simples variables d'environnement globales. L'injection de la bibliothèque (`LD_PRELOAD`) doit être forcée.

Pour analyser les performances du moteur (Frametimes, charge CPU/GPU, VRAM) via l'overlay MangoHud sous Steam, configurez la ligne suivante dans les **Options de lancement** du raccourci Steam :

```text
STEAM_COMPAT_MOUNTS="/chemin/absolu/vers/test-dist/" WINEPREFIX="%compat%" mangohud %command%
```

* **`STEAM_COMPAT_MOUNTS`** : Force le sous-conteneur sécurisé de Valve (*Pressure Vessel*) à monter le répertoire de développement sur l'hôte, garantissant ainsi que Proton puisse y lire et y écrire librement (notamment pour générer les fichiers `suckless_output.log` et `suckless_crash.log`).
* **`mangohud %command%`** : Exécute explicitement le wrapper MangoHud devant la commande de lancement Proton, interceptant ainsi le contexte OpenGL créé par l'exécutable Windows.
