# Analyse des écarts ImGui : suckless-odin vs suckless-ogl

> **Sources analysées**
> - Référence Odin : `src/gui/gui.odin` (1317 l.), `gui_compute.odin` (383 l.), `gui_postfx.odin` (1388 l.)
> - Portage C++ : `src/gui.cpp` (1323 l.), `src/gui.h`
> - Captures d'écran de référence du runtime suckless-odin

## Légende

| Symbole | Signification |
|---------|--------------|
| ✅ | Implémenté et fonctionnel |
| ⚠️ | Partiellement implémenté ou placeholder |
| ❌ | Absent de suckless-ogl |
| 🔴 | Écart critique (fonctionnalité manquante ou simulée) |
| 🟠 | Écart important (feature significative absente) |
| 🟡 | Écart qualité (ergonomie, discoverabilité) |

---

## 1. Fonctionnalités globales / transversales

### 1.1 Tooltips `(?)`

Chaque slider, checkbox et combo en Odin dispose d'un label `(?)` affichant sur survol
un `SetTooltip` décrivant : l'effet du paramètre, ses unités, et des conseils pratiques.

**Statut suckless-ogl** : environ 5 contrôles sur 80 ont un tooltip. Le reste n'a que des labels nus.

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| Camera — 8 sliders tous annotés `(?)` | ✅ | ❌ | 🟠 |
| Scene — tooltips | ✅ | ❌ (2/10) | 🟠 |
| Rendering — tooltips | ✅ | ❌ | 🟠 |
| Post-FX — tooltip par effet | ✅ | ❌ | 🟠 |
| Onglet MBlur — tous les paramètres annotés | ✅ | ❌ | 🟠 |
| Compute Tuning — SPBRDF, SPMap, IRMap | ✅ | ❌ | 🟠 |
| Compute Tuning — paramètres de slicing | ✅ | ❌ | 🟠 |

### 1.2 Boutons Reset

Odin fournit des boutons `Reset` ou `Reset to Defaults` par paramètre/effet.
Dans suckless-ogl, **aucun mécanisme de reset n'existe** dans l'interface.

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| Post-FX : `Reset` par effet dans Settings | ✅ | ❌ | 🟠 |
| Compute Tuning : `Reset` par slider | ✅ | ❌ | 🟠 |
| Onglet MBlur : `Reset to Defaults` | ✅ | ❌ | 🟠 |
| Camera : `Reset Camera` | ✅ | ✅ | OK |

### 1.3 A/B Split par effet Post-FX

Chaque effet en Odin expose : une **checkbox A/B Split**, un **slider de position** (`← %.0f%% →`)
et un **badge coloré `[S]`** (palette Glasbey) pour la lisibilité multi-split.
suckless-ogl n'a **aucun A/B split par effet**.

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| A/B split par effet + slider position | ✅ tous effets | ❌ | 🔴 |
| Indicateur coloré `[S]` multi-split | ✅ | ❌ | 🟡 |
| A/B split global Specular AA | ✅ | ❌ | 🟠 |

### 1.4 ImGui Docking

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| `ImGuiConfigFlags_DockingEnable` | ✅ | ❌ | 🟡 |
| Fenêtre détachable/repositionnable | ✅ | ❌ | 🟡 |

---

## 2. Onglet Camera

**Paramètres : parité. Manquant : tous les tooltips côté C++.**

| Paramètre | Odin | C++ | Tooltip |
|-----------|:----:|:---:|:-------:|
| Position (affichage) | ✅ | ✅ | — |
| Yaw / Pitch (affichage) | ✅ | ✅ | — |
| Speed | ✅ | ✅ | Odin ✅ C++ ❌ |
| Acceleration | ✅ | ✅ | Odin ✅ C++ ❌ |
| Friction | ✅ | ✅ | Odin ✅ C++ ❌ |
| Sensitivity | ✅ | ✅ | Odin ✅ C++ ❌ |
| Rotation Smoothing | ✅ | ✅ | Odin ✅ C++ ❌ |
| Mouse Smoothing | ✅ | ✅ | Odin ✅ C++ ❌ |
| FOV | ✅ | ✅ | Odin ✅ C++ ❌ |
| Head Bobbing toggle | ✅ | ✅ | Odin ✅ C++ ❌ |
| Bobbing Frequency | ✅ | ✅ | Odin ✅ C++ ❌ |
| Bobbing Amplitude | ✅ | ✅ | Odin ✅ C++ ❌ |
| Bouton Reset Camera | ✅ | ✅ | — |

---

## 3. Onglet Scene

