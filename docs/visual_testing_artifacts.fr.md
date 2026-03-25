# Artefacts de tests visuels et de régression

Ce document explique les raisons techniques des différences visuelles observées dans les rapports de régression automatisés, en particulier entre le rendu matériel local et le rendu logiciel CI.

## Le défi fondamental : logiciel vs matériel

Le CI du projet (GitHub Actions) utilise **Mesa llvmpipe**, une implémentation OpenGL basée sur le logiciel qui s'exécute sur le CPU. Bien que très conforme, elle diffère des GPU matériels (NVIDIA, Intel, AMD) sur plusieurs points :

### 1. Précision en virgule flottante

- **Parité bit-à-bit** : Même avec des pilotes conformes aux standards, les fonctions transcendantes comme `pow()`, `exp()`, `sin()`, et `cos()` varient légèrement dans leur implémentation de bas niveau.
- **Erreurs accumulées** : Dans les shaders PBR complexes, ces micro-écarts s'accumulent, produisant du bruit "sel et poivre" dans les cartes de différence.

### 2. L'artefact "centre de sphère"

Un constat fréquent dans nos tests PBR est un motif d'anneaux concentriques au centre des sphères.

- **Cause** : Au centre d'une sphère, le produit scalaire **N · V** vaut exactement `1.0`.
- **Sensibilité** : La LUT BRDF (Look-Up Table) est échantillonnée avec (**N · V**, **rugosité**). La moindre variation de virgule flottante dans le calcul de **N · V** ou les coordonnées d'échantillonnage amène le matériel à choisir un pixel différent dans la LUT par rapport au rendu logiciel.
- **Résultat** : Deltas à fort contraste dans la carte de différence, même si la différence visuelle est imperceptible à l'œil humain.

## Évolution du moteur PBR

Les améliorations récentes du moteur de rendu contribuent également aux deltas par rapport aux références "Master" plus anciennes :

### 1. Diffusion multiple (Kulla-Conty)

Un terme de compensation pour la perte d'énergie sur les surfaces rugueuses a été implémenté. Cela augmente la luminosité des matériaux métalliques/rugueux, modifiant la luminance globale par rapport à une implémentation PBR "standard".

### 2. Clamping analytique de la rugosité

Pour garantir la portabilité entre les vendeurs, le lissage basé sur les dérivées (`fwidth`) a été remplacé par le **clamping analytique de la rugosité** (`MIN_ROUGHNESS = 0.03`).

- **Avantage** : Résultats identiques sur Intel, NVIDIA et Mesa.
- **Delta** : Les images de référence capturées avec les anciennes versions utilisant `fwidth` afficheront des deltas significatifs aux bords.

## Convention de nommage et multi-vues

Pour assurer une couverture complète et une catégorisation claire, une convention de nommage structurée est utilisée pour les images de référence :

**Format** : `ref_<vue>_<mode>_<effet>.png`

- **Vue** : `front`, `back`, `left`, `right`, `top`, `bottom`.
- **Mode** : `default`, `subtle`.
- **Effet** : `none`, `bloom`, `fxaa`, `dof`, `auto_exposure`, `motion_blur`, etc.

### Stratégie du mode Subtle

Par défaut, les tests de régression visuelle pour les effets de post-traitement sont conduits en mode **Subtle** (`PRESET_SUBTLE`). Cela évite que les artefacts visuels surcharge les cartes de différence et concentre l'attention sur l'impact haute fréquence de l'effet lui-même.

### Stratégie de test du motion blur

Comme le motion blur repose sur le calcul de vélocité caméra entre frames (`previousViewProj`), le tester avec un seul rendu statique ne produit aucun flou. Une stratégie **Double Séquence de frames** est employée :

1. **Frame N-1 (Échauffement)** : Rendu de la vue statique pour initialiser la matrice `previousViewProj` dans le shader.
2. **Frame N (Mouvement)** : Application d'une rotation caméra déterministe (`yaw` ou `pitch`) selon le `ViewPoint` et rendu de cette seconde frame.
3. **Capture** : L'image résultante capture un flou cohérent et déterministe piloté entièrement par des données spatiales (pas par le temps). La rotation est privilégiée par rapport à la translation pour éviter l'introduction de parallaxe et de discontinuités de pondération de profondeur qui surviennent lors du déplacement autour du pilier de sphères, garantissant des références de test 100% stables dans tous les environnements CI.

## Rapport visuel interactif

Le rapport visuel (`index.html`) généré par le CI fournit des outils de comparaison avancés :

1. **Menu sélectif** : Filtrer par Vue, Mode et Effet pour isoler rapidement les régressions.
2. **Slider de comparaison interactif** : Faire glisser un curseur sur l'image pour voir une comparaison côte à côte entre la Référence (Baseline) et le Rendu PR.
3. **Visualisation d'effets** : Un mode dédié pour voir le delta entre "Aucun" et "Avec Effet", aidant les développeurs à comprendre la contribution exacte d'un pass shader spécifique.
4. **Loupe** : Un outil de zoom activable (2.5×) qui suit le curseur, permettant une inspection pixel parfaite des détails de rendu (AA, bruit, banding) dans le slider et les cartes de différence.
5. **Cartes de différence** : Des cartes à fort contraste (intensité ×5) sont générées automatiquement pour les régressions PR et la visualisation d'effets.

## Directives pour la mise à jour des références

### 1. Vérification ISO

Lors de la création ou de la mise à jour des références, la cohérence visuelle est assurée par :

- La standardisation sur les **Billboard + Imposteurs raycastés** pour les primitives géométriques (sphères).
- La suppression des mises à jour de buffers/maillages redondants (comme `icosphere_generate`) pour minimiser le bruit des écarts de triangulation côté CPU.
- L'utilisation d'une distance caméra fixe (`25.0`) centrée à l'origine.

### 2. Mise à jour des références

Quand un rapport de régression visuelle affiche des échecs :

1. **Inspecter avec la loupe** : Déterminer si les deltas sont concentrés aux bords géométriques (précision/AA) ou dans des motifs de bruit (échantillonnage).
2. **Vérifier l'intention du PR** : Si le PR modifie les mathématiques PBR, l'éclairage ou le post-traitement, un delta est attendu.
3. **Regénérer localement** :
   - Lancer `GEN_REFS=1 .github/workflows/scripts/run_test_with_xvfb.sh build/tests/test_app` pour regénérer la baseline.
   - Commiter les fichiers PNG mis à jour.

---

*Voir aussi : [Compatibilité shader multi-GPU](./shader-cross-gpu-compatibility.md)*
