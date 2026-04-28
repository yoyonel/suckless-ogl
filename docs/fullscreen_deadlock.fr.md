# Résolution du blocage en plein écran

**Statut** : Résolu
**Plateformes affectées** : NVIDIA (confirmé) ; Intel non affecté

## Problème

Lors de la bascule en plein écran via la touche `F`, l'application se bloquait de manière aléatoire sur les GPU NVIDIA. L'application devenait non réactive, nécessitant une terminaison forcée.

## Analyse de la cause

Le blocage se produisait parce que `glfwSetWindowMonitor()` était appelé depuis le callback `framebuffer_size_callback` pendant qu'une opération GPU était en cours. Avec les pilotes NVIDIA, cela provoquait une condition de course entre :
1. Le thread GLFW gérant le changement de mode
2. Le pipeline GPU encore en train d'exécuter des commandes en attente

## Solution

La correction utilise un **modèle de redimensionnement différé** en deux parties :

### 1. Drapeaux de redimensionnement différé

Au lieu d'appeler directement `postprocess_resize` depuis le callback, on lève un drapeau :

```c
// Dans framebuffer_size_callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    App* app = glfwGetWindowUserPointer(window);
    app->win.resize_pending = 1;
    app->win.pending_width = width;
    app->win.pending_height = height;
}
```

### 2. Drainage GPU avant la bascule

Dans `app_toggle_fullscreen()`, on appelle maintenant `glFinish()` avant d'invoquer `glfwSetWindowMonitor()`. Cela garantit que le pipeline GPU est entièrement vidé et que toutes les commandes en attente (clôtures, transferts PBO, échanges) sont terminées avant que le pilote tente le changement de mode.

### 3. Traitement dans la boucle principale

Le travail lourd réel (`postprocess_resize`) est déplacé au début de l'image suivante dans `app_run`, hors du contexte de callback GLFW.

```c
// Dans la boucle while de app_run()
glfwPollEvents();

if (app->win.resize_pending) {
    postprocess_resize(&app->postprocess, app->win.pending_width, app->win.pending_height);
    app->win.resize_pending = 0;
}
```

## Tests de stress

Un test de stress dédié a été créé pour vérifier ce correctif : `scripts/test_stress_fullscreen.sh`.

### Fonctionnement
- Utilise `xdotool` pour envoyer des frappes 'F' rapidement (intervalles de 10 ms à 50 ms par exemple).
- Surveille la **sortie du journal** de l'application plutôt que la visibilité de la fenêtre. Une application bloquée peut avoir une fenêtre visible mais arrêtera de journaliser « Switched to fullscreen/windowed ».
- Si le message de journal attendu n'apparaît pas dans les 5 secondes, un blocage est détecté, une trace de pile GDB complète de tous les threads est capturée, et l'application est terminée.

### Exécution du test
```bash
# Test standard
just stress-fullscreen

# Test agressif avec ASan
just stress-fullscreen-asan 200 10
```

## Résumé des modifications

| Fichier | Modification |
| :--- | :--- |
| `include/app.h` | Ajout des champs `resize_pending`, `pending_width`, `pending_height`. |
| `src/app_input.c` | Mise à jour de `framebuffer_size_callback` pour utiliser des drapeaux ; ajout de `glFinish()` à la bascule. |
| `src/app.c` | Ajout de la logique de traitement du redimensionnement différé et de l'initialisation d'état. |
| `Justfile` | Ajout de l'automatisation `stress-fullscreen`. |
