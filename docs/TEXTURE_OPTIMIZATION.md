# Optimisation du Pipeline de Texture (Immutable Storage & Alignment)

Ce document détaille le refactoring du système d'upload de textures HDR pour utiliser l'approache **Immutable Storage** introduite par OpenGL 4.2.

## 1. Mutable vs Immutable Storage

### L'approche Classique (`glTexImage2D`)
Dans l'approche classique (mutable), chaque appel à `glTexImage2D` est une redéfinition potentielle de la texture :
- **Réallocations** : Le driver doit souvent libérer et réallouer la mémoire GPU à chaque upload.
- **Validation** : Le driver doit vérifier la cohérence de chaque niveau de mipmap individuellement à chaque rendu (**texture completeness**).

### L'approche Moderne (`glTexStorage2D`)
L'approche immuable sépare la définition de la structure de la texture de l'envoi des données :
- **Contrat de Fixité** : La taille, le format et le nombre de mipmaps sont fixés à la création.
- **Allocation Unique** : Le driver alloue tout le bloc mémoire en une fois, permettant des optimisations de placement physique des données (tiling/swizzling) optimales pour le matériel.
- **Zéro Overheads** : Comme la structure ne peut plus changer, les vérifications de cohérence lors du rendu sont quasi-inexistantes.

## 2. Détail de l'Implémentation

Le refactoring dans `src/texture.c` suit ce schéma :

```c
// 1. Définition de la structure immuable
int levels = (int)floor(log2(fmax(width, height))) + 1;
glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA16F, width, height);

// 2. Upload des données (pure copie)
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, data);
```

## 3. Importance de l'Alignement (RGBA vs RGB)

Lors du passage à l'immutable storage, nous avons constaté que l'upload HDR 4K prenait environ **76 ms** de temps CPU en format `RGB`. En forçant le format `RGBA` (4 canaux), ce temps est tombé à **37 ms**.

### Pourquoi ?
Les drivers (particulièrement Intel Iris Xe) préfèrent manipuler des blocs de mémoire alignés sur 4 composants :
- **RGB** : Nécessite souvent une conversion logicielle destructive (re-padding) par le CPU avant l'envoi au GPU pour s'aligner sur les bus mémoire.
- **RGBA** : Permet un transfert direct (DMA) sans aucune modification par le CPU.

## 4. Conclusion

L'utilisation combinée de `glTexStorage2D` et d'un alignement `RGBA` offre :
1. Un code plus robuste et plus facile à optimiser pour le driver.
2. Une réduction massive du "stutter" CPU lors du chargement des assets.
3. Une base technique compatible avec des techniques plus avancées comme le **Bindless Textures** ou les **PBO**.
