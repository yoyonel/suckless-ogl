# Guide de configuration IDE et environnement

Ce guide explique comment configurer un environnement de développement robuste pour **Suckless OGL**, en se concentrant sur les outils C modernes (clangd, CMake) et la compatibilité avec les flux de travail conteneurisés comme **Distrobox**.

## 1. Prérequis

### Outils essentiels
- **CMake** (3.14+)
- **Clang / GCC** (support C11)
- **Make** ou **Ninja**
- **clangd** (Serveur de langage)

### Éditeur recommandé
**VS Code** est fortement recommandé pour son excellente intégration `clangd`.
- Installez l'extension **clangd** (`llvm-vs-code-extensions.vscode-clangd`).
- *Optionnel* : **CodeLLDB** pour le débogage (`vadimcn.vscode-lldb`).

---

## 2. Architecture de la configuration

Ce projet utilise une **approche de construction hybride** pour s'assurer que les en-têtes são visibles à la fois pour le système de construction (à l'intérieur de Distrobox/Docker) et votre IDE (sur l'hôte).

### Composants clés
1. **CMake FetchContent** : On force `FetchContent` pour les bibliothèques tierces (comme GLFW) au lieu d'utiliser les paquets système (`pkg-config`).
    - *Pourquoi ?* Cela télécharge les sources dans `build/_deps/`, garantissant que les en-têtes sont physiquement présents dans le dossier du projet.
2. **Base de données de compilation** : On génère `compile_commands.json` qui indique à `clangd` exactement où chercher les en-têtes.

---

## 3. Étapes de configuration

### Étape 1 : Générer la base de données de compilation
Exécutez cette commande une fois (ou chaque fois que vous ajoutez de nouvelles cibles CMake). Fonctionne à l'intérieur de Distrobox ou sur un système Debian/Linux natif.

```bash
# Construction propre (optionnel mais recommandé pour une configuration fraîche)
rm -rf build/

# Configurer et générer compile_commands.json
# Note : On configure dans 'build/' mais on lie le JSON à la racine pour clangd
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Créer un lien symbolique pour clangd
ln -sf build/compile_commands.json compile_commands.json
```

**Ce que fait cette commande :**
- Télécharge GLFW, cglm, stb, etc. dans `build/_deps/`.
- Génère `build/compile_commands.json` avec des chemins absolus vers ces en-têtes.
- Le lie à la racine pour que l'éditeur le trouve.

### Étape 2 : Configuration VS Code
Si ce n'est pas déjà fait, acceptez la recommandation de l'espace de travail d'installer `clangd`.

1. Ouvrez le projet dans VS Code.
2. Si invité, **désactivez IntelliSense** pour l'extension Microsoft C/C++ (pour laisser `clangd` prendre le relais).
3. Ouvrez `src/main.c`. Vous devriez pouvoir faire Ctrl+Clic sur les fonctions comme `glfwInit` ou `app_run`.

---

## 4. Résolution de problèmes

### En-têtes introuvables (soulignements rouges)
Si `clangd` signale des en-têtes manquants malgré un build fonctionnel :

1. **Vérifiez `compile_commands.json`** : Assurez-vous que le lien symbolique existe à la racine du projet.
2. **Vérifiez les chemins d'inclusion** : Ouvrez `compile_commands.json` et cherchez `-I.../build/_deps/glfw-src/include`. Si absent, relancez `cmake -B build`.
3. **Rechargez la fenêtre** : VS Code met parfois en cache les anciens chemins. Exécutez `Ctrl+Shift+P` → `Developer: Reload Window`.
4. **Reconstruisez** :
    ```bash
    # Parfois, la génération des en-têtes (glad) nécessite un build
    make
    ```

### Chemins Distrobox vs hôte
Si vous construisez dans un conteneur mais éditez sur l'hôte :
- **Le correctif** : Notre `CMakeLists.txt` force les constructions locales des dépendances (`FetchContent`). Cela évite le problème où `pkg-config` pointe vers `/usr/include` à l'intérieur du conteneur (qui n'existe pas sur l'hôte).
- **Validation** : Assurez-vous de **NE PAS** utiliser les paquets GLFW dev installés sur le système pour la *configuration du build* si vous voulez que les complétions IDE fonctionnent parfaitement sur l'hôte.

---

## 5. Compatibilité Debian / Linux natif
Cette configuration est entièrement compatible avec Debian 13 ou toute autre distribution sans Distrobox.
- **Prérequis** : Vous devez avoir les en-têtes de développement X11 installés sur le système, car GLFW compile depuis les sources.
    ```bash
    sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
    ```