| Paramètre | Odin | C++ | Notes |
|-----------|:----:|:---:|-------|
| Toggle Skybox | ✅ | ✅ | |
| Skybox Blur (LOD) | ✅ | ✅ | |
| Subdivisions (icosphère) | ❌ | ✅ | Exclusivité C++ |
| **Skybox Mode** (Equirect / Cubemap) | ✅ | ❌ | 🟠 |
| **Blur Source** (Mipmap / IBL Prefilter) | ✅ | ❌ | 🟠 |
| **Cubemap Mipmap Mode** (seamless) | ✅ | ❌ | 🟠 |
| **Show Blur Diff + Diff Gain** | ✅ | ❌ | 🟡 |
| PBR Debug Mode combo (10 modes, live) | ❌ placeholder | ✅ | C++ plus complet |
| **GI Mode** live (OFF / Volume / SSBO) | ❌ placeholder | ✅ | Exclusivité C++ |
| **Show Probe Grid** | ❌ | ✅ | Exclusivité C++ |
| Sort Mode | ✅ 3 modes | ✅ 3+GPU Bitonic | |
| Wireframe | ✅ | ✅ | |

---

## 4. Onglet Rendering

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| **Edge AA checkbox + vue debug heatmap** | ✅ | ❌ | 🟠 |
| Tooltip `(?)` sur Edge AA | ✅ | ❌ | 🟡 |
| Info MSAA (lecture seule) | ⚠️ | ✅ texte | |
| Specular AA toggle | ✅ | ✅ | |
| Specular AA Mode (Screen-Space / Curvature) | ✅ | ✅ | |
| Tooltip `(?)` sur Specular AA | ✅ | ❌ | 🟡 |
| **Vues debug Specular AA** (variance / diff) | ✅ | ❌ | 🟠 |
| **A/B Split — Specular AA** + slider position | ✅ | ❌ | 🟠 |
| Section Debug Views (Fog, Histogram, Stencil) | ⚠️ placeholder | ❌ | |
| Section Profiling (GPU Timeline, Metrics, Perf) | ⚠️ placeholder | ❌ | |
| Section Scene Debug (N-Body, Probes, sliders) | ⚠️ placeholder | ❌ | |
| Section Environment (HDR Index, Hot-Reload) | ⚠️ placeholder | ❌ | |

---

## 5. Onglet Post-FX

### 5.1 Contrôles globaux

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| Master toggle Enable Post-FX | ✅ | ✅ | |
| Combo Preset (Default, Cinematic…) | ✅ | ✅ (13 presets) | |
| Section Save / Load dépliable | ✅ | ⚠️ stub | 🟠 |

### 5.2 Pattern par effet — absent en C++

Chaque effet en Odin suit un pattern **entièrement absent en C++** :

```
[✓] Nom effet  [S]           ← badge coloré A/B split actif
    ▶ Settings
        [Reset]              ← reset aux valeurs par défaut
        Slider (?)           ← tooltip sur chaque paramètre
        [✓] A/B Split        ← split-screen par effet
            ← 50% →         ← slider de position du split
```

### 5.3 Matrice par effet

| Effet | Reset Odin | Reset C++ | A/B Odin | A/B C++ | Tooltips Odin | Tooltips C++ |
|-------|:----------:|:---------:|:--------:|:-------:|:-------------:|:------------:|
| Exposure | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Tonemapping | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Vignette | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Film Grain | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Chrom. Aberration | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Color Grading | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Bloom | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| FXAA | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Auto-Exposure | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Depth of Field | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Motion Blur | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Banding | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Fog | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| LUT3D | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |

---

## 6. Onglet MBlur (Dédié)

Odin dispose d'un **onglet dédié** avec des contrôles avancés absents de suckless-ogl.

| Feature | Odin (onglet MBlur) | C++ (dans Post-FX) |
|---------|:-:|:-:|
| Enable Motion Blur | ✅ | ✅ |
| Intensity + `(?)` | ✅ | ❌ tooltip |
| Max Velocity + `(?)` | ✅ | ❌ tooltip |
| Samples + `(?)` | ✅ | ❌ tooltip |
| **Reset to Defaults** | ✅ | ❌ |
| **Injection de vélocité synthétique** | ✅ | ❌ |
| — Enable Injection | ✅ | ❌ |
| — Direction (0–360°) | ✅ | ❌ |
| — Magnitude (fraction UV) | ✅ | ❌ |
| — Presets directionnels: Right/Up/Left/Down | ✅ | ❌ |
| — Presets de vitesse: Slow/Medium/Fast/Max | ✅ | ❌ |
| — Vecteur UV live | ✅ | ❌ |
| Debug Mode combo (Velocity / Tile-Max / Neighbor-Max / Heatmap) | ✅ 4 modes | ❌ |
| Diagnostic état (bits flags) | ✅ | ❌ |

