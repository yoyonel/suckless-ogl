# Bilan d'Audit Qualité - yoyonel/suckless-ogl

## Résumé
L'audit a révélé **5 familles de patterns** récurrents qui génèrent du bruit visuel et alourdissent la charge cognitive de la base de code, principalement liés à l'usage d'OpenGL et à la gestion du préprocesseur.

## Top des Répétitions

### 1. Boilerplate de Génération et Bind des Buffers/VertexArrays (VAO/VBO)
- **Description** : Les séquences d'appels à `glGenVertexArrays`, `glBindVertexArray`, `glGenBuffers`, `glBindBuffer` sont omniprésentes dans presque tous les modules de rendu. Ces appels de bas niveau masquent l'intention de la création de la géométrie ou des buffers.
- **Avant (Extrait)** :
  ```c
  glGenVertexArrays(1, &group->vao);
  glBindVertexArray(group->vao);
  glGenBuffers(1, &group->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, group->vbo);
  // ... configuration des attributs ...
  glBindVertexArray(0);
  ```
- **Occurrences** : ~20+ pour les VAO, ~20+ pour les VBO/SSBO.
- **Fichiers concernés** : `billboard_rendering.c`, `ssbo_rendering.c`, `instanced_rendering.c`, `light_probes.c`, `scene.c`, `ui.c`, `render_utils.c`, `postprocess.c`, `sphere_sorting.c`
- **Suggestion d'abstraction** : Créer une API de création de mesh de plus haut niveau, par exemple `mesh_create_vao_vbo(Mesh* mesh, const float* vertices, size_t size)` ou encapsuler la création de buffers dans `gl_utils_create_buffer(GLenum target, size_t size, const void* data)`.

### 2. Boilerplate de Génération des Textures (et Framebuffers)
- **Description** : La création de textures et de framebuffers (FBO) implique souvent de multiples lignes répétitives (`glGenTextures`, `glBindTexture`, `glTexParameteri`, etc.), parfois encapsulée dans les effets mais pas toujours uniformisée.
- **Avant (Extrait)** :
  ```c
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  // ... glTexImage2D, glTexParameteri ...
  ```
- **Occurrences** : ~10+ (`glGenTextures`), ~5+ (`glGenFramebuffers`)
- **Fichiers concernés** : `texture.c`, `light_probes.c`, `ui.c`, `render_utils.c`, `postprocess.c`, effets divers.
- **Suggestion d'abstraction** : Pousser l'utilisation du `fx_utils_create_texture` (déjà introduit pour certains effets) à l'ensemble du projet, ou créer une structure de configuration `TextureDesc` pour abstraire `texture_create(const TextureDesc* desc)`.

### 3. Récupération des Uniforms (`glGetUniformLocation`)
- **Description** : La récupération manuelle des locations d'uniforms avec `glGetUniformLocation` est dupliquée, même si certaines abstractions (`PBRSpecUniforms`) existent déjà. La recherche des chaînes de caractères allonge le code d'initialisation.
- **Avant (Extrait)** :
  ```c
  sorter->loc_stage = glGetUniformLocation(sorter->compute_program, "u_stage");
  sorter->loc_count = glGetUniformLocation(sorter->compute_program, "u_count");
  sorter->loc_cam = glGetUniformLocation(sorter->compute_program, "u_cam_pos");
  ```
- **Occurrences** : ~20+
- **Fichiers concernés** : `shader.c`, `sphere_sorting.c`, `pbr.c`
- **Suggestion d'abstraction** : Définir un tableau ou un dictionnaire des uniforms attendus lors de la compilation du shader, avec un système d'introspection automatique, ou consolider tous les uniform "communs" via des UBOs (Uniform Buffer Objects) ce qui éliminerait le besoin d'interroger la majorité des locations.

### 4. Bruit Conditionnel du Préprocesseur (`#ifdef`)
- **Description** : Le code métier (particulièrement dans `scene.c` et `app.c`) est fortement haché par des macros de compilation conditionnelle telles que `#ifdef USE_SSBO_RENDERING` ou `#ifdef USE_TRANSPARENT_BILLBOARDS`. Cela rend la lecture du flux logique très difficile, nécessitant de suivre virtuellement plusieurs chemins d'exécution.
- **Avant (Extrait)** :
  ```c
  #ifdef USE_SSBO_RENDERING
      current_shader = scene->pbr_ssbo_shader;
  #else
      current_shader = scene->pbr_instanced_shader;
  #endif
  ```
- **Occurrences** : ~50+
- **Fichiers concernés** : `scene.c`, `app.c`, `shader.c`, `perf_mode.c`
- **Suggestion d'abstraction** : Remplacer l'usage massif de macros de compilation par le polymorphisme (interfaces de rendu via des pointeurs de fonction, ou des "Drivers" de rendu). Déplacer les conditions au niveau des options de CMake pour compiler des fichiers `.c` différents selon la plateforme/configuration, plutôt que d'entrelacer la logique avec des `#ifdef`.

### 5. Formatage Redondant des Chaînes de Caractères (`safe_snprintf`)
- **Description** : Il y a un grand nombre d'appels à `safe_snprintf` dédiés exclusivement à la concaténation de chemins ou au formatage basique de texte de débogage/UI.
- **Avant (Extrait)** :
  ```c
  (void)safe_snprintf(buf, sizeof(buf), "Debug: %s", mode);
  (void)safe_snprintf(buf, sizeof(buf), "Exposure: %.2f", val);
  ```
- **Occurrences** : ~50+
- **Fichiers concernés** : `app_input.c`, `app_ui.c`, `postprocess_input.c`, `async_loader.c`, etc.
- **Suggestion d'abstraction** : Pour la construction des éléments d'UI, on pourrait introduire des fonctions spécialisées `ui_text_fmt(...)` ou `ui_value_slider_fmt(...)` qui masquent le buffer temporaire et l'appel à `safe_snprintf`. Pour les chemins de fichiers, introduire une fonction utilitaire `path_join(dir, file)`.
