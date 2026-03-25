# Guide de contribution aux traductions

Ce document explique comment contribuer à la localisation de la documentation de `suckless-ogl`.

## Système i18n

La documentation utilise le plugin `mkdocs-static-i18n` avec la structure de suffixe :
- `docs/page.md` — version anglaise (langue par défaut)
- `docs/page.fr.md` — version française
- `docs/page.de.md` — version allemande (etc.)

## Qu'est-ce qu'il faut traduire ?

### À traduire

- Les titres (`#`, `##`, `###`)
- Les paragraphes explicatifs
- Les descriptions dans les tableaux
- Le texte des admonitions (`> [!NOTE]`, `> [!WARNING]`)
- Les étiquettes naturelles dans les diagrammes Mermaid

### À ne pas traduire

- Les blocs de code (```` ``` ````...```` ``` ````)
- Les équations mathématiques (`$$`...`$$`)
- Les noms de fonctions, variables, fichiers (`app_run()`, `compile_commands.json`)
- Les identificateurs techniques (`GLFW_KEY_F11`, `GL_MAP_PERSISTENT_BIT`)
- Les noms d'attributs dans les diagrammes (`label=`, `style=`)

## Convention de nommage

| Langue | Suffixe | Exemple |
|--------|---------|---------|
| Français | `.fr.md` | `build.fr.md` |
| Allemand | `.de.md` | `build.de.md` |
| Espagnol | `.es.md` | `build.es.md` |

## Processus de contribution

### 1. Choisir une page

Consultez le tableau d'état ci-dessous pour voir les pages non encore traduites.

### 2. Créer le fichier

```bash
# Exemple pour traduire build.md en français
cp docs/build.md docs/build.fr.md
```

### 3. Traduire le contenu

Modifiez `docs/build.fr.md` en respectant les règles ci-dessus.

### 4. Vérifier le rendu

```bash
just docs        # Construit la documentation
just docs-serve  # Lance le serveur local sur http://localhost:8000
```

Naviguez vers `http://localhost:8000/fr/build/` pour vérifier votre traduction.

### 5. Soumettre une Pull Request

```bash
git add docs/build.fr.md
git commit -m "docs(i18n): add French translation for build.md"
```

Créez une PR avec la description du travail effectué.

## Tableau d'état des traductions