---

## 7. Onglet Profiling — Écarts critiques

### 7.1 Widget Performance Mode

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| **Section Performance Mode** en tête d'onglet | ✅ | ❌ | 🔴 |
| Tooltip `(?)` (GameMode / SCHED_FIFO / nice) | ✅ | ❌ | |
| Checkbox Enable/Disable | ✅ | ❌ | |
| Label backend `[GameMode]` / `[SCHED_FIFO]` / `[Nice]` | ✅ | ❌ | |
| Notification restart Mesa | ✅ | ❌ | |

### 7.2 Tableau GPU Timer

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| **Toggle Enable Profiling** | ✅ | ❌ | 🔴 |
| Colonnes : Pass, **Avg**, **Min**, **Max**, % | ✅ 5 colonnes | ⚠️ 3 (Stage, ms, %) | 🟠 |
| **Colonnes Min / Max** (plage statistique, grisée) | ✅ | ❌ | 🟠 |
| **Ligne Total** (Avg/Min/Max agrégés) | ✅ | ❌ | 🟠 |
| **Résumé budget frame** `PostFX: X.X% (Y.YY ms)` vert/jaune/rouge | ✅ | ❌ | 🟠 |

!!! note "Statistiques glissantes vs échantillon instantané"
    Le tableau Odin expose des statistiques **min/max/avg** sur fenêtre glissante —
    indispensable pour détecter les pics GPU. Le C++ n'affiche que l'échantillon
    instantané du dernier frame.

---

## 8. Onglet Shaders — Écarts critiques

### 8.1 Enable / Disable

| Feature | Odin | C++ |
|---------|:----:|:---:|
| Checkbox Enable Variants | ✅ | ✅ |
| Texte "Disabled — using uber-shader" | ✅ | ❌ |

### 8.2 Affichage des effets actifs

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| **Active Effects** — liste à puces avec noms lisibles | ✅ | ⚠️ flags hex uniquement | 🟠 |

### 8.3 Statut du cache

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| **Status: CACHED (program N)** / **MISS** coloré | ✅ | ⚠️ compteur statique | 🟠 |

### 8.4 Actions

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| **Compile Current** (appelle `pipeline_compile_variant`) | ✅ | ❌ | 🔴 |
| Bouton désactivé quand déjà en cache | ✅ | ❌ | 🟡 |
| Hint "(already cached)" / "(cache full)" | ✅ | ❌ | 🟡 |
| **Clear All** (détruit le cache) | ✅ | ❌ | 🔴 |

### 8.5 Tableau des variantes cachées

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| `Cached Variants: N / MAX` live | ✅ | ⚠️ statique "0 ou 1" | 🔴 |
| Tableau : #, Program ID, Effects (noms lisibles) | ✅ | ❌ | 🔴 |
| Variante active mise en valeur en vert | ✅ | ❌ | 🟡 |

---

## 9. Onglet Compute Tuning

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| SPBRDF Sample Count + `(?)` | ✅ | ✅ sans tooltip | 🟡 |
| SPMap Sample Count + `(?)` | ✅ | ✅ sans tooltip | 🟡 |
| IRMap Sample Delta + `(?)` | ✅ | ✅ sans tooltip | 🟡 |
| **Reset par slider** | ✅ | ❌ | 🟠 |
| Mip 0/1/2 Slices + `(?)` | ✅ | ✅ sans tooltip | 🟡 |
| Irradiance Slices + `(?)` | ✅ | ✅ sans tooltip | 🟡 |
| **Mip Grouping Start Mip + `(?)`** | ✅ | ❌ | 🟠 |
| **Seamless Progressive Mip Threshold + `(?)`** | ✅ | ❌ | 🟠 |
| **Apply & Recalculate** (callback réel câblé) | ✅ réel | ⚠️ mock | 🔴 |
| **Combo profil actif** | ✅ | ❌ | 🔴 |
| **Save Draft to profile** | ✅ | ❌ | 🔴 |
| **Delete Profile** (modal de confirmation) | ✅ | ❌ | 🔴 |
| **Create new profile** (saisie nom + Create) | ✅ | ❌ | 🔴 |
| **Persistance JSON** (lecture/écriture fichier config) | ✅ | ❌ | 🔴 |
| Validation avant apply/save | ✅ | ❌ | 🟠 |
| Messages status/erreur avec timer d'auto-dismiss | ✅ | ⚠️ status seulement | 🟡 |

