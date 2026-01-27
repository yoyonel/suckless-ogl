# Guide Utilisateur : RenderDoc sur Debian 13 (Intel Iris Xe)

Ce guide explique comment installer et utiliser RenderDoc pour profiler et vérifier les performances GPU de `suckless-ogl`.

## 1. Installation sur Debian 13

Le paquet `renderdoc` a été supprimé des dépôts Debian Testing (Trixie). La méthode la plus fiable (et la plus à jour) consiste à utiliser le binaire officiel :

1.  **Téléchargement** : Allez sur [renderdoc.org](https://renderdoc.org/builds) et téléchargez la dernière version stable pour **Linux 64-bit**.
2.  **Extraction** :
    ```bash
    tar -xvf renderdoc_*.tar.gz
    cd renderdoc_*
    ```
3.  **Lancement** :
    ```bash
    ./bin/qrenderdoc
    ```
    *(Optionnel) Vous pouvez ajouter le dossier `bin` à votre PATH ou créer un lien symbolique vers `/usr/local/bin/qrenderdoc`.*

## 2. Configuration du Profiling

1.  Lancez l'interface graphique : `qrenderdoc`.
2.  Allez dans l'onglet **"Launch Application"**.
3.  Configurez les chemins :
    *   **Executable Path** : `/home/latty/Prog/__PERSO__/suckless-ogl/build-small/app`
    *   **Working Directory** : `/home/latty/Prog/__PERSO__/suckless-ogl`
4.  Cochez les options suivantes (recommandé pour le profiling) :
    *   `Capture Child Processes`
    *   `Ref All Resources` (utile pour voir toutes les textures HDR même si non bindées sur le moment)

## 3. Capturer un événement de chargement

Le chargement de l'environnement est asynchrone et se déclenche via les touches `Page Up` / `Page Down`.

1.  Cliquez sur **"Launch"** dans RenderDoc.
2.  Dans votre application, préparez-vous à changer d'environnement.
3.  Appuyez sur **F12** (ou `Print Screen`) dans l'application pour capturer une frame.
    *   *Note : Comme le chargement IBL prend plusieurs frames (~500ms), vous devrez peut-être faire plusieurs captures successives pour tomber sur la frame exacte où les Compute Shaders tournent.*
4.  Une miniature apparaît dans RenderDoc. Double-cliquez dessus pour l'ouvrir.

## 4. Analyse des performances (Ground Truth)

Une fois la capture ouverte :

1.  Ouvrez la fenêtre **"Event Browser"** (`Window` -> `Event Browser`).
2.  Cherchez les appels `glDispatchCompute`. Ils correspondent à votre IBL (Luminance, Specular, Irradiance).
3.  Cliquez sur l'icône **Horloge** (Time durations) en haut de l'Event Browser.
    *   RenderDoc va re-jouer la frame plusieurs fois pour obtenir une mesure précise du GPU.
4.  **Vérification** : Comparez la valeur dans la colonne `Duration` avec vos logs `perf.hybrid`.
    *   Si RenderDoc indique `325,450 us` (microsecondes), cela correspond à `325.45 ms`.

## 5. Astuces spécifiques Intel / Mesa

*   **Détails du pipeline** : Dans l'onglet **"Pipeline State"**, vous pouvez voir exactement quel shader est utilisé, les "Dispatch Thread Groups" et les textures bindées.
*   **Visualisation HDR** : Dans le **"Texture Viewer"**, vous pouvez inspecter vos textures `RGBA16F`. Utilisez le curseur d'exposition en haut de la fenêtre pour "voir" les détails dans les zones très lumineuses.
*   **Debug Shaders** : Vous pouvez cliquer sur "Edit" sur un shader dans RenderDoc, modifier une formule, et "Rafraîchir" pour voir l'impact visuel et de performance instantanément sans recompiler votre projet C.
