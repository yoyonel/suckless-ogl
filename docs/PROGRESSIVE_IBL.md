# Architecture IBL Progressive & Asynchrone

Ce document détaille l'implémentation du chargement asynchrone et de la génération progressive des cartes IBL (Image Based Lighting) pour éliminer les freezes lors des changements d'environnement.

## 1. Vue d'Ensemble

L'objectif était de passer d'un chargement synchrone bloquant (100ms - 800ms de freeze) à une approche fluide où le temps de calcul est réparti sur plusieurs frames (Time Slicing).

### Le Pipeline

1.  **Chargement Disque (Thread Séparé)** : Le fichier `.hdr` est chargé et décodé (stb_image) dans un thread dédié (`async_loader.c`).
2.  **Upload GPU (Main Thread)** : Une fois prêt, les données brutes sont uploadées en VRAM (texture HDR).
3.  **Génération IBL (Progressive)** : Une state machine (`app_process_ibl_state_machine`) pilote les compute shaders étape par étape pour générer :
    *   L'Irradiance Map (Diffuse).
    *   La Specular Prefiltered Map (Reflection).
4.  **Swap (Double Buffering)** : On utilise des textures "Pending". L'ancien environnement reste affiché jusqu'à ce que le nouveau soit 100% prêt.

---

## 2. Stratégie de "Slicing" (Découpage)

Les Compute Shaders PBR (surtout pour les cartes Specular haute résolution) sont très coûteux. Calculer une texture 512x512 complète prend ~250ms sur GPU intégré, ce qui gèle l'application.

**Solution** : Découper le travail horizontalement ("Slicing") et ne calculer qu'une bande de l'image par frame.

### 2.1 Protection contre l'Overlap (Crucial)

Les Workgroups Compute Shader ont une taille fixe (32x32). Si on demande de calculer une slice de **1 pixel** de haut, le GPU lance quand même un block de 32 de haut.
Sans protection, les 31 lignes excédentaires écrasent/recalculent les pixels voisins, gaspillant massivement les ressources.

**Le Fix (`u_max_y`)** :
Nous passons une limite précise au shader :
```glsl
// shaders/IBL/spmap.glsl & irmap.glsl
uniform int u_max_y; // Limite Y de la slice actuelle

void main() {
    // ...
    if (pos.y >= u_max_y) return; // Arrêt immédiat des threads fantômes
    // ...
}
```

---

## 3. Configuration Optimisée (Adaptive Slicing)

Pour concilier **fluidité** (pas de freeze) et **vitesse globale** (chargement rapide), nous utilisons une stratégie adaptative selon la lourdeur de la tâche.

### A. Irradiance Map (64x64)
- **Stratégie** : Slicing constant.
- **Découpage** : 4 Slices.
- **Coût** : ~40ms / slice (Total ~160ms).

### B. Specular Map (512x512)
C'est la partie la plus lourde. Le coût par mipmap décroît exponentiellement.

| Niveau Mip | Taille | Stratégie | Coût Est. / Frame | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Mip 0** | 512x512 | **4 Slices** | ~30-40ms | Le plus lourd (High Frequency details). |
| **Mip 1** | 256x256 | **2 Slices** | ~20-30ms | Moyen. |
| **Mip 2** | 128x128 | **1 Slice** | ~15ms | Léger, calculé en une fois. |
| **Mip 3-10** | 64..1 | **Tail Grouping** | ~20ms (Total) | Tous calculés en **une seule frame**. |

**Total "Tail Grouping"** : Regrouper les petits mips (3 à 10) évite de perdre 7 frames de latence pour des travaux minuscules (<1ms chacun).

---

## 4. Performance Globale

Avec cette architecture sur un GPU intégré (Intel UHD) :
- **FPS** : Reste fluide (pas de chute brutale sous les 30 FPS).
- **Temps Total** : Une transition complète d'environnement prend environ **600ms à 800ms**.
- **Latence Ressentie** : Quasi-nulle grâce à l'affichage continu de l'ancien environnement pendant le calcul.

## 5. Fichiers Clés

- `src/app.c` : Contient la State Machine (`app_process_ibl_state_machine`).
- `src/pbr.c` : Implémente l'envoi des uniforms de slicing (`pbr_prefilter_mip`, `pbr_irradiance_slice_compute`).
- `shaders/IBL/*.glsl` : Shaders modifiés pour supporter `u_offset_y` et `u_max_y`.
