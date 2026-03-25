# Contrôle de caméra à la souris

Ce document décrit l'implémentation du contrôle de caméra FPS (First Person Shooter) à la souris.

## Architecture

Le contrôle de caméra utilise les angles d'Euler (yaw/pitch) pour une implémentation simple et stable, sans gimbal lock dans la plage d'utilisation normale.

### Variables d'état

```c
typedef struct {
    float yaw;    // Rotation horizontale (axe Y) — en radians
    float pitch;  // Rotation verticale (axe X) — en radians, clampé
    float fov;    // Champ de vision — en degrés
} CameraState;
```

## Callback GLFW

```c
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    App* app = glfwGetWindowUserPointer(window);

    // Premier mouvement — initialiser les coordonnées précédentes
    if (app->camera.first_mouse) {
        app->camera.last_x = (float)xpos;
        app->camera.last_y = (float)ypos;
        app->camera.first_mouse = false;
        return;
    }

    float dx = (float)(xpos - app->camera.last_x) * MOUSE_SENSITIVITY;
    float dy = (float)(app->camera.last_y - ypos) * MOUSE_SENSITIVITY; // Inversé Y

    app->camera.last_x = (float)xpos;
    app->camera.last_y = (float)ypos;

    app->camera.yaw   += dx;
    app->camera.pitch += dy;

    // Limiter le pitch pour éviter le flippage
    app->camera.pitch = fclampf(app->camera.pitch, -89.0f * DEG_TO_RAD, 89.0f * DEG_TO_RAD);
}
```

## Calcul de la direction

À partir du yaw et du pitch, la direction de la caméra est recalculée :

```c
static inline vec3s camera_direction(float yaw, float pitch)
{
    return (vec3s){{
        cosf(yaw) * cosf(pitch),
        sinf(pitch),
        sinf(yaw) * cosf(pitch)
    }};
}
```

La matrice de vue est construite via `glms_lookat(position, position + direction, up)`.

## Sensibilité et réglages

| Paramètre | Valeur par défaut | Description |
|-----------|------------------|-------------|
| `MOUSE_SENSITIVITY` | `0.002` | Rapport pixel → radian |
| `PITCH_MAX` | `89°` | Limite d'inclinaison verticale |
| `FOV_MIN` | `10°` | Zoom maximum |
| `FOV_MAX` | `110°` | Grand angle maximum |

## Mode de capture souris

La caméra n'est active que lorsque le mode de capture souris est activé (touche `M` ou clic droit) :

```c
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
```

En mode `GLFW_CURSOR_DISABLED`, GLFW capture la souris et fournit des deltas illimités — essentiel pour une rotation fluide sans butée sur les bords de l'écran.

## Zoom via molette

```c
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    App* app = glfwGetWindowUserPointer(window);
    app->camera.fov -= (float)yoffset * SCROLL_SENSITIVITY;
    app->camera.fov = fclampf(app->camera.fov, FOV_MIN, FOV_MAX);
}
```

## Voir aussi

- [keyboard_system.md](./keyboard_system.md) — Système de raccourcis clavier
- [project_structure.md](./project_structure.md) — Contrôles de l'application
