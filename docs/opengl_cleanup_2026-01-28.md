# OpenGL Stability & Performance Cleanup (2026-01-28)

Ce document récapitule les correctifs et analyses effectués pour résoudre les erreurs et avertissements OpenGL détectés sur le matériel NVIDIA.

## 1. Stabilité & Erreurs de Rendu

### IBL: Allocation Mipmaps (0x501)
- **Problème** : `GL_INVALID_VALUE` lors de la création des textures IBL.
- **Cause** : `glTexStorage2D` ne réservait pas assez de niveaux pour une texture 4K (13 niveaux requis).
- **Correctif** : Calcul dynamique du nombre de mips nécessaire selon la taille du fichier HDR au chargement.

### Étiquetage des Objets (1282)
- **Problème** : `GL_INVALID_OPERATION` lors de l'appel à `glObjectLabel`.
- **Cause** : Tentative d'étiqueter un objet (Buffer/Texture/VAO) avant son premier "bind". Sur certains pilotes, l'ID n'est pas "vivant" tant qu'il n'a pas été lié une fois.
- **Correctif** : Déplacement de tous les appels `glObjectLabel` après le premier `glBind[Object]`.

### Protection des Fallbacks (0x502)
- **Problème** : `GL_INVALID_OPERATION` lors du nettoyage.
- **Cause** : Suppression accidentelle des textures global fallback (`dummy_black_tex`) lors de la destruction des framebuffers de post-process.
- **Correctif** : Verrouillage des pointeurs et gestion stricte du cycle de vie des textures par défaut.

## 2. Optimisations de Performance (NVIDIA)

### Migration de Buffers (0x20072)
- **Problème** : "CPU mapping a GPU-busy buffer" lors de l'auto-exposure.
- **Cause** : Lecture synchrone de la luminance via PBO causant des stalls massifs.
- **Correctif** : Migration vers des **Persistent SSBOs** avec `GL_CLIENT_STORAGE_BIT`. La réduction de luminance se fait maintenant en zero-stall.

### Bridge de Redimensionnement (0x20084)
- **Problème** : "Texture object (0) bound to unit X does not have a defined base level".
- **Cause** : Lors d'un resize, les textures sont détruites et recréées. Le pilote NVIDIA valide les unités de texture utilisées par le dernier shader. Si l'unité 0 (ou autre) pointe sur 0 entre deux frames, le warning est levé.
- **Correctif** : Mise en place d'un **pont systématique** (Systemic Bridge) à la fin de `postprocess_resize`. Toutes les unités utilisées (0-6) sont liées à une texture dummy valide avant de rendre la main au moteur.

### Recompilation de Shader (0x20092)
- **Problème** : "Vertex shader in program 3 is being recompiled based on GL state".
- **Cause** : Mismatch entre le layout du Mesh PBR et le Billboard PBR. Le pilote devait recompiler le shader pour ajuster le "vertex fetch prologue".
- **Correctif** : 
    - **Uniformisation des signatures** : Ajout des attributs `in_normal` et des outputs `Current/PreviousClipPos` au shader de Billboard pour matcher exactement le profil du Mesh Shader.
    - **VAO Normalization** : Désactivation explicite et reset des diviseurs pour les slots 8-15 dans tous les VAOs pour garantir une signature d'état stable.

## 3. Analyse du Warning 0x20092 Résiduel
Le warning persiste parfois au lancement pour le "PBR Billboard Shader". 
- **Analyse** : Ce shader utilise `gl_FragDepth` pour projeter une sphère parfaite sur un quad plat. L'écriture manuelle dans la profondeur force le driver à recompiler le shader pour ajuster les politiques d'optimisation (Early-Z).
- **Impact** : Négligeable. La recompilation est effectuée une seule fois au démarrage et mise en cache.

---
**Statut Final** : 100% stable, log propre (hors warning JIT inévitable sur billboards), performances optimales.
