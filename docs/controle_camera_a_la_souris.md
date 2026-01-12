# Contrôle de Caméra à la Souris - Documentation Technique

## 🎯 Équivalence Python → C

### **Python (moderngl-window)**
```python
class CameraWindow(mglw.WindowConfig):
    def on_mouse_position_event(self, x: int, y: int, dx, dy):
        if self.camera_enabled:
            self.camera.rot_state(-dx, -dy)
    
    def on_key_event(self, key, action, modifiers):
        if key == keys.C and action == keys.ACTION_PRESS:
            self.camera_enabled = not self.camera_enabled
            self.wnd.mouse_exclusivity = self.camera_enabled
            self.wnd.cursor = not self.camera_enabled
```

### **C (GLFW)**
```c
static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    App* app = (App*)glfwGetWindowUserPointer(window);
    if (!app->camera_enabled) return;
    
    double dx = xpos - app->last_mouse_x;
    double dy = ypos - app->last_mouse_y;
    
    app->camera_yaw += (float)(-dx * MOUSE_SENSITIVITY);
    app->camera_pitch += (float)(-dy * MOUSE_SENSITIVITY);
}

static void key_callback(GLFWwindow* window, int key, ...)
{
    if (key == GLFW_KEY_C) {
        app->camera_enabled = !app->camera_enabled;
        glfwSetInputMode(window, GLFW_CURSOR, 
            app->camera_enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}
```

## 🏗️ Architecture de la Caméra

### **Coordonnées Sphériques**

La position de la caméra est calculée en coordonnées sphériques :

```c
cam_x = distance * cos(pitch) * sin(yaw)
cam_y = distance * sin(pitch)
cam_z = distance * cos(pitch) * cos(yaw)
```

**Paramètres** :
- **Yaw (θ)** : Rotation horizontale (azimut)
- **Pitch (φ)** : Rotation verticale (élévation)
- **Distance (r)** : Rayon de l'orbite

### **Schéma Conceptuel**

```
                    Y (up)
                    |
                    |  pitch
                    | /
                    |/_____ X
                   /|
                  / |
                 /  |
              Z /   |
             (forward)
```

## 🖱️ Gestion de la Souris

### **Callbacks GLFW**

```c
/* Setup dans app_init() */
glfwSetCursorPosCallback(app->window, mouse_callback);
glfwSetScrollCallback(app->window, scroll_callback);

/* Mode de capture initial */
if (app->camera_enabled) {
    glfwSetInputMode(app->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}
```

### **État de la Souris**

```c
typedef struct {
    int camera_enabled;     /* Camera control active */
    int first_mouse;        /* First movement flag */
    double last_mouse_x;    /* Previous X position */
    double last_mouse_y;    /* Previous Y position */
    float camera_yaw;       /* Horizontal angle */
    float camera_pitch;     /* Vertical angle */
} App;
```

## 🔄 Flux de Traitement

### **1. Mouvement de Souris**

```c
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // 1. Vérifier si le contrôle est activé
    if (!app->camera_enabled) return;
    
    // 2. Gérer le premier mouvement (évite le saut)
    if (app->first_mouse) {
        app->last_mouse_x = xpos;
        app->last_mouse_y = ypos;
        app->first_mouse = 0;
        return;
    }
    
    // 3. Calculer le delta
    double dx = xpos - app->last_mouse_x;
    double dy = ypos - app->last_mouse_y;
    
    // 4. Mettre à jour la position précédente
    app->last_mouse_x = xpos;
    app->last_mouse_y = ypos;
    
    // 5. Appliquer la sensibilité et mettre à jour l'orientation
    app->camera_yaw += (float)(-dx * MOUSE_SENSITIVITY);
    app->camera_pitch += (float)(-dy * MOUSE_SENSITIVITY);
    
    // 6. Limiter le pitch (éviter gimbal lock)
    app->camera_pitch = clamp(app->camera_pitch, MIN_PITCH, MAX_PITCH);
}
```

### **2. Molette de Souris (Zoom)**

```c
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Ajuster la distance avec la molette
    app->camera_distance -= (float)yoffset * 0.2f;
    
    // Limiter la distance
    app->camera_distance = clamp(app->camera_distance, 1.5f, 10.0f);
}
```

### **3. Toggle Contrôle (Touche C)**

```c
case GLFW_KEY_C:
    app->camera_enabled = !app->camera_enabled;
    
    if (app->camera_enabled) {
        // Activer: cacher et capturer le curseur
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        app->first_mouse = 1; // Reset pour transition douce
    } else {
        // Désactiver: montrer le curseur
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    break;
```

