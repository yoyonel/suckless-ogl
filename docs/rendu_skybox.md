# Technique de## Rendu de la Skybox (Equirectangular)

Le projet utilise un mapping **Equirectangulaire** direct pour l'environnement. C'est plus efficace en termes de mémoire car cela évite de stocker un cubemap généré.

### Optimisation Early-Z
Pour maximiser les performances sur les GPU intégrés, la skybox est rendue **après** les objets de la scène.

1.  **Vertex Shader** : Le shader positionne les triangles de la skybox exactement sur le plan lointain (`z = 1.0`).
2.  **Depth Test** : En utilisant `glDepthFunc(GL_LEQUAL)`, le GPU rejette automatiquement les fragments de la skybox masqués par les objets 3D (comme l'icosphère) avant de lancer le shader de fragment.
3.  **Fragment Shader** : Le shader effectue une projection sphérique inverse pour sampler la texture HDR 2D.

```glsl
// Projection Equirectangulaire Inverse
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleEquirectangular(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv.x += 0.5;
    uv.y = 0.5 - uv.y;
    return uv;
}
```
 */
    uv.y = 0.5 - uv.y;
    return uv;
}

void main() {
    vec3 dir = normalize(v_direction);
    vec2 uv = SampleEquirectangular(dir);
    FragColor = textureLod(environmentMap, uv, blur_lod);
}
```

### **Implémentation en C (Matrice de Vue)**

On retire la composante de **translation** de la matrice de vue :

```c
/* Copier la vue et retirer la translation */
mat4 view_sky;
glm_mat4_copy(view, view_sky);
view_sky[3][0] = 0.0f;
view_sky[3][1] = 0.0f;
view_sky[3][2] = 0.0f;

/* Calculer l'inverse view-projection */
mat4 inv_vp_sky;
glm_mat4_mul(proj, view_sky, inv_vp_sky);
glm_mat4_inv(inv_vp_sky, inv_vp_sky);
```

## 🔍 Détails Techniques

### **Échantillonnage avec Mipmaps**

L'utilisation de `textureLod` avec une texture equirectangulaire permet un contrôle précis du flou :
- **LOD 0** : Environnement net.
- **LOD > 0** : Environnement flouté (utile pour le PBR ou le debug).

### **Correction d'Orientation**

L'inversion `uv.y = 0.5 - uv.y` est cruciale pour que le "haut" de l'image HDR corresponde au "haut" dans l'espace 3D.

## 🎨 Workflow Complet

```c
void render_scene(App* app) {
    // 1. Matrice de vue sans translation
    mat4 view_sky;
    glm_mat4_copy(app->view, view_sky);
    view_sky[3][0] = 0.0f;
    view_sky[3][1] = 0.0f;
    view_sky[3][2] = 0.0f;
    
    mat4 inv_vp_sky;
    glm_mat4_mul(app->proj, view_sky, inv_vp_sky);
    glm_mat4_inv(inv_vp_sky, inv_vp_sky);
    
    // 2. Rendu via le module skybox
    skybox_render(&app->skybox, app->skybox_shader, 
                  app->hdr_texture, inv_vp_sky, app->env_lod);
}
```

## 🌟 Avantages de cette Technique

1. **Performance** : Pas de calcul complexe, juste mettre à zéro 3 valeurs
2. **Simplicité** : Facile à comprendre et maintenir
3. **Robustesse** : Technique standard utilisée dans l'industrie
4. **Qualité** : Effet visuel parfait d'infini

## 📝 Notes Importantes

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
