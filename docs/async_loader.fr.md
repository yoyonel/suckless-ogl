# Chargeur HDR asynchrone

Ce document décrit le système de chargement asynchrone des cartes d'environnement HDR.

## Vue d'ensemble

Le chargement d'une carte HDR et la génération des textures IBL associées (irradiance, spéculaire préfiltrée, BRDF LUT) peuvent prendre plusieurs centaines de millisecondes. Pour éviter tout gel de l'interface, ce processus est entièrement asynchrone.

## Architecture

Le chargement s'effectue en deux phases distinctes :

1. **Phase CPU** (thread asynchrone) — Lecture du fichier HDR depuis le disque, décodage, upload vers un PBO
2. **Phase GPU** (thread principal) — Téléversement de la texture depuis le PBO, génération IBL

## Protocole PBO

Les PBOs (Pixel Buffer Objects) servent de pont entre le thread de chargement et le thread de rendu :

```
Thread de chargement                Thread principal (rendu)
         │                                    │
   Lire HDR depuis disque                     │
         │                                    │
   Écrire dans PBO mappé                      │
         │                                    │
   Lever WAITING_FOR_PBO ──────────────────►  │
                                    Détecter l'état
                                              │
                                    glTexSubImage2D depuis PBO
                                              │
                                    Générer les mipmaps IBL
```

## Machine à états

Le chargeur utilise une machine à états pour coordonner les deux phases :

| État | Description |
|------|-------------|
| `IDLE` | Aucun chargement en cours |
| `LOADING` | Thread asynchrone en cours de lecture |
| `WAITING_FOR_PBO` | Lecture terminée, en attente de traitement GPU |
| `PROCESSING` | Génération IBL en cours |
| `DONE` | Texture prête pour le rendu |

## Intégration en 3 étapes

### Étape 1 — Lancer le chargement

```c
async_loader_request(&app->loader, "assets/environment.hdr");
```

### Étape 2 — Vérifier dans la boucle de rendu

```c
if (async_loader_poll(&app->loader) == ASYNC_LOADER_WAITING_FOR_PBO) {
    app_env_upload_from_pbo(&app->env, &app->loader);
}
```

### Étape 3 — Générer l'IBL

```c
if (app->env.texture_ready) {
    app_env_generate_ibl(&app->env);
}
```

## Gestion de la mémoire

Le PBO est alloué en persistant (`GL_MAP_PERSISTENT_BIT`) pour éviter les réallocations entre chargements. La taille est calculée pour accommoder la plus grande résolution HDR supportée (4K RGBE).

## Voir aussi

- [async_pbo.md](./async_pbo.md) — Détails techniques des PBOs persistants
- [env_transitions.md](./env_transitions.md) — Transitions entre environnements
- [ibl_architecture_ideas.md](./ibl_architecture_ideas.md) — Optimisations futures
