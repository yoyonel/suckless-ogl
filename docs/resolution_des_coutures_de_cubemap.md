# Résolution des Coutures de Cubemap

## 🔍 Problème Identifié

Les bords du cubemap sont visibles sous forme de lignes ou d'artefacts. Cela peut être causé par plusieurs facteurs :

1. **LOD trop élevé** : Un blur_lod élevé (4.0) utilise des niveaux de mipmap bas résolution
2. **Résolution insuffisante** : 512x512 peut être trop petit
3. **Filtering sans seamless** : Les transitions entre faces ne sont pas lissées
4. **Échantillonnage aux bords** : Interpolation entre les faces mal gérée

## 🏁 Solution Définitive : Mapping Equirectangulaire

Bien que les solutions précédentes (Seamless Cubemap, Résolution augmentée) améliorent la situation, la solution la plus robuste pour ce projet a été de **supprimer totalement l'étape de conversion en cubemap**.

### **Pourquoi ?**

1. **Plus de faces** : Une texture equirectangulaire est un seul rectangle 2D continu. Il n'y a plus de "bords de faces" où les coutures peuvent apparaître.
2. **Pipelines simplifiés** : On passe directement de l'image HDR (panoramique) au rendu, sans passer par un compute shader de conversion.
3. **Moins de mémoire** : Pas besoin d'allouer une texture de cubemap supplémentaire.
4. **Qualité maximale** : On échantillonne directement les données d'origine.

### **Comparaison Cubemap vs Equirectangulaire**

| Caractéristique | Cubemap (Ancien) | Equirectangulaire (Actuel) |
|-----------------|------------------|---------------------------|
| Coutures | Possibles aux bords | **Impossibles** |
| Complexité | Élevée (Compute Shader) | **Faible** (Direct) |
| Artefacts | Mipmapping aux coins | **Nuls** (Linéaire continu) |
| Flexibilité | Standard industry | Idéal pour visualiseurs HDR |

### **Implémentation Logicielle**

Le passage à l'equirectangulaire a permis de supprimer :
- Le compute shader `equirect2cube.glsl`.
- Les fonctions `texture_create_env_cubemap` et `texture_build_env_cubemap`.
- La complexité de gestion des 6 faces.

### **Conclusion**

Pour un rendu de skybox où la fidélité de l'image HDR source est primordiale, le mapping equirectangulaire direct est la solution la plus "suckless" : moins de code, plus de qualité.
