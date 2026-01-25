# Architecture UBO pour le Post-Processing

Ce document détaille l'implémentation du **Uniform Buffer Object (UBO)** utilisé pour gérer les paramètres de post-traitement dans le moteur. Cette transition depuis les uniformes individuels (`glUniform*`) a été réalisée pour améliorer les performances CPU et la clarté du code.

## 1. Vue d'Ensemble

Au lieu d'envoyer des dizaines d'uniformes (flottants, entiers) un par un à chaque frame, nous regroupons toutes les données de configuration (Vignette, Bloom, Exposition, etc.) dans une seule structure mémoire contiguë.

- **Côté C** : `struct PostProcessUBO` (fichier `include/postprocess.h`)
- **Côté GPU** : `program Block` avec layout `std140` (fichier `shaders/postprocess/ubo.glsl`)
- **Transfert** : Un seul appel `glBufferSubData` par frame.

## 2. Contraintes Critiques : Layout std140

Le layout `std140` impose des règles d'alignement strictes en mémoire GPU. Pour garantir que le driver lise les données correctement, la structure C doit mimer **à l'octet près** cet alignement.

### Règles d'Alignement std140 (Simplifiées)
1. **Scalaires (float, int, bool)** : Alignement de base N (4 octets).
2. **Vecteurs (vec2)** : Alignement 2N (8 octets).
3. **Vecteurs (vec3, vec4)** : Alignement 4N (16 octets).
4. **Tableaux / Structures** : Alignement arrondi à 16 octets (taille d'un vec4).
5. **Padding** : Il n'y a **pas** de padding implicite entre les scalaires, sauf si nécessaire pour respecter l'alignement du membre suivant.

### Le Piège du Padding "Array vs Scalar"
C'est ici qu'une erreur subtile peut survenir :
- Si on déclare `float padding[3]` en GLSL, `std140` traite cela comme un tableau. **Tout élément de tableau doit être aligné sur 16 octets**. Donc `padding[0]` prend 16 octets (4 utiles + 12 vides), `padding[1]` prend 16 octets, etc.
- **Solution** : Utiliser des champs scalaires individuels pour le padding GLSL (`float _pad1; float _pad2; ...`) afin qu'ils soient packés de manière compacte (4 octets chacun), correspondant exactement au `float padding[3]` du C (qui est packé).

## 3. Structure des Données

### Structure C (`postprocess.h`)
Nous utilisons un padding explicite pour aligner les blocs logiques sur 16 octets, facilitant la lecture mémoire et le débogage.

```c
typedef struct {
    uint32_t active_effects; // 4 octets
    float time;              // 4 octets
    float _pad0[2];          // 8 octets (Padding pour atteindre 16 octets)

    /* Vignette (16 octets) */
    float vignette_intensity;
    float vignette_smoothness;
    float vignette_roundness;
    float _pad1;             // 4 octets

    // ... suite des effets
} PostProcessUBO;
```

### Bloc GLSL (`ubo.glsl`)
Le bloc définit le layout `std140` et binding 0.

> **IMPORTANT** : Les champs de padding doivent être déclarés individuellement !

```glsl
layout(std140, binding = 0) uniform PostProcessBlock {
    uint activeEffects;
    float time;
    float _pad0_0; // Correspond à _pad0[0] en C
    float _pad0_1; // Correspond à _pad0[1] en C

    /* Vignette */
    float v_intensity;
    float v_smoothness;
    float v_roundness;
    float _pad1;

    // ... suite des effets
};
```

## 4. Ajouter un Nouveau Paramètre

Pour ajouter un effet ou un paramètre, suivez scrupuleusement ces étapes :

1. **Ajout dans la struct C (`postprocess.h`)** :
   - Ajoutez le champ `float mon_param`.
   - Ajustez le tableau de padding `_padX` pour que la taille totale du bloc reste un multiple de 16 octets (facultatif mais recommandé pour l'organisation).

2. **Ajout dans le bloc GLSL (`ubo.glsl`)** :
   - Ajoutez le champ `float mon_param`.
   - **Crucial :** Mettez à jour les champs de padding scalaires (`_padX_0`, etc.) pour correspondre *exactement* aux offsets C.

3. **Utilisation dans les shaders** :
   - Le fichier `ubo.glsl` est inclus via `@header "postprocess/ubo.glsl"`.
   - Accédez directement à `mon_param` (le bloc rend les membres globaux).
   - Pour les flags booléens, ajoutez une macro dans `ubo.glsl` :
     ```glsl
     #define enableMonEffet ((activeEffects & (1u << N)) != 0u)
     ```

## 5. Performance

L'utilisation de l'UBO a permis de supprimer les appels suivants à chaque frame :
- `glUseProgram` (pour les uploads)
- ~30-50 appels à `glUniform1f` / `glUniform1i`
- La validation driver associée à chaque appel.

Désormais, `postprocess_end()` effectue :
1. Copie des données dans la struct locale (sur la pile).
2. **Un seul appel** `glBufferSubData`.
