# Documentation : Profilage CPU et Multithread avec Intel VTune Profiler

> **Date de mise à jour :** 2026-06-21 16:00:00 CEST
> **Contexte :** Moteur de rendu OpenGL 4.x (suckless-ogl), C11, processeur hybride Intel (13th Gen Raptor Lake-P), Debian GNU/Linux 13.

## 1. Prérequis et Installation (Debian / Linux)

Intel VTune Profiler est l'outil de référence pour analyser l'utilisation de la microarchitecture, les contentions de threads et les défauts de cache.

### 1.1. Installation du paquet natif
Sur Debian/Ubuntu, VTune s'installe via le dépôt officiel Intel oneAPI :
```bash
wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] [https://apt.repos.intel.com/oneapi](https://apt.repos.intel.com/oneapi) all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list
sudo apt update
sudo apt install intel-oneapi-vtune
```

### 1.2. Configuration du Noyau (Critique)
Pour permettre à VTune d'accéder aux compteurs matériels (PMU) sans droits `root`, le noyau Linux de l'hôte doit être configuré :
```bash
sudo sysctl -w kernel.perf_event_paranoid=0
sudo sysctl -w kernel.kptr_restrict=0
sudo sysctl -w kernel.nmi_watchdog=0
```

### 1.3. Initialisation de l'environnement
Pour rendre la commande `vtune-gui` disponible dans le terminal :
```bash
source /opt/intel/oneapi/setvars.sh
```

---

## 2. Configuration du Projet (CMake)

Pour que VTune puisse résoudre les symboles et reconstruire l'arbre d'appels (*Call Stack*), le binaire doit être compilé avec des options spécifiques.

```cmake
# CMakeLists.txt
# Requis : Symboles de débogage (-g) et préservation des pointeurs de frame
target_compile_options(app PRIVATE -g -O2 -fno-omit-frame-pointer)
```

**Vérification de la validité du binaire :**
1. Présence des symboles : `readelf -S build/app | grep debug` (doit retourner `.debug_info`).
2. Pointeur de frame conservé : `objdump -d build/app | grep -A 2 "<main>:"` (doit montrer `push %rbp` et non pas directement un `sub %rsp`).

---

## 3. Configuration d'une Analyse VTune

Si l'application se coupe immédiatement (ex: *Elapsed Time: 0.1s*), c'est généralement un problème de résolution de chemins relatifs.
* Dans **Analysis Configuration** > **WHAT**, définissez impérativement le **Working directory** sur la racine du projet (là où se trouvent les dossiers `assets/` et `shaders/`).

### 3.1. Filtrer le "Bruit" des bibliothèques tierces
L'écosystème Linux (Mesa, X11, glibc) génère beaucoup de trafic CPU qui masque le code métier. Pour obtenir une vue claire dans l'onglet **Bottom-up** :
1. **Filtre Module :** Dans la barre inférieure, utilisez le menu déroulant `Module` et sélectionnez uniquement votre exécutable (ex: `app`).
2. **Call Stack Mode :** Dans cette même barre, réglez l'affichage sur **Only user functions**. Les appels bas niveau (`malloc`, `pthread_mutex_lock`) seront alors repliés dans la fonction C11 appelante.

---

## 4. Méthodologie : Profilage Multithread (Threading)

**Objectif :** Identifier les interblocages (Deadlocks) et les temps d'attente (Stalls) liés aux Mutex et Variables de Condition.
**Type d'analyse :** *Parallelism > Threading*

* **Top Waiting Objects (Summary) :** Liste les objets de synchronisation bloquants. Les variables de condition (`Condition Variable`) avec un temps élevé mais un `Wait Count` faible sont normales (threads workers en sommeil). Un mutex avec un `Wait Count` très élevé (milliers) indique une forte contention.
* **Timeline (Platform) :** Permet de visualiser la chorégraphie des threads.
  * Les blocs marrons indiquent le temps CPU actif.
  * Les blocs verts clairs indiquent une attente (Wait/Sleep).
  * *Critère de bonne santé :* Un worker asynchrone doit présenter de larges blocs continus lors du traitement d'une tâche, sans fragmentation excessive due au thread principal.

---

## 5. Méthodologie : Performance Pure (Hotspots)

**Objectif :** Trouver les goulots d'étranglement algorithmiques et les défauts d'accès mémoire.
**Type d'analyse :** *Algorithm > Hotspots* (Mode : **Hardware Event-Based Sampling** recommandé pour une précision maximale avec 0% d'overhead).

* **Microarchitecture Usage :** Un drapeau rouge ici indique des *Pipeline Stalls* (le CPU attend la RAM ou a fait une mauvaise prédiction de branchement).
* **Instructions Retired :** Donne l'ordre de grandeur du volume de calcul exigé par une fonction.

---

## 6. Cas Pratique d'Optimisation : Génération d'UI (suckless-ogl)

### Le Problème Identifié
Lors du rendu IMGUI du texte, l'analyse Hotspots a révélé une anomalie majeure :
* `ui_draw_text_scaled` et `make_glyph_quad` consommaient **~150 ms** par frame.
* La *Microarchitecture Usage* affichait un stall de 36.7%.
* L'origine : L'initialisation sur la pile d'une structure lourde (`UIQuad`, 288 octets) suivie d'un retour par valeur (`return quad;`) et d'une boucle de copie.

### La Solution : Écriture In-Place (Data-Oriented)
La suppression de la structure intermédiaire et de la copie a été réalisée en remplaçant la logique par une fonction `static inline` qui écrit directement dans le buffer de destination (VBO batch).

**Avant (Lent, copies mémoire) :**
```c
UIQuad quad = { .vertices = { ... } };
return quad;
// Suivi d'une boucle for pour copier les 6 vertices dans le batch...
```

**Après (Rapide, zéro copie) :**
```c
static inline void push_batch_quad(
    UIVertex* out_vert,
    float left, float right, float top, float bottom,
    float tex_u0, float tex_v0, float tex_u1, float tex_v1,
    float col_r, float col_g, float col_b, float col_a,
    float mode, float param_w, float param_h, float radius)
{
    // Écriture directe des 6 sommets à l'adresse mémoire finale
    out_vert[0] = (UIVertex){left, bottom, tex_u0, tex_v1, col_r, col_g, col_b, col_a, mode, param_w, param_h, radius};
    // ...
}
```

### Résultats de l'Optimisation
* Temps d'exécution de la génération du texte divisé par 2.5 (chute à **~60 ms**).
* *Microarchitecture Usage* de la fonction d'écriture passée à **100.0%** (Pipeline CPU parfait, aucun défaut de cache).
* Éradication de plus de 100 millions d'instructions de copie inutiles par frame.
