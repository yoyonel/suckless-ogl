# Rapport d'Analyse Qualité (suckless-ogl) - Branche master

## Résumé
L'audit de la base de code a mis en évidence **5 familles de patterns** répétitifs (boilerplate) qui alourdissent la charge cognitive et augmentent le bruit visuel dans la base de code, principalement liés à l'utilisation directe de l'API OpenGL, de macros conditionnelles et de la création de notifications utilisateur.

## Top des Répétitions

### 1. Boilerplate de création et configuration de buffers OpenGL
- **Description** : La création de buffers (`glGenBuffers`), leur liaison (`glBindBuffer`) et l'allocation de données (`glBufferData`) sont systématiquement dupliquées manuellement à travers les modules de rendu. Cela crée un couplage fort et une répétition verbeuse de concepts bas niveau.
- **Extrait "Avant"** :
  ```c
  glGenBuffers(1, &group->instance_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               (GLsizeiptr)(count * sizeof(SphereInstance)), data,
               GL_STATIC_DRAW);
  ```
- **Occurrences estimées** : ~76 occurrences de `glBindBuffer`, ~25 de `glBufferData`, et ~22 de `glGenBuffers`, réparties dans environ 13 fichiers (`src/billboard_rendering.c`, `src/instanced_rendering.c`, `src/light_probes.c`, `src/render_utils.c`, etc.).
- **Suggestion d'abstraction** : Créer une fonction utilitaire ou un constructeur de buffer du type `render_utils_create_buffer(GLenum target, GLsizeiptr size, const void* data, GLenum usage, GLuint* out_buffer)` pour condenser ces trois étapes en un seul appel clair.

### 2. Boilerplate de configuration des Vertex Array Objects (VAO)
- **Description** : L'initialisation des VAO (`glGenVertexArrays`) et la déclaration des attributs de sommets (`glEnableVertexAttribArray`, `glVertexAttribPointer`) génèrent un bloc de code dense et très répétitif à chaque nouveau type de géométrie, augmentant le bruit visuel.
- **Extrait "Avant"** :
  ```c
  glGenVertexArrays(1, vao);
  glBindVertexArray(*vao);
  glBindBuffer(GL_ARRAY_BUFFER, geometry_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
  ```
- **Occurrences estimées** : ~19 occurrences pour `glEnableVertexAttribArray` et `glVertexAttribPointer`, ainsi que ~9 appels à `glGenVertexArrays`, identifiés dans 7 fichiers (`src/billboard_rendering.c`, `src/ssbo_rendering.c`, `src/render_utils.c`, `src/ui.c`, etc.).
- **Suggestion d'abstraction** : Introduire une définition déclarative des attributs de vertex (une structure `VertexLayout`) et implémenter une fonction factorisée comme `render_utils_setup_vao(GLuint* vao, GLuint vbo, const VertexLayout* layout)`.

### 3. Bruit visuel lié à la récupération des locations d'uniformes
- **Description** : De multiples fonctions enchaînent des appels individuels à `glGetUniformLocation` sans abstraction, créant de longs blocs de code fastidieux à lire.
- **Extrait "Avant"** :
  ```c
  out->u_env_map = glGetUniformLocation(shader, "envMap");
  out->u_roughness = glGetUniformLocation(shader, "roughnessValue");
  out->u_mip = glGetUniformLocation(shader, "currentMipLevel");
  ```
- **Occurrences estimées** : ~19 occurrences détectées, particulièrement concentrées dans `src/pbr.c` et `src/sphere_sorting.c`.
- **Suggestion d'abstraction** : Définir un tableau de constantes textuelles et initialiser les uniformes en boucle via une fonction comme `shader_init_uniforms(GLuint program, const char** names, GLint* locations, size_t count)`.

### 4. Formatage manuel redondant pour les notifications UI
- **Description** : La logique métier d'entrée utilisateur est fortement couplée à la création de messages via un buffer local, formaté avec `safe_snprintf`, puis poussé manuellement dans l'interface utilisateur.
- **Extrait "Avant"** :
  ```c
  char buf[NOTIF_BUF_SIZE];
  (void)safe_snprintf(buf, sizeof(buf), "Debug: %s", modeNames[app->scene.pbr_debug_mode]);
  action_notifier_push(&app->notifier, buf, NOTIF_DUR_LONG);
  ```
- **Occurrences estimées** : ~44 appels à `action_notifier_push` (souvent consécutifs à l'un des ~54 appels à `safe_snprintf`), observés dans `src/app_input.c` et `src/postprocess_input.c`.
- **Suggestion d'abstraction** : Créer une macro ou une fonction utilitaire variadique telle que `action_notifier_pushf(notifier, duration, format, ...)` qui encapsule l'allocation temporaire du buffer et le formatage de la chaîne de caractères.

### 5. Blocs conditionnels (Macros) hachant la logique métier
- **Description** : L'utilisation de directives de préprocesseur (`#ifdef`, `#ifndef`) pour isoler des fonctionnalités de rendu, d'optimisation ou de profilage s'accumule et hache la lisibilité du flux de contrôle dans la logique métier principale.
- **Extrait "Avant"** :
  ```c
  #ifdef USE_SSBO_RENDERING
      // Logique SSBO
  #else
      // Logique Instanciée
  #endif
  ```
- **Occurrences estimées** : Identifiées dans plusieurs fichiers clés avec un nombre élevé de macros de branchement, comme `src/scene.c` (~10 blocs), `src/perf_mode.c` (~9 blocs) et `src/app.c`.
- **Suggestion d'abstraction** : Remplacer ces directives de compilation conditionnelle éparpillées par des interfaces/structures polymorphes (ex: une interface de rendu `RendererStrategy` qui encapsule la divergence SSBO vs Instanced derrière un appel uniforme).
