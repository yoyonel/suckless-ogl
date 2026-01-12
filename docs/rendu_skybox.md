# Technique de Rendu Skybox

## 🎯 Problème à Résoudre

Une skybox doit toujours apparaître **infiniment lointaine**, peu importe la position de la caméra. Si on utilise la matrice de vue complète (avec translation), la skybox se déplace avec la caméra, créant un effet de proximité indésirable.

## ✨ Solution : Retirer la Translation

### **Principe**

On retire la composante de **translation** de la matrice de vue avant de calculer la matrice inverse view-projection pour la skybox. Cela permet :

1. La skybox **ne se déplace pas** avec la caméra
2. La skybox **tourne** avec la rotation de la caméra
3. L'illusion d'un environnement **infiniment distant**

### **Implémentation en C**

```c
/* 1. Créer la matrice de vue normale */
mat4 view;
glm_lookat(camera_pos, target, up, view);

/* 2. Copier la vue et retirer la translation */
mat4 view_no_translation;
glm_mat4_copy(view, view_no_translation);

/* Retirer la translation (dernière colonne des 3 premières lignes) */
view_no_translation[3][0] = 0.0f;  // X
view_no_translation[3][1] = 0.0f;  // Y
view_no_translation[3][2] = 0.0f;  // Z
/* view_no_translation[3][3] reste 1.0f */

/* 3. Calculer l'inverse view-projection pour la skybox */
mat4 inv_view_proj;
glm_mat4_mul(proj, view_no_translation, inv_view_proj);
glm_mat4_inv(inv_view_proj, inv_view_proj);
```

## 🔍 Détails Techniques

### **Structure d'une Matrice 4x4**

En OpenGL (column-major), une matrice de transformation est structurée ainsi :

```
[  Xx   Yx   Zx   Tx  ]
[  Xy   Yy   Zy   Ty  ]
[  Xz   Yz   Zz   Tz  ]
[  0    0    0    1   ]
```

Où :
- **X, Y, Z** : Vecteurs de rotation (3x3)
- **T (Tx, Ty, Tz)** : Vecteur de translation (dernière colonne)

### **Accès en cglm**

```c
mat4[3][0]  // Tx - Translation X
mat4[3][1]  // Ty - Translation Y
mat4[3][2]  // Tz - Translation Z
mat4[3][3]  // Toujours 1.0 (coordonnée homogène)
```

### **Pourquoi ça fonctionne ?**

1. **Sans translation** : La caméra est conceptuellement à l'origine (0,0,0)
2. **Avec rotation** : L'orientation de la caméra est préservée
3. **Résultat** : La skybox tourne mais ne se déplace pas

## 📊 Comparaison

### **Avec Translation (❌ Incorrect)**

```c
// Matrice de vue complète
glm_lookat(camera_pos, target, up, view);
glm_mat4_mul(proj, view, view_proj);
glm_mat4_inv(view_proj, inv_view_proj);

// ❌ Problème : la skybox se déplace avec la caméra
// ❌ Elle semble proche et finie
```

### **Sans Translation (✅ Correct)**

```c
// Retirer la translation
view[3][0] = 0.0f;
view[3][1] = 0.0f;
view[3][2] = 0.0f;

glm_mat4_mul(proj, view, view_proj);
glm_mat4_inv(view_proj, inv_view_proj);

// ✅ La skybox reste infiniment lointaine
// ✅ Elle tourne avec la caméra
```

## 🎨 Workflow Complet

```c
void render_scene() {
    // 1. Setup caméra
    mat4 view, proj;
    glm_lookat(cam_pos, target, up, view);
    glm_perspective(fov, aspect, near, far, proj);
    
    // 2. Pour la skybox : vue sans translation
    mat4 view_sky;
    glm_mat4_copy(view, view_sky);
    view_sky[3][0] = 0.0f;
    view_sky[3][1] = 0.0f;
    view_sky[3][2] = 0.0f;
    
    mat4 inv_vp_sky;
    glm_mat4_mul(proj, view_sky, inv_vp_sky);
    glm_mat4_inv(inv_vp_sky, inv_vp_sky);
    
    // 3. Render skybox d'abord
    render_skybox(inv_vp_sky);
    
    // 4. Pour les objets : vue complète (avec translation)
    mat4 view_proj;
    glm_mat4_mul(proj, view, view_proj);
    
    render_objects(view_proj);
}
```

## 🌟 Avantages de cette Technique

1. **Performance** : Pas de calcul complexe, juste mettre à zéro 3 valeurs
2. **Simplicité** : Facile à comprendre et maintenir
3. **Robustesse** : Technique standard utilisée dans l'industrie
4. **Qualité** : Effet visuel parfait d'infini

## 📝 Notes Importantes

- La skybox doit être rendue **avant** les objets (ou avec `GL_LEQUAL`)
- Utiliser `glDepthFunc(GL_LEQUAL)` pour que la skybox soit au fond
- La skybox n'écrit pas de profondeur significative
- Le LOD (blur_lod) permet de contrôler le flou de l'environnement

## 🔗 Équivalence Python → C

### Python (moderngl)
```python
view = camera.matrix
view[3][0] = 0
view[3][1] = 0
view[3][2] = 0
inv_view_proj = glm.inverse(projection * view)
```

### C (cglm)
```c
mat4 view;
glm_lookat(camera_pos, target, up, view);
view[3][0] = 0.0f;
view[3][1] = 0.0f;
view[3][2] = 0.0f;

mat4 inv_view_proj;
glm_mat4_mul(proj, view, inv_view_proj);
glm_mat4_inv(inv_view_proj, inv_view_proj);
```

**Parfaitement équivalent !** ✅
