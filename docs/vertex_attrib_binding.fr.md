# Liaison Moderne des Attributs de Sommets (Vertex Attrib Binding)

Ce document décrit la modernisation du pipeline de spécification et de liaison des attributs de sommets dans `suckless-ogl`, passant de l'ancienne API `glVertexAttribPointer` à l'API moderne d'OpenGL 4.3+ **Vertex Attrib Binding** (`glVertexAttribFormat`, `glVertexAttribBinding`, `glBindVertexBuffer`).

## 1. Contexte et Motivation

Auparavant, le moteur s'appuyait sur le modèle classique de spécification des sommets d'OpenGL, où la configuration des attributs (taille, type, pas, décalage) était couplée à la liaison du tampon (VBO) via `glVertexAttribPointer`. Cette approche présentait plusieurs inconvénients :
- **Conversion d'Entiers en Pointeurs** : Les décalages (offsets) devaient être passés sous forme de `const void*`, nécessitant des macros comme `utils_buffer_offset` (qui convertissaient en interne des entiers en `void*`). Ce modèle déclenchait des avertissements stricts du compilateur et des outils de l'analyse statique (comme `performance-no-int-to-ptr` de clang-tidy).
- **Couplage Fort** : Tout changement de tampon nécessitait de spécifier à nouveau le format des attributs, ce qui augmentait le nombre d'appels au pilote OpenGL et le coût de validation des états.

### L'Approche Moderne d'OpenGL 4.3+

L'API de *Vertex Attrib Binding* découple le **format** des attributs de sommets de leurs **tampons sources (VBOs)** :
1. **Spécification du Format** : `glVertexAttribFormat` définit la structure de l'attribut (taille, type, décalage relatif au sein de la structure) et l'associe à un emplacement d'attribut logique (slot).
2. **Liaison Logique** : `glVertexAttribBinding` relie cet emplacement d'attribut à un **point de liaison** (binding point) logique.
3. **Liaison du Tampon** : `glBindVertexBuffer` associe un tampon physique de la carte graphique (VBO) à un point de liaison logique, en indiquant le décalage de départ et le pas (stride).

```mermaid
graph TD
    classDef buffer fill:#1a1b26,stroke:#7aa2f7,color:#7aa2f7,stroke-width:2
    classDef slot fill:#1a1b26,stroke:#bb9af7,color:#bb9af7
    classDef bindpoint fill:#1a1b26,stroke:#e0af68,color:#e0af68

    VBO_Geom[VBO Géométrie]:::buffer
    VBO_Inst[VBO Instance]:::buffer

    BP_0[Point de liaison 0<br/>Pas: 3*sizeof(float)]:::bindpoint
    BP_1[Point de liaison 1<br/>Pas: sizeof(SphereInstance)]:::bindpoint

    Attr_0[Attribut 0 : Position]:::slot
    Attr_1[Attribut 1 : Normales]:::slot
    Attr_2[Attribut 2 : Albedo]:::slot
    Attr_3[Attribut 3 : Métallique]:::slot

    VBO_Geom -->|glBindVertexBuffer| BP_0
    VBO_Inst -->|glBindVertexBuffer| BP_1

    Attr_0 -->|glVertexAttribBinding| BP_0
    Attr_1 -->|glVertexAttribBinding| BP_0
    Attr_2 -->|glVertexAttribBinding| BP_1
    Attr_3 -->|glVertexAttribBinding| BP_1
```

## 2. Détails du Refactoring

Le refactoring a impacté les composants suivants :

### A. Suppression de `utils_buffer_offset`
La fonction utilitaire `utils_buffer_offset` a été complètement retirée de `include/utils.h` et `src/utils.c`. La spécification des décalages utilise désormais directement des valeurs `GLuint` simples, éliminant les casts en `void*`.

### B. Standardisation des Points de Liaison
Le moteur adopte une convention claire pour les points de liaison au sein des VAO :
- **Points de liaison 0 & 1** : Données géométriques des sommets (positions, normales, coordonnées de texture).
- **Point de liaison 2** : Données d'instance (paramètres PBR des sphères, matrices d'instance, etc.).

### C. Modules Refactorisés

- **Utilitaires de Rendu (`src/render_utils.c`)** :
  - `render_utils_create_fullscreen_quad` configure l'attribut de position avec `glVertexAttribFormat` et lie le VBO du quad via `glBindVertexBuffer` sur le point 0.
  - `render_utils_setup_sphere_instance_attributes` prend maintenant un paramètre `GLuint binding_point` et configure les attributs d'instance des sphères sur ce point de liaison logique.
- **Rendu Instancié (`src/instanced_rendering.c`)** :
  - Refactorisation de `instanced_group_bind_mesh` pour lier la géométrie du mesh (positions sur le point 0, normales sur le point 1) et les données d'instance (matrices/couleurs) sur le point 2.
- **Rendu Standard & SSBO (`src/scene_render.c`, `src/ssbo_rendering.c`)** :
  - Refactorisation de `scene_update_gpu_buffers` (VAO icosphère) et `ssbo_group_bind_mesh` (VAO géométrie SSBO) pour utiliser le modèle de liaison moderne découplé.
- **Interface Utilisateur (`src/ui.c`)** :
  - `setup_vertex_buffers` mappe le flux de sommets d'ImGui (positions, uv, couleurs) avec `glVertexAttribFormat` sur le point de liaison 0.
- **Rendu de Traînées (`src/trail_renderer.c`)** :
  - Configuration des positions et couleurs des traînées sur le point de liaison 0.
- **Rendu de Billboards & Shockwaves (`src/billboard_rendering.c`, `src/shockwave.c`)** :
  - Configuration de la géométrie du quad sur les points 0 et 1, et des données d'instance sur le point 2.
  - Refactorisation de `shockwave_renderer_init` pour le quad de billboard de l'onde de choc.
- **Débogage & Visualisation (`src/light_probes.c`, `src/effects/fx_lut_viz.c`)** :
  - Refactorisation de `light_probe_grid_init` (VAO de la boîte englobante wireframe d'aide au débogage) pour utiliser VAB.
  - Refactorisation de `fx_lut_viz_init` (VAO de la grille de points pour la LUT 3D) pour utiliser VAB.

## 3. Avantages

1. **Conformité aux Lints Stricts** : Élimine la conversion d'entiers en pointeurs, résolvant tous les avertissements de type `performance-no-int-to-ptr`.
2. **Performances** : La liaison des tampons indépendamment des formats réduit le coût de validation des états par le pilote GPU.
3. **Clarity** : Séparation nette de la structure des données (format) et de l'emplacement physique en mémoire (tampons).
