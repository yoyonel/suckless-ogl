# Effet de Lensing Shockwave

## Vue d'ensemble

Lorsqu'une particule N-body franchit le rayon de confinement, une **onde de choc lensing sur billboard** s'étend depuis le point d'impact. L'effet est rendu sous forme d'un quad face-caméra qui échantillonne la scène derrière lui et applique :

1. **Déplacement UV radial** — les pixels sont repoussés vers l'extérieur depuis le centre de l'anneau
2. **Aberration chromatique** — les canaux R, G, B sont échantillonnés à des décalages différents (1.0×, 1.25×, 1.5×)
3. **Glow HDR additif** — le bord de l'anneau émet de la lumière colorée qui alimente le bloom

## Architecture

```mermaid
graph TD
    A[Géométrie de scène] --> B[Scene FBO - RGBA16F]
    B --> C{Shockwaves actives ?}
    C -->|Non| E[Post-Processing]
    C -->|Oui| D[Grab Pass : glCopyTexSubImage2D]
    D --> F[Draw Billboard]
    F --> G[Fragment Shader : échantillonne grab_tex + distorsion]
    G --> B
    B --> E
```

### Flux de rendu

1. La **géométrie de scène** est rendue dans le FBO HDR (`scene_color_tex`, RGBA16F)
2. **Grab pass** : `glCopyTexSubImage2D` copie `scene_color_tex` → `grab_tex` (allocation paresseuse, même format/taille)
3. **Draw billboard** : pour chaque shockwave active, un quad face-caméra est rastérisé
4. **Fragment shader** : échantillonne `grab_tex` aux UV écran décalés avec aberration chromatique par canal
5. Le **post-processing** lit le `scene_color_tex` modifié normalement

### Fichiers clés

| Fichier | Rôle |
|---------|------|
| `include/shockwave.h` | Struct `ShockwaveRenderer`, constantes, API |
| `src/shockwave.c` | Grab pass, logique emit/update/draw |
| `shaders/shockwave.vert` | Vertex shader billboard, sortie `vScreenUV` |
| `shaders/shockwave.frag` | Déplacement + aberration chromatique + glow |
| `src/scene.c` | Site d'appel avec profiler GPU + support wireframe |

## Analyse du coût du Grab Pass

Le grab pass utilise `glCopyTexSubImage2D` — une copie **GPU vers GPU** (pas un readback CPU). Le coût est limité par la bande passante VRAM uniquement :

| Résolution | Format | Taille/frame | Bande passante @ 60 fps | % d'un GPU 200 Go/s |
|---|---|---|---|---|
| 1920×1080 | RGBA16F (8 o/px) | ~16 Mo | ~960 Mo/s | **0.5%** |
| 2560×1440 | RGBA16F | ~29 Mo | ~1.7 Go/s | **0.9%** |
| 3840×2160 | RGBA16F | ~66 Mo | ~3.9 Go/s | **2.0%** |

**Propriétés clés :**

- Aucun stall CPU, aucune synchronisation de pipeline
- Coût = 0 quand aucune shockwave n'est active (early return avant la copie)
- Allocation paresseuse : `grab_tex` n'est créée qu'au premier usage et redimensionnée au changement de résolution
- Alternative : `glCopyImageSubData` (GL 4.3) ou `glBlitFramebuffer` pourraient être marginalement plus rapides mais la différence est imperceptible

## Design des Shaders

### Vertex Shader (`shockwave.vert`)

Le billboard est un quad unitaire `[-1,1]` mis à l'échelle par `u_radius` et orienté face à la caméra via des vecteurs de base par produit vectoriel. Deux sorties :

- `vUV` — coordonnées locales du quad `[-1,1]` pour le profil de l'anneau
- `vScreenUV` — coordonnées écran après division perspective `[0,1]` pour échantillonner la scène

### Fragment Shader (`shockwave.frag`)

```text
Profil de l'anneau : Gaussienne centrée sur le front d'onde en expansion
                     ring_radius = 0.3 + 0.6 * progress
                     ring = exp(-8 * (dist - ring_radius)²)

Enveloppe temporelle : sin(π * progress)  →  fondu entrant/sortant lisse

Déplacement :    factor = 0.06 * intensity * ring * envelope
                 offset = factor * direction_radiale

Aberration chromatique :
                 R = sample(grab_tex, screenUV + offset * 1.0)
                 G = sample(grab_tex, screenUV + offset * 1.25)
                 B = sample(grab_tex, screenUV + offset * 1.5)

Glow additif :   couleur * 3.0 * ring * envelope * intensity * 0.25
```

### Constantes

| Constante | Valeur | Description |
|---|---|---|
| `DISTORT_STRENGTH` | 0.06 | Déplacement UV maximum au pic |
| `RING_SHARPNESS` | 8.0 | Largeur de la Gaussienne (plus = anneau plus fin) |
| `CA_SPREAD_R/G/B` | 1.0 / 1.25 / 1.5 | Multiplicateurs de déplacement par canal |
| `HDR_GLOW_SCALE` | 3.0 | Intensité du glow émissif |
| `GLOW_MIX` | 0.25 | Contribution du glow vs distorsion pure |

## Filtrage des émissions

Chaque franchissement de frontière ne produit pas forcément une shockwave visible. Trois filtres s'appliquent :

1. **Seuil de vitesse** (`SHOCKWAVE_MIN_VELOCITY = 0.05`) : seule la composante de vitesse radiale sortante est mesurée (`v_out = dot(velocity, radial_hat)`). Les franchissements tangentiels ne produisent aucun effet
2. **Suivi du pic** : par body, seule la vitesse maximale pendant une frame est enregistrée (évite les doublons du sous-stepping)
3. **Limite de capacité** (`SHOCKWAVE_MAX_ACTIVE = 16`) : quand plein, l'événement le plus ancien est évincé

## Support du temps inversé

La simulation supporte le temps inversé via `time_scale < 0`. Les shockwaves utilisent `fabsf(sim_time - start_time)` pour le calcul de l'âge, garantissant :

- L'expansion de l'anneau est toujours vers l'extérieur quelle que soit la direction du temps
- Le nettoyage supprime les événements après `SHOCKWAVE_DURATION` secondes absolues
- Aucune accumulation d'événements périmés en mode inversé

## Profilage & Observabilité

Le draw des shockwaves est instrumenté à trois niveaux :

| Outil | Mécanisme | Visibilité |
|---|---|---|
| **GPU Profiler (F3)** | `GPU_STAGE_PROFILER("Shockwave VFX")` | Overlay in-app, timer query |
| **Tracy** | `TracyCZoneN` + `tracy_gpu_zone_begin/end` | Profiler Tracy (build avec `TRACY_ENABLE`) |
| **RenderDoc** | `gl_debug_push_group("Shockwave_VFX")` | Groupes de capture RenderDoc |

Le stage du profiler inclut le grab pass et les draw calls des billboards, donnant le coût GPU total de l'effet.

## Debug Wireframe

Quand le mode wireframe est actif (touche W), les billboards shockwave sont rendus en quads filaires via `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)`. Cela montre :

- La géométrie et l'orientation du quad billboard
- Le rayon d'expansion de l'anneau au fil du temps
- Le nombre de shockwaves actives
