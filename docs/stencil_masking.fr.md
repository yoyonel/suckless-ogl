# Masquage par Stencil

Ce document décrit l'utilisation du tampon stencil pour séparer les objets de la skybox et les effets de post-traitement sélectifs.

## Rôle du tampon stencil

Le tampon stencil est utilisé pour deux objectifs :

1. **Séparation objet/skybox** : Marquer les pixels qui contiennent de la géométrie, pour que le rendu de la skybox saute ces fragments (voir aussi [skybox_rendering.md](./skybox_rendering.md))
2. **Masquage d'effets** : Appliquer certains effets de post-traitement uniquement sur certains objets

## Séparation objet/skybox

### Écriture du masque (passe géométrique)

```c
// Activer l'écriture stencil
glEnable(GL_STENCIL_TEST);
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); // Écrire sur la passe depth+color
glStencilFunc(GL_ALWAYS, 1, 0xFF);          // Toujours écrire la valeur 1
glStencilMask(0xFF);                        // Autoriser l'écriture complète

// Rendre la géométrie (sphères, etc.)
render_scene(app);
```

### Lecture du masque (passe skybox)

```c
// Ne pas écrire dans le stencil
glStencilMask(0x00);
// Passer uniquement là où le stencil vaut 0 (pas de géométrie)
glStencilFunc(GL_EQUAL, 0, 0xFF);

// Rendre la skybox
render_skybox(app);
```

## Masquage d'effets post-traitement

Pour appliquer un effet (ex. contour lumineux) uniquement sur certains objets :

```c
// Passe 1 : Marquer les objets sélectionnés avec la valeur stencil 2
glStencilFunc(GL_ALWAYS, 2, 0xFF);
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
render_selected_objects(app);

// Passe 2 : Appliquer le contour uniquement sur ces objets
glStencilFunc(GL_EQUAL, 2, 0xFF);
glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
render_outline_effect(app);
```

## Débogage du tampon stencil

Pour visualiser le contenu du tampon stencil :

```c
// Visualisation via shader de débogage
if (app->debug_stencil) {
    // Lire et afficher le tampon stencil comme texture
    glReadPixels(0, 0, width, height,
                 GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                 stencil_debug_buffer);
}
```

Raccourci : `SHIFT+S` pour basculer la visualisation stencil.

## Restauration de l'état

Important : Toujours restaurer l'état stencil après utilisation pour ne pas affecter les passes suivantes :

```c
// Restaurer l'état par défaut
glDisable(GL_STENCIL_TEST);
glStencilMask(0xFF);
glStencilFunc(GL_ALWAYS, 0, 0xFF);
glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
```

## Voir aussi

- [skybox_rendering.md](./skybox_rendering.md) — Optimisation Early-Z avec stencil
- [debugging.md](./debugging.md) — Outils de débogage GL