---

## 10. Onglet IBL Debug

Parité quasi totale sur cet onglet.

| Feature | Odin | C++ |
|---------|:----:|:---:|
| Slider Preview Size | ✅ | ✅ |
| Preview Exposure (EV) + reset clic droit | ✅ | ✅ |
| Env Map : dépliable + inspecteur pixel | ✅ | ✅ |
| Irradiance Map : dépliable + inspecteur | ✅ | ✅ |
| Prefilter Map + slider Mip Roughness | ✅ | ✅ |
| BRDF LUT : dépliable + inspecteur | ✅ | ✅ |
| Section GPU Memory Estimate | ✅ | ✅ |
| Bouton "Go To" depuis la recherche | ✅ | ✅ |

---

## 11. Configuration UI / Application et persistance

| Feature | Odin | C++ | Écart |
|---------|:----:|:---:|-------|
| Profils Compute Tuning (JSON I/O) | ✅ | ❌ | 🔴 |
| Save / Load preset Post-FX (JSON) | ✅ | ⚠️ apply only | 🟠 |
| Persistance état UI entre sessions | ❌ | ❌ | — |
| Restauration onglet après recherche (`restore_tab`) | ✅ | ✅ | OK |

---

## 12. Feuille de route de portage

| Priorité | Feature | Effort |
|----------|---------|--------|
| 🔴 1 | Compute Tuning — Apply callback réel | Faible |
| 🔴 2 | Compute Tuning — CRUD + JSON | Moyen |
| 🔴 3 | Shaders — Compile Current + Clear All | Faible |
| 🔴 4 | Shaders — Tableau variantes live | Faible |
| 🔴 5 | Shaders — Liste effets actifs lisible | Faible |
| 🟠 6 | Post-FX — boutons Reset par effet | Faible |
| 🟠 7 | Post-FX — A/B Split par effet + slider | Moyen |
| 🟠 8 | Profiling — toggle Enable Profiling | Faible |
| 🟠 9 | Profiling — colonnes Min/Max + ligne Total | Faible |
| 🟠 10 | Profiling — résumé budget frame | Faible |
| 🟠 11 | Widget Performance Mode | Moyen |
| 🟠 12 | Onglet MBlur dédié avec injection | Moyen |
| 🟠 13 | Rendering — Edge AA + debug heatmap | Faible |
| 🟠 14 | Rendering — A/B Split Specular AA | Faible |
| 🟡 15 | Tooltips `(?)` sur tous les paramètres | Faible (volume) |
| 🟡 16 | ImGui Docking | Trivial (1 flag) |
| 🟡 17 | Indicateur `[S]` A/B split | Faible |

---

## 13. Scorecard récapitulatif

```
Domaine                  suckless-odin    suckless-ogl (C++)
──────────────────────────────────────────────────────────────
Tooltips (?)             ████████████     ██░░░░░░░░░░  ~17%
Boutons Reset            ████████████     ░░░░░░░░░░░░   0%
A/B Split par effet      ████████████     ░░░░░░░░░░░░   0%
Contrôles Post-FX        ████████████     ████████░░░░  ~65%
Observabilité Profiling  ████████████     ████░░░░░░░░  ~35%
Onglet Shaders           ████████████     ████░░░░░░░░  ~35%
Compute Tuning           ████████████     ████░░░░░░░░  ~35%
Onglet MBlur dédié       ████████████     ░░░░░░░░░░░░   0%
Performance Mode         ████████████     ░░░░░░░░░░░░   0%
IBL Debug                ████████████     ████████████  ~95%
Contrôles Camera         ████████████     ████████░░░░  ~80%
──────────────────────────────────────────────────────────────
Estimation globale       100%             ~45%
```

!!! warning "Apply & Recalculate est un mock dans suckless-ogl"
    Le bouton "Apply & Recalculate Active Environment" dans `src/gui.cpp`
    ne fait qu'afficher un message de statut. Il **ne déclenche aucun**
    recalcul de shader. En Odin, il appelle `apply_compute_tuning` qui
    recompile et relance la convolution IBL.

!!! note "Features exclusives à suckless-ogl (non portées en Odin)"
    - GPU Bitonic Sort (tri GPU parallèle)
    - Subdivisions icosphère (niveau de détail mesh)
    - Toggle Show Probe Grid
    - GI Mode live (OFF / Volume 3D / SSBO)
    - PBR Debug Mode live combo (10 modes)
