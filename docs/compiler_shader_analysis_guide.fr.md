# Guide d'Analyse des Compilateurs & Shaders (Juillet 2026)

**Date** : 3 juillet 2026
**Statut** : Guide de Référence

Ce guide fournit des instructions concrètes et étape par étape pour utiliser **Compiler Explorer (Godbolt.org)** et **NVIDIA Nsight Graphics (Shader Profiler)** afin d'auditer et d'optimiser l'exécution CPU et GPU dans la base de code `suckless-ogl`.

---

## 1. Compiler Explorer (Godbolt.org)

Compiler Explorer est un outil interactif permettant d'analyser le code assembleur généré par les compilateurs et de valider les optimisations (telles que l'auto-vectorisation AVX2, le padding des structures et le nombre d'instructions).

### Scénario A : Vérification de la Vectorisation AVX2/FMA (N-Body Physics)
Pour vérifier si la boucle critique des calculs de gravité N-Body est correctement vectorisée sans surcharge d'assemblage :

1. **Ouvrir Godbolt** : Allez sur [godbolt.org](https://godbolt.org).
2. **Configurer le Langage** : Définissez le menu déroulant du langage source à gauche sur **C**.
3. **Configurer le Compilateur** : Définissez le compilateur à droite sur **x86-64 gcc 14.2** (ou supérieur).
4. **Coller le Code** : Copiez et collez le snippet autonome de simulation N-Body suivant dans l'éditeur :
   ```c
   #include <math.h>

   typedef double dvec3[3];

   typedef struct {
       dvec3 position;
       dvec3 velocity;
       double mass;
       double radius;
   } Body;

   typedef struct {
       Body bodies[128];
       int body_count;
       float gravity;
   } NBodySim;

   static inline void dvec3_zero(dvec3 dest) {
       dest[0] = 0.0; dest[1] = 0.0; dest[2] = 0.0;
   }
   static inline void dvec3_sub(const dvec3 a, const dvec3 b, dvec3 dest) {
       dest[0] = a[0] - b[0]; dest[1] = a[1] - b[1]; dest[2] = a[2] - b[2];
   }
   static inline double dvec3_dot(const dvec3 a, const dvec3 b) {
       return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
   }
   static inline double dvec3_norm2(const dvec3 v) {
       return dvec3_dot(v, v);
   }
   static inline void dvec3_scale(const dvec3 v, double scalar, dvec3 dest) {
       dest[0] = v[0] * scalar; dest[1] = v[1] * scalar; dest[2] = v[2] * scalar;
   }
   static inline void dvec3_addto(dvec3 dest, const dvec3 b) {
       dest[0] += b[0]; dest[1] += b[1]; dest[2] += b[2];
   }
   static inline void dvec3_subfrom(dvec3 dest, const dvec3 b) {
       dest[0] -= b[0]; dest[1] -= b[1]; dest[2] -= b[2];
   }

   void compute_accelerations(const NBodySim* sim, double accel[][3]) {
       for (int i = 0; i < sim->body_count; i++) {
           dvec3_zero(accel[i]);
       }

       for (int i = 0; i < sim->body_count; i++) {
           for (int j = i + 1; j < sim->body_count; j++) {
               dvec3 diff;
               dvec3_sub(sim->bodies[j].position, sim->bodies[i].position, diff);

               double eps2 = 0.001; // pair softening
               double dist_sq = dvec3_norm2(diff) + eps2;
               double inv_dist = 1.0 / sqrt(dist_sq);
               double inv_dist3 = inv_dist * inv_dist * inv_dist;
               double grav = (double)sim->gravity * inv_dist3;

               dvec3 force;
               dvec3_scale(diff, grav * sim->bodies[j].mass, force);
               dvec3_addto(accel[i], force);

               dvec3_scale(diff, grav * sim->bodies[i].mass, force);
               dvec3_subfrom(accel[j], force);
           }
       }
   }
   ```
5. **Définir les Options de Build** :
   * Saisissez `-O3 -march=haswell -ffast-math` dans la zone de texte Compiler Options.
6. **Analyser l'Assembleur** :
   * Regardez le panneau d'assembleur généré. Vous devriez voir des instructions comme `vfmadd213pd` (Fused Multiply-Add) et des opérations packées `vmulpd`, `vaddpd` utilisant des registres 256 bits `%ymm`.
   * Essayez de changer les options pour `-O1` ou de supprimer `-ffast-math` pour voir comment le compilateur revient à des instructions mathématiques scalaires (`mulsd`, `addsd`).

---

### Scénario B : Vérification de l'Alignement des Structures (UBO / SSBO)
Lors de la copie de structures de C vers des Uniform Buffer Objects (UBO) ou des Shader Storage Buffer Objects (SSBO) OpenGL, la disposition mémoire doit correspondre aux normes d'alignement GLSL (`std140`/`std430`).

1. **Coller le Code** :
   ```c
   #include <stddef.h>

   typedef struct {
       float exposure;
       float gamma;
       int   contrast;
   } PostProcessUniforms;

   unsigned long get_struct_size() {
       return sizeof(PostProcessUniforms);
   }

   _Static_assert(sizeof(PostProcessUniforms) == 12, "Incorrect alignment!");
   ```
2. **Observer le Comportement du Compilateur** :
   * Sélectionnez **x86-64 gcc 14.2** (Linux) et **x64 msvc v19.latest** (Windows).
   * Vérifiez si les deux compilateurs passent l'assertion statique ou introduisent des octets de padding à la fin de la structure.
   * Modifiez les membres de la structure (par exemple, en ajoutant des équivalents `double` ou `vec4`) et inspectez l'assembleur pour voir la taille retournée dans `get_struct_size`.

---

### Scénario C : Validation des Shaders en Mode Hors-Ligne (GLSL/SPIR-V)
Pour vérifier si la syntaxe GLSL est correcte et inspecter l'assembleur SPIR-V :

1. **Configurer le Langage** : Définissez le menu déroulant du langage source à gauche sur **GLSL**.
2. **Configurer le Compilateur** : Sélectionnez **glslangValidator** (ou **dxc**).
3. **Coller le Compute Shader GLSL** :
   ```glsl
   #version 450
   layout(local_size_x = 256) in;

   layout(std430, binding = 0) buffer ParticleBuffer {
       vec4 positions[];
   };

   layout(std430, binding = 1) buffer VelocityBuffer {
       vec4 velocities[];
   };

   uniform float deltaTime;

   void main() {
       uint idx = gl_GlobalInvocationID.x;
       positions[idx] += velocities[idx] * deltaTime;
   }
   ```
4. **Inspecter SPIR-V** : Examinez les instructions assembleurs SPIR-V générées (telles que `OpAccessChain`, `OpFMul`, `OpFAdd`) pour contrôler l'organisation des variables et de la logique de calcul.

---

## 2. NVIDIA Nsight Graphics (Shader Profiler)

NVIDIA Nsight Graphics est un outil de profilage de GPU haut de gamme. Alors que Godbolt valide la compilation, Nsight Graphics mesure les goulots d'étranglement matériels réels sur le GPU.

### Comment Profiler des Shaders GLSL
1. **Préparer le Binaire Release** :
   * Compilez le projet en utilisant :
     ```bash
     cmake -B build-release -DCMAKE_BUILD_TYPE=Release
     cmake --build build-release --parallel
     ```
2. **Configurer Nsight Graphics** :
   * Ouvrez **NVIDIA Nsight Graphics**.
   * Définissez **Application Executable** sur le chemin de votre binaire compilé (`build-release/app`).
   * Définissez **Working Directory** sur la racine du projet.
   * Sous **Activity**, sélectionnez **GPU Trace** (pour les goulots d'étranglement matériels globaux) ou **Frame Debugger** (pour le débogage du rendu des frames).
   * Cliquez sur **Launch**.
3. **Capturer & Analyser** :
   * Exécutez la simulation et cliquez sur **Generate GPU Trace** (ou appuyez sur `F11`).
   * Une fois le tracé terminé, accédez à l'onglet **Shader Pipelines**.
   * Localisez le pipeline représentant votre compute shader (par exemple `nbody.comp`).
4. **Localiser les Hot Spots & Warp Stalls** :
   * Double-cliquez sur le compute shader pour ouvrir la vue **Shader Profiler**.
   * Allez dans l'onglet **Hot Spots**. Nsight Graphics superpose le code GLSL avec des compteurs d'exécution correspondants.
   * Inspectez la colonne **Top Stall** pour voir pourquoi le shader attend :
     * `TEXTHR` (Texture Throashing/Fetch) : Le shader est bloqué en attente d'opérations mémoire ou de lectures de cache de texture.
     * `NOTSEL` (Not Selected) : Les pipelines d'exécution sont occupés, en attente de ressources de planification du GPU.
     * `IMCMIS` (Instruction Cache Miss) : Le cache du code du shader a subi un échec, indiquant des structures de branchement complexes.
     * `MATHATH` (Math Latency) : Les pipelines ALU sont saturés par des calculs mathématiques complexes (FMA, transcendantes).

---

## 3. Alternatives Locales en Ligne de Commande

### Génération Assembleur via Just
Pour inspecter rapidement l'assembleur localement sans envoyer de code sur Godbolt, lancez :
```bash
just asm src/app.c
```
Cela génère `src/app.c.s` localement en mode Release avec les drapeaux d'architecture native mais la LTO désactivée.

### Entrelacement du Code Source (`objdump`)
Pour afficher votre code C côte à côte avec l'assembleur généré dans votre terminal local :
```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
objdump -S --demangle build/app | less
```
Cela affiche les instructions C d'origine entrelacées directement au-dessus de leurs blocs d'assembleur compilés.
