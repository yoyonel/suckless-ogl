# Système d'aide clavier et de raccourcis

Ce document décrit la superposition d'aide clavier interactive et le système sous-jacent `AppBindingRegistry`.

## Vue d'ensemble

Le système d'aide fournit une visualisation moderne et interactive des raccourcis clavier de l'application. Il se compose de deux parties principales :

1. **`AppBindingRegistry`** : Une structure de données centralisée stockant toutes les métadonnées des touches (touche, modificateurs, catégorie, description).
2. **Interface d'aide** : Un rendu dynamique de disposition clavier qui utilise le registre pour mettre en évidence les touches actives et afficher leurs fonctionnalités. Il présente une esthétique Cyberpunk utilisant des PNG pré-rendus et des effets SDF (Signed Distance Field) procéduraux.

## Conception visuelle et finitions

La superposition d'aide utilise une approche de rendu hybride avancée définie dans `src/app_ui.c` et `shaders/ui.frag` :

1. **Textures Cyberpunk** : Le cadre du panneau UI de base et les touches individuelles sont rendus en utilisant des textures PNG haute qualité (`kbd_panel_frame.png` et `kbd_key_base.png`). Les touches non assignées restent discrètes en arrière-plan, tandis que les touches assignées sont teintées via le shader selon leur type de contrainte (Bascule, Cycle, Combinaison).
2. **Neon Bloom procédural** : Lorsqu'une touche est pressée, une bordure néon SDF intense et procédurale est dessinée directement par `ui.frag` (Mode `5.0`). L'intensité de la lueur suit une décroissance exponentielle mathématiquement précise pour un aspect lumineux et net.
3. **Mise à l'échelle réactive de l'interface** : Toute la superposition se met à l'échelle dynamiquement en fonction de la hauteur de la fenêtre. Un facteur `ui_scale` est calculé par rapport à une résolution de référence (1080p), garantissant que la superposition reste parfaitement proportionnée et lisible sur n'importe quelle taille d'écran (de 720p à 4K).
4. **Rendu de texte scalable** : Utilise `ui_draw_text_scaled()` pour rendre les glyphes Freetype avec mise à l'échelle des métriques à la volée (largeur, hauteur, avance et décalages). Cela permet au texte de se mettre à l'échelle parfaitement avec les formes UI sans nécessiter plusieurs passes de rastérisation de police.
5. **Atténuation Spotlight et décroissance au survol** : Activer ou survoler une touche atténue progressivement le reste du clavier. Pour éviter le scintillement lorsque la souris passe sur les espaces entre les touches, un minuteur de **décroissance au survol de 150 ms** maintient l'état atténué pour une expérience visuelle fluide.
6. **Mise en évidence intelligente des modificateurs** : La superposition interroge activement le registre des raccourcis pour déterminer si une combinaison de touches dépend d'un modificateur donné (ex. : `Shift`). Appuyer sur des touches modificateurs seuls, ou associées négligemment à des touches non-modificateurs, n'allumera pas artificiellement la touche Shift sauf si la fonction active l'exige strictement.

## Architecture

### Registre centralisé

Tous les raccourcis sont définis dans `src/app_binding.c`. Le registre est détenu par la structure `App` principale et initialisé au démarrage.

```c
typedef struct {
    int key;                // Constante GLFW_KEY_*
    int mods;               // Masque binaire GLFW_MOD_*
    const char* action;      // Titre court (ex. : "Toggle Bloom")
    const char* desc;        // Description détaillée
    BindingCategory category;// Movement, Visuals, PostFX, System
    BindingType type;        // Toggle, Cycle, Action
} AppBinding;
```

### La capture « Dry-Run »

Lorsque la superposition d'aide est visible (`ctx->overlay->show_help != HELP_MODE_OFF`), le `key_callback` dans `app_input.c` entre en mode **Dry-Run** :

- Il intercepte les appuis de touches.
- Il met à jour `app->help_pressed_key` et `app->help_pressed_mods`.
- Il **empêche** l'exécution de la commande réelle, permettant à l'utilisateur d'explorer le clavier en toute sécurité.

## Maintenance des raccourcis

### Ajouter un nouveau raccourci

Pour ajouter un nouveau raccourci au système d'aide :

1. Ouvrez `src/app_binding.c`.
2. Localisez la fonction `app_binding_registry_init`.
3. Utilisez la macro helper `add_binding` :

```c
add_binding(GLFW_KEY_X, 0, "Mon action", "Détail supplémentaire sur ce que fait X.",
            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
```

### Support des modificateurs

Le système prend en charge les combinaisons de touches (ex. : `Shift + Touche`). Pour en ajouter une :

```c
add_binding(GLFW_KEY_Y, GLFW_MOD_SHIFT, "Toggle Probes", "...",
            BINDING_CAT_VISUALS, BINDING_TYPE_TOGGLE);
```

### Catégories et couleurs

L'interface code automatiquement les touches en couleur selon leur `BindingType` :

- **Cyan (`BINDING_TYPE_TOGGLE`)** : Commutateurs marche/arrêt.
- **Vert (`BINDING_TYPE_CYCLE`)** : Actions qui cyclent à travers plusieurs états.
- **Orange/Jaune (`GLFW_MOD_SHIFT`, `GLFW_MOD_ALT`)** : Touches de combinaison.
- **Bleu (`BINDING_TYPE_ACTION`)** : Actions ponctuelles (Réinitialisation, Capture d'écran, Quitter).

### Intégration dans la barre d'état

Certains modes de débogage (comme Bloom Debug) s'intègrent directement avec la **Superposition d'informations principale** (F1) pour afficher l'étape ou le sous-niveau (mip) actuellement actif, fournissant un retour en temps réel lors de la navigation.

## Personnalisation de l'interface

La disposition du clavier est définie dans `include/app_ui.h` et est centrée automatiquement. Vous pouvez ajuster les paramètres visuels (taille, rembourrage, textures) via `KeyboardLayoutConfig` et les constantes associées dans `include/app_ui.h`.

Les positions et étiquettes des touches sont stockées dans le tableau `KEY_LAYOUT_QWERTY` dans `include/app_ui.h`. Chaque entrée définit :

- La `key` gérée.
- L'index de `row`.
- La `xPos` relative au début de la rangée.
- La `width` de la touche (unité : largeur d'une touche carrée).
- Le `label` affiché sur la touche.
