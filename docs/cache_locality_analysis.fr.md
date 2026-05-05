# Localité Cache vs Découplage : Analyse du Layout Mémoire

*Mai 2026 — Phase 10 : Approfondissement Architectural V*

## Contexte

Durant la Phase 10 d'approfondissement architectural, nous avons audité le layout des structs centrales (`App`, `Scene`, `PostProcess`) pour comprendre les compromis entre découplage par pointeurs opaques et performance du cache CPU.

## Inventaire des Tailles de Structs

| Type | Taille | Lignes de Cache (64B) | Emplacement |
|------|--------|----------------------|-------------|
| `App` | 534 Ko | 8355 | Stack (main) |
| `Scene` | 133 Ko | 2080 | Inline dans App |
| `SceneVisuals` | 132 Ko | 2067 | Inline dans Scene |
| `TrailRenderer` | 128 Ko | 2058 | Inline dans SceneVisuals |
| `PostProcess` | 2.8 Ko | 44 | Inline dans App |
| `SceneLighting` | 488 o | 8 | Inline dans Scene |
| `SceneConfig` | 44 o | 1 | Inline dans Scene |
| `IcosphereGeometry` | 72 o | 1 | Inline dans Scene |
| `InstancedGroup` | 12 o | <1 | Inline dans Scene |
| `BillboardGroup` | 24 o | <1 | Inline dans Scene |

## Problème de Layout Mémoire

```
Scene (133 Ko = 2080 lignes de cache)
 offset    champ                taille      fréquence d'accès
───────────────────────────────────────────────────────────────
 +0        geometry             72 o        10/frame (surtout à l'init)
 +72       instanced_group      12 o        7/frame  (CHAUD: par draw)
 +84       billboard_group      24 o        12/frame (CHAUD: par draw)
 +112      billboard_sorter     96 o        4/frame
 +208      simulation*          8 o (ptr)   29/frame
 +216      gpu*                 8 o (ptr)   105/frame (CHAUD)
 +224      shaders*             8 o (ptr)   93/frame  (CHAUD)
 +232      ████ SceneVisuals ██████████████ 132 Ko de données FROIDES ██████
 +132576   lighting             488 o       (CHAUD: uniforms par draw)
 +133064   config               44 o        (CHAUD: vérifications par frame)
───────────────────────────────────────────────────────────────
```

### Le Problème

`SceneVisuals` (132 Ko) est coincé entre des champs chauds (`gpu*`, `shaders*` à +216/+224) et d'autres champs chauds (`lighting` à +132576, `config` à +133064). Cela crée un **gap de 2067 lignes de cache** entre des données fréquemment co-accédées.

Le cache L1 données fait typiquement 32–48 Ko. Le bloc froid `SceneVisuals` seul dépasse la capacité L1, ce qui signifie qu'accéder à `scene->lighting` après `scene->shaders` provoquera toujours un miss L1, même si les deux sont accédés à chaque frame dans chaque draw call.

### Cause Racine : TrailRenderer

```
SceneVisuals (132 Ko)
├── Skybox             20 o    (accédé ~2/frame)
├── TrailRenderer      128 Ko  (accédé ~3-5/frame, FROID)
│   └── rings[32]      32 × TrailRing
│       └── points[256] × vec3 + timestamps[256] × float = ~4 Ko/ring
└── ShockwaveRenderer  552 o   (accédé ~1/frame)
```

`TrailRenderer` stocke `32 × 256 × (vec3 + float) = 128 Ko` d'historique de positions inline. Ces données sont :
- Écrites ~60 fois/seconde (enregistrement d'échantillons)
- Lues 1 fois/frame pour l'upload GPU (rendu des trails)
- **Jamais** accédées dans une boucle CPU serrée en même temps que les données de rendu

## Pointeurs Opaques Existants : Évaluation de l'Impact Performance

Trois sous-structs ont déjà été déplacées vers des pointeurs opaques heap-alloués :

| Champ | Taille (estimée) | Accès/frame | Coût d'indirection |
|-------|-----------------|-------------|-------------------|
| `scene->simulation*` | ~4 Ko | 29 | ~120 ns/frame |
| `scene->gpu*` | ~2 Ko | 105 | ~420 ns/frame |
| `scene->shaders*` | ~8 Ko | 93 | ~372 ns/frame |

**Surcoût total des indirections : ~900 ns/frame** vs budget frame de **16 600 000 ns** (60 fps).

**Impact : 0.005% du budget frame** — totalement négligeable.

Ces champs sont accédés par draw-call (10–20 appels/frame), jamais par vertex ou par pixel. Le chemin chaud CPU est purement de l'orchestration ; le vrai travail se fait sur le GPU.

## Quand les Pointeurs Opaques Sont Sûrs

✅ **Sûr à opacifier** (impact perf négligeable) :
- Structs accédées < 200 fois/frame depuis le CPU
- Structs non accédées dans des boucles serrées (par particule, par vertex)
- Structs > 1 Ko qui poussent les voisins chauds hors du cache

❌ **NE PAS opacifier** :
- Petites structs (< 64 octets) déjà dans la même ligne de cache que les données chaudes
- Données accédées dans des boucles par particule (`NBodyParticle bodies[]`)
- Handles de buffers GPU utilisés dans les boucles de draw (garder inline pour le prefetch)

## Action Recommandée : Heap-Allouer SceneVisuals

Déplacer `SceneVisuals` (incluant `TrailRenderer`) vers un pointeur heap rapproche `lighting` et `config` à 1–2 lignes de cache de `gpu*` et `shaders*` :

```
Scene APRÈS (estimé ~900 octets, 14 lignes de cache)
 offset    champ                taille
───────────────────────────────────────
 +0        geometry             72 o
 +72       instanced_group      12 o
 +84       billboard_group      24 o
 +112      billboard_sorter     96 o
 +208      simulation*          8 o (ptr)
 +216      gpu*                 8 o (ptr)
 +224      shaders*             8 o (ptr)
 +232      visuals*             8 o (ptr) ← était 132 Ko inline
 +240      lighting             488 o
 +728      config               44 o
 +772      hdr_files            ...
───────────────────────────────────────
```

Maintenant `shaders*` (+224) et `lighting` (+240) sont **sur la même ligne de cache**. Un seul fetch L1 sert les deux.

## Autres Opportunités de Découplage Sûres

### Déclarations Anticipées (Coût Zéro)

Remplacer `#include` par `typedef struct X X;` quand seuls des pointeurs sont utilisés :

| Header | Include actuel | Peut être forward-déclaré |
|--------|---------------|--------------------------|
| `scene.h` | `#include "app_settings.h"` | Oui (utilise seulement l'enum `AAMode` — extraire dans un mini-header) |
| `app.h` | `#include "postprocess_internal.h"` | Non (PostProcess par valeur — mais devient Oui après opacification) |
| `app.h` | `#include "scene.h"` | Non (Scene par valeur) |

### Headers Interface (Pare-Feu de Compilation)

Créer des headers minimaux qui n'exposent que les signatures de fonctions + types opaques :

- `postprocess.h` existe déjà comme API publique
- `scene_internal.h` contrôle déjà l'accès à la struct
- **Manquant** : `scene_visuals.h` expose actuellement la struct complète → après opacification, seul le pointeur est nécessaire

### Réduction d'Includes dans postprocess_internal.h

`postprocess_internal.h` tire 16 headers dans 12 consommateurs. Après extraction des structs FX dans un `pp_effects_state.h` (inclus seulement par init/apply/cleanup), le header "internal" descend à ~8 includes.
