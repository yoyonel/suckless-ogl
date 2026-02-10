# Standalone Unit Testing & Mocking Strategy 🧪

Cette documentation décrit la technique de tests "standalone" et de mocking utilisée dans le projet `suckless-ogl`, en particulier pour valider la logique métier complexe (comme le parsing de shaders) sans dépendances GPU.

## 1. Philosophie

L'objectif est de pouvoir tester une unité de code (un fichier `.c`) en isolation totale. Cela permet :

- Une exécution instantanée (< 10ms).
- Aucun besoin d'un contexte OpenGL valide ou de drivers GPU.
- Une compatibilité parfaite avec les environnements CI restreints.

## 2. Technique de Mocking par "Shadowing"

Contrairement aux frameworks de mocking lourds qui utilisent l'injection de dépendances au runtime, nous utilisons une approche au moment de la compilation :

### A. Redéfinition des Symboles

Dans le fichier de test (ex: `tests/test_shader_path_security_standalone.c`), nous redéfinissons les fonctions des bibliothèques externes (GLAD, Log) dont dépend le code testé.

```c
/* Mock OpenGL Definition */
GLuint glCreateShader(GLenum type) {
    (void)type;
    return 1; // Retourne un faux ID
}
```

### B. Inclusion Directe de l'Implémentation

Pour tester les fonctions privées (`static`) sans les exposer dans le header public `.h`, le fichier de test inclut directement le fichier `.c` après avoir défini les mocks.

```c
/* 1. Mocks */
void log_message(...) { /* no-op */ }

/* 2. Target Implementation */
#include "shader.c"

/* 3. Tests */
void test_logic() {
    // Appel d'une fonction static définie dans shader.c
    ASSERT_TRUE(get_dir_from_path(...));
}
```

## 3. Avantages et Limites

| Avantage | Description |
| :--- | :--- |
| **Accès Static** | Permet de tester 100% de la logique interne sans pollution d'API. |
| **Vitesse** | Pas de lien dynamique lourd ou d'initialisation matérielle. |
| **Zéro Dépendance** | Le binaire de test est "pure C". |

| Limite | Précaution |
| :--- | :--- |
| **Collision de Symboles** | Ne peut pas être lié avec la vraie bibliothèque en même temps. |
| **Maintenance** | Si l'API réelle change, le mock doit être synchronisé manuellement. |

## 4. Exemple Concret : Security Standalone

Le test `tests/test_shader_path_security_standalone.c` démontre parfaitement cette approche :

1. Il définit des stubs pour ~20 fonctions OpenGL.
2. Il inclut `src/shader.c`.
3. Il valide les algorithmes de `is_safe_path` et de gestion des buffers de chemins.

> [!TIP]
> Utilisez cette approche pour toute logique de parsing, de calcul mathématique ou de gestion de ressources (RAII) qui ne nécessite pas de lecture effective sur le matériel.
