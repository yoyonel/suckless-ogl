# Profilage CPU par FlameGraph

Ce document décrit le workflow de profilage CPU utilisant `perf` + FlameGraph pour identifier les goulots d'étranglement dans le code C.

## Prérequis

### Permissions perf

```bash
# Vérifier la valeur actuelle
cat /proc/sys/kernel/perf_event_paranoid

# Autoriser le profilage sans root (valeur 1)
echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid

# Rendre permanent (dans /etc/sysctl.conf)
kernel.perf_event_paranoid = 1
```

### Installation des outils

=== "Debian/Ubuntu"

    ```bash
    sudo apt install linux-perf linux-tools-common
    git clone https://github.com/brendangregg/FlameGraph /opt/FlameGraph
    ```

=== "Fedora/Bazzite"

    ```bash
    sudo dnf install perf
    git clone https://github.com/brendangregg/FlameGraph /opt/FlameGraph
    ```

=== "Arch"

    ```bash
    sudo pacman -S perf
    git clone https://github.com/brendangregg/FlameGraph /opt/FlameGraph
    ```

## Workflow standard

### 1. Compiler avec les symboles de débogage

```bash
# Mode Debug — inclut les symboles et désactive les optimisations agressives
just build
# ou pour la Release avec symboles
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### 2. Enregistrer un profil

```bash
# Enregistrement de 30 secondes
perf record -g -F 999 -o perf.data -- ./build/app

# Ou via Just
just perf
```

Options importantes :
- `-g` : Collecter les traces de pile (graphe d'appels)
- `-F 999` : Fréquence d'échantillonnage de 999 Hz
- `-o perf.data` : Fichier de sortie

### 3. Générer le FlameGraph

```bash
# Extraire les traces
perf script -i perf.data > /tmp/perf.out

# Plier les traces
/opt/FlameGraph/stackcollapse-perf.pl /tmp/perf.out > /tmp/perf.folded

# Générer le SVG interactif
/opt/FlameGraph/flamegraph.pl /tmp/perf.folded > /tmp/flamegraph.svg

# Ouvrir dans le navigateur
xdg-open /tmp/flamegraph.svg
```

### 4. Via script automatisé

```bash
# Script intégré au projet
just flamegraph
```

## Lecture des FlameGraphs

```
Axe X (horizontal) : Proportion du temps CPU total
Axe Y (vertical)   : Profondeur de la pile d'appels (bas = main, haut = feuilles)
Largeur d'un bloc   : % du temps passé dans cette fonction ET ses callees
Couleur             : Aléatoire (pour la lisibilité), pas de signification
```

**Comment identifier les goulots d'étranglement :**
- Les plateaux larges en haut de la pile = fonctions qui consomment du temps CPU directement
- Les colonnes larges = chemins d'appel fréquents (à optimiser en priorité)

## Exemple réel : `test_app`

Lors du profilage de `test_app`, les résultats typiques montrent :

| Fonction | % CPU | Action |
|----------|-------|--------|
| `glDrawArraysInstanced` | ~35 % | Normal — appel GPU principal |
| `glTexSubImage2D` | ~18 % | Upload de texture — candidat à l'optimisation PBO |
| `lum_downsample_pass` | ~8 % | Shader de réduction luminance |
| `pthread_cond_wait` | ~12 % | Attente async loader — normal |

## Compatibilité Bazzite / Flatpak

Sur Bazzite, `perf` peut nécessiter une configuration spécifique due aux chemins `/var/home` (voir [fix_lint_bazzite_paths.md](./fix_lint_bazzite_paths.md)) :

```bash
# Dans le conteneur Distrobox
distrobox enter clang-dev
perf record -g -- ./build/app
```

## Autres outils de profilage

| Outil | Usage | Documentation |
|-------|-------|--------------|
| Tracy | Profilage en temps réel avec timeline | [profiling_tracy.md](./profiling_tracy.md) |
| ApiTrace | Capture et replay des appels OpenGL | [profiling_guide.md](./profiling_guide.md) |
| Valgrind | Détection de fuites mémoire | `just memcheck` |
| RenderDoc | Capture et analyse GPU | [renderdoc_guide.md](./renderdoc_guide.md) |
