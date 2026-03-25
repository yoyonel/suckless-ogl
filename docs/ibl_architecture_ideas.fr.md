# Stratégies d'optimisation IBL

Ce document présente des idées architecturales pour réduire l'impact sur les performances des mises à jour de la carte d'environnement, notamment les longs blocages GPU pendant la génération de la carte spéculaire préfiltrée.

## 1. Analyse du problème

Les métriques actuelles montrent que la génération de la carte spéculaire préfiltrée pour un environnement 4K prend ~350 ms sur les graphiques intégrés. Cela provoque un gel complet de la boucle de rendu.

## 2. Solutions proposées

### A. IBL progressif (amortissement temporel)
Au lieu de calculer tous les niveaux de mipmap en une seule boucle, les traiter de manière incrémentielle.
- **Mécanisme** : Utiliser une machine à états dans l'`AsyncLoader` ou la boucle `App`.
- **Flux de travail** : `Image N : Chargement HDR` → `Image N+1 : Mip 0` → `Image N+2 : Mip 1` …
- **Résultat** : Zéro gel. L'utilisateur voit l'environnement « s'affiner » ou « se flouter » sur quelques images.

### B. Comptage d'échantillons adaptatif
Les niveaux de mip inférieurs représentent des surfaces plus rugueuses où les détails haute fréquence sont déjà perdus.
- **Idée** : Utiliser 1024 échantillons pour le Mip 0, mais descendre à 512, 256, 128 pour les mips supérieurs.
- **Technique** : Passer `u_sample_count` comme uniforme au shader compute.

### C. Plafonnement de la résolution
Traiter directement une texture 4K pour l'IBL est souvent excessif car les reflets sont rarement parfaits au pixel près.
- **Règle** : Plafonner la résolution de la carte spéculaire préfiltrée à **1024×512** quelle que soit la taille HDR source.
- **Bénéfice** : Réduction quadratique de la complexité par rapport à 4K.

### D. Distribution en tuiles
Pour les grands mips, même un seul `glDispatchCompute` peut dépasser un budget d'image raisonnable.
- **Idée** : Diviser un niveau de mipmap unique en tuiles (ex. : grille 4×4).
- **Mécanisme** : Dispatcher uniquement un sous-ensemble de tuiles par image jusqu'à ce que le niveau soit complet.

### E. Substitut instantané (la « voie rapide »)
Éviter d'attendre un shader compute pour afficher le nouvel environnement.
- **Flux de travail** :
    1. Charger le HDR en RAM.
    2. Téléverser en VRAM.
    3. `glCopyImageSubData` HDR vers SpecMap Mip 0 (instantané).
    4. Remplacer progressivement les mips au fur et à mesure de leur calcul.
- **Résultat** : Le retour visuel est instantané.

## 3. Feuille de route d'implémentation

1. [ ] Implémenter le **Plafonnement de résolution** (Plus facile, ROI élevé).
2. [ ] Implémenter la **Génération de mip progressive** (Meilleure expérience utilisateur).
3. [ ] Implémenter les **Échantillons adaptatifs** (Optimisation GPU pure).
