# Tri des sphères sur GPU

Ce document fournit une analyse approfondie des algorithmes de tri utilisés dans ce projet pour permettre la transparence correcte de milliers de sphères.

## 1. Pourquoi trier ?

En infographie, la transparence exige de rendre les fragments dans l'ordre **dos à face** (algorithme du Peintre). Si une sphère proche est rendue avant une sphère éloignée, le tampon de profondeur empêchera le rendu de la sphère éloignée, même si elle devrait être visible à travers les pixels semi-transparents de la sphère proche.

## 2. Tri bitonique (implémentation GPU)

Le **tri bitonique** est un algorithme de « réseau de tri ». Contrairement à `qsort` ou `std::sort`, il effectue une séquence fixe de comparaisons indépendamment des valeurs des données.

### Fonctionnement
1. **Séquence bitonique** : Une séquence $a_0, a_1, \dots, a_n$ est bitonique si elle croît de manière monotone puis décroît de manière monotone (ou peut être décalée circulairement pour satisfaire cette propriété).
2. **Division bitonique** : Si vous disposez d'une séquence bitonique et comparez $a_i$ avec $a_{i+n/2}$, en les échangeant pour placer le plus petit en premier, vous obtenez deux séquences bitoniques plus petites où chaque élément du premier est plus petit que chaque élément du second.
3. **Récursion** : En appliquant récursivement cette division, vous obtenez des éléments individuellement triés.

### Pourquoi c'est parfait pour le GPU
- **Pas de branchement** : Un shader GPU est comme une chaîne de montage. Si un ouvrier (thread) doit exécuter un « if » et un autre non, toute la chaîne ralentit (divergence). Le tri bitonique a *zéro* divergence car chaque thread effectue exactement le même motif de comparaison.
- **Parallélisme massif** : Avec une complexité de $O(N \log^2 N)$, il effectue plus de travail total que les algorithmes $O(N \log N)$, mais le réalise **simultanément** sur des milliers de cœurs GPU.

### Notre approche optimisée par « Proxy »
Initialement, nous échangions des structs `SphereInstance` de 128 octets. C'était lent en raison de la bande passante VRAM.
- **Solution** : Nous extrayons des **entrées Proxy** de 8 octets (4 octets float profondeur, 4 octets int index).
- **Passe de préparation** : Calcul des profondeurs une seule fois.
- **Passe de tri** : Tri bitonique sur des proxies de 8 octets (très efficace pour le cache).
- **Passe de permutation** : Utilisation des indices triés pour copier les structs de 128 octets vers le tampon final en un seul saut.

## 3. Tri Radix (implémentation CPU)

Pour le CPU, nous avons implémenté un **tri Radix LSD (Least Significant Digit)**. Il s'agit d'un algorithme non comparatif qui ne compare pas les éléments entre eux.

### Fonctionnement
1. **Mise en seau** : Au lieu de comparer A avec B, on examine les « chiffres » de la valeur. Dans notre cas, nous traitons un float 32 bits comme quatre « chiffres » de 8 bits (base 256).
2. **Passes** :
    - Passe 1 : Tri par bits 0-7.
    - Passe 2 : Tri par bits 8-15.
    - Passe 3 : Tri par bits 16-23.
    - Passe 4 : Tri par bits 24-31.
3. **Comptage** : Dans chaque passe, on compte combien d'éléments tombent dans chacun des 256 « seaux », on calcule la somme préfixe (décalages), et on déplace les éléments vers un tampon temporaire.

### L'astuce « Float IEEE-754 »
Trier des floats avec Radix est délicat car les représentations binaires des floats négatifs ne se trient pas naturellement dans l'ordre numérique.
- **La solution** : On applique une transformation aux bits bruts :
    - Si le float est positif, on inverse le bit de signe.
    - Si le float est négatif, on inverse *tous* les bits.
- **Résultat** : Cela mappe les floats dans un espace entier non signé 32 bits où l'ordre binaire correspond à l'ordre numérique. Après le tri, on inverse les bits.

### Performance
- **Complexité** : $O(N \times K)$ où $K$ est le nombre de bits. Puisque $K$ est constant (32), cela est effectivement **$O(N)$**.
- **Vitesse** : Pour 100 000 sphères, c'est significativement plus rapide que `qsort` car il évite le surcoût des callbacks de fonctions et du déréférencement de pointeurs pour chaque comparaison.

## 4. Tableau comparatif

| Métrique | CPU (qsort) | CPU (Radix) | GPU (Bitonique) |
| :--- | :--- | :--- | :--- |
| **Logique** | Basé sur la comparaison | Basé sur la distribution | Basé sur un réseau |
| **Complexité théorique** | $O(N \log N)$ | $O(N)$ | $O(N \log^2 N)$ |
| **Dépendances entre données** | Élevées | Aucune | Aucune |
| **Implémentation** | `qsort()` | 4 passes personnalisées | Compute GLSL |
| **Idéal pour** | < 1 000 éléments | 1 000 - 100 000 | > 10 000 ou données natives GPU |

## 5. Résumé

Nous utilisons **trois** chemins distincts :
1. **CPU Qsort** : Base solide, utilisée pour les tests et la référence.
2. **CPU Radix** : La « voie dorée » pour le tri haute performance lorsque les données sont déjà sur le CPU.
3. **GPU Bitonique** : La « voie de scalabilité » pour les grands nombres ou quand on veut laisser le CPU libre pour d'autres tâches.
