# PR Evaluation: Shader Path Security 🛡️

Cette PR apporte une amélioration critique à la sécurité et à la robustesse du chargeur de shaders (`@header`). Elle traite non seulement les vulnérabilités de type "Path Traversal", mais améliore aussi la gestion des erreurs internes (troncation).

## 1. Apport de la PR

- **Sécurité (Voisinage Zero-Trust)** : L'ajout de `is_safe_path` empêche l'utilisation de `..`, des chemins absolus `/`, et des caractères suspects (`:`, `\`). C'est essentiel pour éviter qu'un shader malveillant ne puisse lire des fichiers sensibles (ex: `/etc/passwd` ou des clés SSH) via des inclusions relatives.
- **Robustesse des Buffers** : Remplacement des opérations de chaînes potentiellement risquées par des vérifications explicites de taille (`safe_snprintf`, `safe_memcpy`) et détection de la troncation des chemins.
- **Testabilité** : Introduction des tests "Standalone", permettant de tester la logique métier sans dépendances lourdes (GPU, Drivers).

## 2. Qualité du Code

- **Programmation Défensive** : Utilisation de `MAX_INCLUDE_DEPTH` pour limiter la récursivité et `MAX_SHADER_SOURCE_SIZE` (16MB) pour prévenir les attaques par déni de service (DoS).
- **Gestion RAII** : Usage intelligent de `__attribute__((cleanup(ctx_free)))` pour garantir la libération de la mémoire complexe du système d'inclusions, même en cas d'erreur précoce.
- **Précision du Parsing** : Le parser de `@header` gère correctement les chemins avec ou sans guillemets, et nettoie les espaces/retours chariot résiduels.

## 3. Analyse de `tests/test_shader_path_security_standalone.c`

### Méthode de Test : "Mocking & Injection"

Le fichier utilise une stratégie astucieuse pour isoler la logique de `src/shader.c` :

1. **Mocks Légers** : Au lieu de lier `glad` ou le système de log complexe, le test redéfinit des fonctions "vides" (lignes 11-173). Cela permet au compilateur de satisfaire les symboles sans infrastructure OpenGL.
2. **Inclusion Directe du C** : `#include "shader.c"` (ligne 175). C'est la clé pour tester des fonctions `static` (ex: `get_dir_from_path`, `parse_include_path`) sans les exposer dans le header public.
3. **Isolation Totale** : Le test se compile en un exécutable minimal, ultra-rapide et déterministe, idéal pour une CI qui doit tourner sans GPU (ex: GitHub Actions standard).

### Focus sur la Sécurité via Mock

Le test valide spécifiquement les **cas aux limites de troncation**. Par exemple, `test_get_dir_from_path_truncation` vérifie que si le buffer de sortie est trop petit pour contenir le répertoire, la fonction échoue proprement (`false`) au lieu de copier une chaîne tronquée potentiellement dangereuse ou invalide.

---

## Conclusion & Recommandations

**Score : 9/10**

- **Points forts** : Sécurité granulaire, tests rapides, code propre.
- **Amélioration possible** : Ajouter un test dans le standalone pour `is_safe_path` lui-même avec une liste de "payloads" classiques (`../../`, `/etc/`, `C:\`, etc.) pour centraliser la validation de la blacklist.

> [!TIP]
> La méthode d'inclusion directe du fichier `.c` dans les tests est excellente pour le code "infrastructure" comme celui-ci. Elle permet d'atteindre une couverture de branche de 100% sur la logique de parsing sans avoir à créer des fichiers physiques sur le disque pour chaque combinaison.