## 🎮 Modes de Curseur GLFW

### **GLFW_CURSOR_DISABLED**
- Curseur invisible
- Mouvement infini (pas de limites d'écran)
- Position virtuelle continue
- **Idéal pour contrôle FPS/orbital**

### **GLFW_CURSOR_NORMAL**
- Curseur visible et normal
- Confiné à la fenêtre
- **Pour interaction UI**

## 🔒 Gestion du Gimbal Lock

### **Problème**

Sans limitation, la caméra peut atteindre le sommet (pitch = ±90°) et perdre sa référence "up", causant des rotations erratiques.

### **Solution**

```c
#define MIN_PITCH -1.5f  /* ~-86° */
#define MAX_PITCH 1.5f   /* ~+86° */

if (app->camera_pitch > MAX_PITCH)
    app->camera_pitch = MAX_PITCH;
if (app->camera_pitch < MIN_PITCH)
    app->camera_pitch = MIN_PITCH;
```

## 📊 Sensibilité de la Souris

```c
#define MOUSE_SENSITIVITY 0.002f
```

**Ajustement** :
- **Plus petit** (0.001) : Mouvement plus lent, précis
- **Plus grand** (0.005) : Mouvement plus rapide, sensible

## 🎯 First Mouse Movement

### **Problème**

Au premier mouvement après activation, `last_mouse_x/y` pourrait être n'importe où, causant un saut brutal de caméra.

### **Solution**

```c
if (app->first_mouse) {
    app->last_mouse_x = xpos;
    app->last_mouse_y = ypos;
    app->first_mouse = 0;
    return;  // Ignore ce frame
}
```

**Réinitialisé** :
- À l'initialisation
- Après toggle camera_enabled (transition douce)

## 🔄 Calcul de Position

```c
void app_render(App* app)
{
    // Convertir angles → position cartésienne
    float cam_x = app->camera_distance * cosf(app->camera_pitch) * sinf(app->camera_yaw);
    float cam_y = app->camera_distance * sinf(app->camera_pitch);
    float cam_z = app->camera_distance * cosf(app->camera_pitch) * cosf(app->camera_yaw);
    
    vec3 camera_pos = {cam_x, cam_y, cam_z};
    vec3 target = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    
    glm_lookat(camera_pos, target, up, view);
}
```

## ✅ Checklist d'Implémentation

- [x] State de la caméra (yaw, pitch, distance)
- [x] Callbacks GLFW (cursor, scroll)
- [x] Toggle activation/désactivation
- [x] Mode curseur (DISABLED/NORMAL)
- [x] First mouse handling
- [x] Pitch clamping (gimbal lock)
- [x] Distance clamping
- [x] Sensibilité souris configurable
- [x] Reset caméra (SPACE)
- [x] Feedback console (printf)

## 🎨 Expérience Utilisateur

### **Flow Typique**

1. **Démarrage** : Contrôle activé, curseur caché
2. **Explorer** : Bouger souris pour regarder autour
3. **Zoomer** : Molette pour ajuster distance
4. **Pause** : Appuyer **C** pour libérer curseur
5. **Reprendre** : Appuyer **C** pour réactiver
6. **Reset** : **SPACE** si perdu

### **Feedback Visuel**

```c
printf("Camera control: %s\n", 
    app->camera_enabled ? "ENABLED" : "DISABLED");
printf("Camera reset\n");
```

## 🔧 Paramètres Ajustables

```c
/* Sensibilité */
#define MOUSE_SENSITIVITY 0.002f  // Vitesse rotation

/* Limites Pitch */
#define MIN_PITCH -1.5f  // Limite basse
#define MAX_PITCH 1.5f   // Limite haute

/* Limites Distance */
#define MIN_DISTANCE 1.5f
#define MAX_DISTANCE 10.0f

/* Vitesse Zoom */
#define ZOOM_SPEED 0.2f  // Dans scroll_callback
```

## 🚀 Performance

- **Callbacks légers** : Calculs minimaux
- **Pas de recherche** : Accès direct via user pointer
- **Clamp efficace** : Comparaisons simples
- **Pas d'allocation** : Tout en stack/struct

## 🎓 Concepts Clés

1. **Coordonnées sphériques** : Naturelles pour caméra orbitale
2. **Delta mouse** : Mouvement relatif, pas absolu
3. **Cursor disabled** : Mode "infini" pour contrôle continu
4. **First mouse** : Évite sauts initiaux
5. **Gimbal lock prevention** : Clamp pitch
6. **User pointer** : Lien window ↔ app state
