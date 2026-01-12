# Structure du Projet Icosphere Refactorisé

## Architecture Modulaire

Le code a été refactorisé selon les principes suivants :
- **Séparation des responsabilités** : Chaque module a une fonction claire
- **Encapsulation** : Les structures de données sont gérées par leurs propres modules
- **Réutilisabilité** : Les composants peuvent être utilisés indépendamment
- **Maintenabilité** : Code plus facile à lire, tester et modifier

## Structure des Dossiers

```
icosphere/
├── src/
│   ├── main.c              # Point d'entrée du programme
│   ├── app.c               # Logique principale de l'application
│   ├── icosphere.c         # Génération de la géométrie icosphère
│   ├── shader.c            # Gestion des shaders
│   ├── texture.c           # Gestion des textures HDR/cubemaps
│   ├── skybox.c            # Rendu de la skybox
│   └── glad.c              # Chargeur OpenGL (généré)
│
├── include/
│   ├── gl_common.h         # Header OpenGL commun (GLAD + GLFW)
│   ├── app.h               # Interface de l'application
│   ├── icosphere.h         # Interface de génération de géométrie
│   ├── shader.h            # Interface de gestion des shaders
│   ├── texture.h           # Interface de gestion des textures
│   └── skybox.h            # Interface de rendu de skybox
│
├── shaders/
│   ├── phong.vert          # Vertex shader Phong
│   ├── phong.frag          # Fragment shader Phong
│   ├── background.vert     # Vertex shader skybox
│   ├── background.frag     # Fragment shader skybox
│   └── equirect2cube.glsl  # Compute shader HDR→Cubemap
│
├── assets/
│   └── env.hdr             # Texture HDR d'environnement
│
├── obj/                    # Fichiers objets (généré)
├── bin/                    # Exécutable (généré)
└── Makefile                # Script de compilation
```

## Modules et Responsabilités

### 1. **main.c**
- Point d'entrée minimal
- Initialise et lance l'application
- Gère le cycle de vie global

### 2. **app.c / app.h**
- Structure `App` contenant tout l'état de l'application
- Gestion de la fenêtre GLFW
- Boucle de rendu principale
- Gestion des entrées clavier
- Coordination entre les différents modules

### 3. **icosphere.c / icosphere.h**
- Structure `IcosphereGeometry` pour la géométrie
- Génération procédurale de l'icosphère
- Subdivision de la géométrie
- Calcul des normales
- Gestion des tableaux dynamiques (`Vec3Array`, `UintArray`)

### 4. **shader.c / shader.h**
- Chargement et compilation des shaders depuis fichiers
- Création de programmes vertex/fragment
- Création de programmes compute
- Gestion des erreurs de compilation/linkage

### 5. **texture.c / texture.h**
- Chargement des textures HDR avec stb_image
- Création de cubemaps d'environnement
- Conversion equirectangulaire → cubemap via compute shader
- Génération de mipmaps

### 6. **skybox.c / skybox.h**
- Structure `Skybox` pour le rendu de fond
- Géométrie de quad plein écran
- Rendu de l'environnement avec contrôle de blur (LOD)

## Améliorations par Rapport au Code Original

### ✅ **Organisation**
- Code divisé en modules logiques
- Headers séparant interface et implémentation
- Facile de trouver et modifier des fonctionnalités spécifiques

### ✅ **Réutilisabilité**
- Les modules peuvent être réutilisés dans d'autres projets
- API claires et documentées
- Structures de données encapsulées

### ✅ **Maintenabilité**
- Responsabilités clairement définies
- Moins de couplage entre composants
- Plus facile de déboguer et tester

### ✅ **Extensibilité**
- Facile d'ajouter de nouvelles fonctionnalités
- Exemple : ajouter un nouveau type de géométrie en suivant le modèle d'icosphere
- Système de shaders modulaire

### ✅ **Gestion de la Mémoire**
- Fonctions init/cleanup claires pour chaque module
- Moins de risques de fuites mémoire
- Cycle de vie des ressources bien défini

### ✅ **Lisibilité**
- Noms de fonctions descriptifs avec préfixes de module
- Structure du code cohérente
- Commentaires aux endroits clés

## Compilation et Exécution

```bash
# Compiler le projet
make

# Exécuter l'application
make run

# Nettoyer les fichiers générés
make clean
```

## Contrôles

### 🖱️ Contrôle de la Caméra à la Souris

**Mode Caméra Activé (par défaut)** :
- **Déplacer la souris** : Orienter la caméra (yaw/pitch)
- **Molette de la souris** : Zoom avant/arrière
- **C** : Toggle activation/désactivation du contrôle caméra
- **ESPACE** : Réinitialiser la position de la caméra

Quand le mode caméra est **activé** :
- Le curseur est caché et capturé
- Les mouvements de souris contrôlent l'orientation
- Pitch limité pour éviter le gimbal lock

Quand le mode caméra est **désactivé** (appuyez sur **C**) :
- Le curseur redevient visible
- Les mouvements de souris n'affectent pas la caméra
- Utile pour interagir avec l'interface

### ⌨️ Contrôle au Clavier

**Affichage** :
- **W** : Toggle wireframe/solid
- **↑** : Augmenter les subdivisions (max 6)
- **↓** : Diminuer les subdivisions (min 0)
- **ESC** : Quitter l'application

## Dépendances

- **GLFW** : Gestion de fenêtre et entrées
- **GLAD** : Chargeur OpenGL 4.4+
- **cglm** : Mathématiques vectorielles/matricielles
- **stb_image** : Chargement d'images HDR

## Notes Techniques

- Utilise OpenGL 4.4 Core Profile
- Support macOS avec `GLFW_OPENGL_FORWARD_COMPAT`
- Génération procédurale de géométrie en temps réel
- Environment mapping avec HDR et mipmaps
- Compute shaders pour conversion de textures
