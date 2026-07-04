# Architecture de l'Intégration de Dear ImGui

Ce document décrit la conception, l'architecture et l'intégration de **Dear ImGui (v1.92.4)** dans le moteur C11 `suckless-ogl`.

## Aperçu Général

Dear ImGui fournit une interface utilisateur graphique (GUI) interactive en temps réel pour le diagnostic d'exécution, le réglage des paramètres, le profilage et l'inspection de la scène. Elle s'active avec la touche **`F2`**.

```
       [ Callback d'Entrée GLFW ]
                  │ (Touche F2 / Événements Souris / Défilement)
                  ▼
         [ Couche d'Entrée Applicative ]
                  │
                  │ (Délègue au Coordinateur Applicatif)
                  ▼
      [ Coordinateur de l'Application ] ──► [ Synchro Capture Curseur & Caméra ]
                  │
                  ▼
         [ Sous-système ImGui ] ──► [ Rendu des Panneaux & Inspecteur Pixels ]
```

---

## Principes d'Architecture (SRP & SoC)

Pour maintenir une base de code propre, l'intégration d'ImGui respecte strictement les principes **SRP (Single-Responsibility Principle)** et **SoC (Separation of Concerns)** :

1. **Découplage des Entrées (`src/app_input.c`)** :
   * Le système d'entrée est uniquement responsable de capter les touches GLFW physiques et de les propager.
   * Lorsque `F2` est pressé, le callback d'entrée appelle simplement `app_toggle_gui(app)`. Il ne modifie pas directement le mode de curseur, la caméra ou l'état interne d'ImGui.

2. **Coordinateur de Synchronisation d'État (`src/app.c`)** :
   * Les fonctions `app_set_gui_visible` et `app_toggle_gui` synchronisent les composants du moteur.
   * Lorsque l'interface graphique est visible, elle libère le curseur de la souris (`GLFW_CURSOR_NORMAL`), désactive les mouvements de la caméra, et émet un log de diagnostic.
   * Lorsqu'elle est masquée, elle capture à nouveau la souris (`GLFW_CURSOR_DISABLED`), réactive la caméra, et réinitialise l'état d'amortissement de la souris.

3. **Pont de Rendu ImGui (`src/gui.h` / `src/gui.cpp`)** :
   * Encapsulé derrière une interface C compatible (`extern "C"`).
   * Dessine les contrôles et affiche les textures de débogage sans polluer le compilateur C avec des types C++.

---

## Panneaux d'Interface

L'interface se décompose en plusieurs onglets :

* **Camera** : Réglage de la vitesse de déplacement, sensibilité, FOV, et paramètres d'oscillation (head-bobbing).
* **Scene** : Activation de la skybox, niveau de flou d'arrière-plan, subdivisions de l'icosphère et modes de tri.
* **Rendering** : Surveillance du taux d'anti-aliasing MSAA et ajustements de l'anti-aliasing spéculaire.
* **Post-FX** : Paramétrage en direct des effets Vignette, Exposition, Aberration Chromatique, Color Grading, Bloom, FXAA, Auto-Exposure, Depth of Field, et Brouillard.
* **Profiling** : Affiche les temps de traitement CPU/GPU dans un tableau synthétique.
* **Shaders** : État de compilation des variantes et de la mise en cache des shaders.
* **IBL Debug** : Rendu interactif de l'équirectangulaire d'origine, de l'Irradiance Map, de la Prefiltered Specular Map (avec choix des mipmaps pour simuler la rugosité) et de la LUT BRDF.
* **Compute Slicing** : Budget d'échantillonnage de l'IBL Progressive et limitations de découpage par frame.

---

## Inspecteur de Pixels (Click-to-Inspect)

L'inspecteur permet d'analyser les valeurs physiques d'une texture :
1. Un double-clic sur une prévisualisation de texture (ex: LUT BRDF, Irradiance) ouvre une vue agrandie.
2. Un clic dans cette vue capture la couleur du pixel cliqué.
3. Un Framebuffer Object (FBO) temporaire est relié, et `glReadPixels` récupère la couleur réelle codée en virgule flottante (`RGBA32F`) directement depuis la mémoire GPU.

---

## Diagnostics & Logs

Tous les événements d'ImGui sont enregistrés sous le module `"suckless-ogl.gui"` :
* **Démarrage** : Traces d'initialisation des backends GLFW/OpenGL3.
* **Activation** : Logs de changements de capture du curseur et d'activation des contrôles.
* **Fermeture** : Suivi de la libération des ressources.
