# Rendu de la skybox

Ce document décrit l'implémentation et les optimisations du rendu de la skybox equirectangulaire.

## Projection equirectangulaire

La skybox utilise une texture equirectangulaire (panoramique 360°) plutôt qu'un cubemap. La conversion direction→UV est :

```glsl
vec2 dir_to_eq_uv(vec3 dir) {
    float phi = atan(dir.z, dir.x);         // Azimuth [-π, π]
    float theta = asin(clamp(dir.y, -1.0, 1.0)); // Élévation [-π/2, π/2]
    return vec2(
        phi / (2.0 * PI) + 0.5,             // U : [0, 1]
        theta / PI + 0.5                     // V : [0, 1]
    );
}
```

## Optimisation Early-Z

La skybox est rendée **après la passe géométrique principale** avec le test de profondeur `GL_LEQUAL` :

```c
// Passe géométrique : les sphères PBR remplissent le depth buffer
render_scene(app);

// Skybox : rendue après, passe uniquement là où il n'y a pas de géométrie
glDepthFunc(GL_LEQUAL);  // ← Passe si depth == 1.0 (arrière-plan)
render_skybox(app);
glDepthFunc(GL_LESS);    // Restaurer pour la prochaine frame
```

Avantage : Seuls les pixels de l'arrière-plan exécutent le shader skybox, économisant ~60-80 % des fragments sur les scènes typiques.

## Suppression de la translation dans la matrice de vue

Pour créer l'illusion que la skybox est à l'infini, la composante de translation est retirée de la matrice de vue :

```c
// Retirer la translation (4e colonne de la matrice 4×4)
mat4 view_no_translation = app->camera.view;
view_no_translation.col[3] = (vec4){{0.0f, 0.0f, 0.0f, 1.0f}};

glUniformMatrix4fv(u_view, 1, GL_FALSE, (float*)&view_no_translation);
```

Sans cette suppression, la skybox se déplacerait avec la caméra, brisant l'illusion de profondeur infinie.

## Vertex shader minimal

Le vertex shader de la skybox utilise un triangle plein écran (une seule primitive, zéro VBO) :

```glsl
// Triangle plein écran — génèré procéduralement depuis gl_VertexID
vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0); // depth = 1.0 (arrière-plan)
```

`gl_Position.z = 1.0` garantit que le skybox se situe au fond du frustum, passant le test `GL_LEQUAL` uniquement là où aucune géométrie n'est présente.

## Niveaux de mipmap

La skybox utilise le niveau de mip 0 pour la plus haute résolution. Pour les reflections dans le IBL, des niveaux de mip inférieurs sont utilisés (voir [progressive_ibl.md](./progressive_ibl.md)).

## Voir aussi

- [equirectangular_seam_fix.md](./equirectangular_seam_fix.md) — Correction des coutures
- [cubemap_seam_resolution.md](./cubemap_seam_resolution.md) — Problématique des coutures
- [progressive_ibl.md](./progressive_ibl.md) — Cartes IBL générées depuis la skybox
