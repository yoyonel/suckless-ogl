# Résolution des Coutures de Cubemap

## 🔍 Problème Identifié

Les bords du cubemap sont visibles sous forme de lignes ou d'artefacts. Cela peut être causé par plusieurs facteurs :

1. **LOD trop élevé** : Un blur_lod élevé (4.0) utilise des niveaux de mipmap bas résolution
2. **Résolution insuffisante** : 512x512 peut être trop petit
3. **Filtering sans seamless** : Les transitions entre faces ne sont pas lissées
4. **Échantillonnage aux bords** : Interpolation entre les faces mal gérée

## ✅ Solutions Implémentées

### **1. Réduction du LOD (Blur)**

```c
/* Avant: blur_lod = 4.0 (très flou, utilise mipmaps bas niveau) */
skybox_render(&app->skybox, app->skybox_shader, 
             app->env_cubemap, inv_view_proj, 4.0f);

/* Après: blur_lod = 0.0 (net, utilise niveau 0 de mipmap) */
skybox_render(&app->skybox, app->skybox_shader, 
             app->env_cubemap, inv_view_proj, 0.0f);
```

**Effet** : Utilise la résolution maximale, élimine le flou et les artefacts de mipmap.

### **2. Augmentation de la Résolution**

```c
/* Avant */
#define CUBEMAP_SIZE 512

/* Après */
#define CUBEMAP_SIZE 1024
```

**Effet** : Plus de détails, moins d'artefacts de pixelisation aux bords.

**Trade-off** :
- ✅ Meilleure qualité visuelle
- ⚠️ Plus de mémoire GPU (6 faces × 1024² × 4 channels × 2 bytes = ~50MB)
- ⚠️ Génération plus lente (compute shader)

### **3. Activation du Seamless Cubemap**

```c
/* Dans texture_create_env_cubemap() */
glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
```

**Ce que ça fait** :
- Active l'interpolation **entre les faces** du cubemap
- OpenGL interpole automatiquement les texels aux bords adjacents
- Élimine les discontinuités visuelles
- Feature OpenGL 3.2+ (Core Profile)

### **4. Paramètres de Filtering Optimaux**

```c
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
```

**Explication** :
- `GL_LINEAR` : Interpolation bilinéaire douce
- `GL_CLAMP_TO_EDGE` : Évite le wrapping aux bords (important pour cubemaps)

## 🎨 Options Supplémentaires

### **Option A : Mipmaps avec Filtrage Anisotrope**

Si vous voulez garder les mipmaps pour la performance :

```c
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, 
               GL_LINEAR_MIPMAP_LINEAR);
glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);
```

### **Option B : Résolution Adaptative**

Pour équilibrer qualité/performance :

```c
/* Haute qualité */
#define CUBEMAP_SIZE 2048

/* Moyenne qualité */
#define CUBEMAP_SIZE 1024

/* Basse qualité (mobile) */
#define CUBEMAP_SIZE 512
```

### **Option C : LOD Dynamique**

Ajouter un contrôle utilisateur pour ajuster le blur :

```c
/* Dans key_callback */
case GLFW_KEY_KP_ADD:
    app->skybox_lod = fminf(app->skybox_lod + 0.5f, 8.0f);
    break;
case GLFW_KEY_KP_SUBTRACT:
    app->skybox_lod = fmaxf(app->skybox_lod - 0.5f, 0.0f);
    break;
```

## 🔬 Comprendre le LOD (Level of Detail)

Le paramètre `blur_lod` dans le shader contrôle quel niveau de mipmap est échantillonné :

```glsl
// Dans le fragment shader de la skybox
vec3 color = textureLod(environmentMap, direction, blur_lod).rgb;
```

**Niveaux de Mipmap** :
- **LOD 0** : Résolution complète (1024×1024)
- **LOD 1** : 512×512
- **LOD 2** : 256×256
- **LOD 3** : 128×128
- **LOD 4** : 64×64
- ...

**LOD 4.0 = 64×64 pixels par face** → Très flou, artefacts visibles !

## 📊 Comparaison Visuelle

| Configuration | Qualité | Performance | Mémoire |
|---------------|---------|-------------|---------|
| 512px, LOD 4.0 | ⭐ Mauvaise (coutures) | ⭐⭐⭐ Excellente | ⭐⭐⭐ Faible |
| 512px, LOD 0.0 | ⭐⭐ Correcte | ⭐⭐⭐ Excellente | ⭐⭐⭐ Faible |
| 1024px, LOD 0.0 | ⭐⭐⭐ Bonne | ⭐⭐ Bonne | ⭐⭐ Moyenne |
| 2048px, LOD 0.0 | ⭐⭐⭐⭐ Excellente | ⭐ Correcte | ⭐ Élevée |

## 🛠️ Debugging

### **Vérifier les Coutures**

Pour tester si les coutures sont visibles :

```c
/* Activer le wireframe temporairement */
glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
```

### **Visualiser les Niveaux de Mipmap**

Ajoutez cette option pour débugger :

```c
/* Dans le shader */
vec3 color = textureLod(environmentMap, direction, float(debugLevel)).rgb;
```

### **Inspecter le Cubemap**

Vous pouvez sauvegarder les faces pour inspection :

```c
for (int face = 0; face < 6; face++) {
    float* data = malloc(size * size * 4 * sizeof(float));
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 
                  GL_RGBA, GL_FLOAT, data);
    // Sauvegarder en image pour inspection
    free(data);
}
```

## 🎯 Résultat Attendu

Après ces corrections :
- ✅ Pas de lignes visibles aux bords
- ✅ Transitions douces entre les faces
- ✅ Image nette et détaillée
- ✅ Skybox uniforme sans artefacts

## 💡 Recommandation Finale

**Configuration Optimale** :
```c
#define CUBEMAP_SIZE 1024      // Bon équilibre
blur_lod = 0.0f                // Net, pas de flou
GL_TEXTURE_CUBE_MAP_SEAMLESS   // Activé
```

**Pour des effets artistiques** :
- Augmentez progressivement le `blur_lod` pour un effet de profondeur de champ
- Utilisez 2048px pour des rendus photoréalistes
- Gardez 512px pour du prototypage rapide