| Page | EN | FR | Description |
|------|----|----|-------------|
| `index.md` | ✅ | ✅ | Page d'accueil |
| `architecture.md` | ✅ | ✅ | Architecture de l'application |
| `async_loader.md` | ✅ | ✅ | Chargeur HDR asynchrone |
| `async_pbo.md` | ✅ | ✅ | PBOs persistants |
| `auto_exposure_debug_histogram.md` | ✅ | ✅ | Histogramme de débogage |
| `billboard_optimization.md` | ✅ | ✅ | Optimisation des billboards |
| `bloom_debug.md` | ✅ | ✅ | Mode débogage Bloom |
| `build.md` | ✅ | ✅ | Guide de compilation |
| `cicd_pipeline.md` | ✅ | ✅ | Pipeline CI/CD |
| `cinematic_rendering.md` | ✅ | ✅ | Rendu cinématique |
| `cpu_profiling_flamegraph.md` | ✅ | ✅ | Profilage CPU |
| `cubemap_seam_resolution.md` | ✅ | ✅ | Résolution des coutures cubemap |
| `debugging.md` | ✅ | ✅ | Débogage OpenGL |
| `docker.md` | ✅ | ✅ | Configuration Docker |
| `docs_staging_guide.md` | ✅ | ✅ | Guide de staging des docs |
| `doxygen_customization.md` | ✅ | ✅ | Personnalisation Doxygen |
| `env_transitions.md` | ✅ | ✅ | Transitions d'environnement |
| `equirectangular_seam_fix.md` | ✅ | ✅ | Correction coutures equirectangulaires |
| `exposure_analysis.md` | ✅ | ✅ | Analyse de l'exposition |
| `fix_async_loader_deadlock.md` | ✅ | ✅ | Correctif blocage chargeur |
| `fix_lint_bazzite_paths.md` | ✅ | ✅ | Correctif chemins Bazzite |
| `fullscreen_deadlock.md` | ✅ | ✅ | Correctif blocage plein écran |
| `FXAA.md` | ✅ | ✅ | Anti-aliasing FXAA |
| `gpu-rendering-synchronization.md` | ✅ | ✅ | Synchronisation GPU |
| `gpu_profiling.md` | ✅ | ✅ | Profilage GPU |
| `gpu_sorting.md` | ✅ | ✅ | Tri GPU |
| `ibl_architecture_ideas.md` | ✅ | ✅ | Idées architecture IBL |
| `ide_setup.md` | ✅ | ✅ | Configuration IDE |
| `justfile.md` | ✅ | ✅ | Utilisation de Just |
| `keyboard_system.md` | ✅ | ✅ | Système de raccourcis |
| `linting_strategy.md` | ✅ | ✅ | Stratégie de lint |
| `mesa_f32_to_f16_analysis.md` | ✅ | ✅ | Analyse Mesa F32→F16 |
| `mouse_camera_control.md` | ✅ | ✅ | Contrôle caméra souris |
| `nvidia_optimizations.md` | ✅ | ✅ | Optimisations NVIDIA |
| `opengl_cleanup.md` | ✅ | ✅ | Nettoyage OpenGL |
| `perf_and_notifications.md` | ✅ | ✅ | Performances et notifications |
| `perf_profiling.md` | ✅ | ✅ | Profilage perf |
| `photographic_standards.md` | ✅ | ✅ | Standards photographiques |
| `portability_pal.md` | ✅ | ✅ | Couche de portabilité |
| `postprocess_ubo_architecture.md` | ✅ | ✅ | Architecture UBO post-traitement |
| `profiling_guide.md` | ✅ | ✅ | Guide de profilage |
| `profiling_tracy.md` | ✅ | ✅ | Profilage Tracy |
| `progressive_ibl.md` | ✅ | ✅ | IBL progressif |
| `project_structure.md` | ✅ | ✅ | Structure du projet |
| `raii_cleanup_guide.md` | ✅ | ✅ | Guide RAII en C |
| `refactoring_analysis_2026_02.md` | ✅ | ✅ | Analyse de refactoring |
| `renderdoc_guide.md` | ✅ | ✅ | Guide RenderDoc |
| `shader-cross-gpu-compatibility.md` | ✅ | ✅ | Compatibilité shaders cross-GPU |
| `shader_optimization.md` | ✅ | ✅ | Optimisation des shaders |
| `simd_optimization.md` | ✅ | ✅ | Optimisation SIMD |
| `skybox_rendering.md` | ✅ | ✅ | Rendu skybox |
| `specular_aa.md` | ✅ | ✅ | Anti-aliasing spéculaire |
| `sphere_rendering.md` | ✅ | ✅ | Rendu des sphères |
| `stencil_masking.md` | ✅ | ✅ | Masquage stencil |
| `synchronization_overview.md` | ✅ | ✅ | Vue d'ensemble synchronisation |
| `texture_optimization.md` | ✅ | ✅ | Optimisation des textures |
| `ui_batching_optimization.md` | ✅ | ✅ | Optimisation du batching UI |
| `ui_visual_parameters.md` | ✅ | ✅ | Paramètres visuels UI |
| `visual_testing_artifacts.md` | ✅ | ✅ | Artefacts de test visuels |

---

## Interface du sélecteur de langue

Le bouton de langue dans l'en-tête est piloté par deux fichiers qui ne nécessitent **aucune modification** lors de l'ajout d'une nouvelle langue :

### Fonctionnement

```
document.documentElement.lang  →  "fr"
         ↓ lang_switcher.js
button[data-lang="FR"]          →  CSS content: attr(data-lang)  →  "FR"
```

1. **`docs/assets/javascripts/lang_switcher.js`** — s'exécute au `DOMContentLoaded`, lit `document.documentElement.lang` (défini automatiquement par MkDocs sur chaque page), le convertit en majuscules sur 2 caractères, puis pose `data-lang="XX"` sur le bouton.

2. **`docs/assets/stylesheets/extra.css`** — masque l'icône SVG globe par défaut et utilise une seule règle CSS générique :

```css
.md-header__option .md-select > button.md-header__button[data-lang]::before {
  content: attr(data-lang);
}
```

La langue active est également mise en évidence dans le menu déroulant avec un `✓` via la pseudo-classe CSS `:lang()`.

### Ajouter une nouvelle langue

Aucune modification CSS ou JS nécessaire. Il suffit d'ajouter la locale dans `mkdocs.yml` :

```yaml
plugins:
  - i18n:
      languages:
        - locale: de
          name: Deutsch
          build: true
```

Le bouton affichera automatiquement `DE` lors de la navigation sur les pages en allemand.

---

## Aide et questions

Pour toute question sur le processus de traduction, ouvrez une issue sur le dépôt GitHub.

Pour comprendre le fonctionnement technique du système i18n, voir la [documentation du plugin mkdocs-static-i18n](https://ultrabug.github.io/mkdocs-static-i18n/).
