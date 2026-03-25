# Analyse de refactoring — IBL State Machine (2026-02)

**Date** : Février 2026
**Type** : Analyse de risque

## Objectif

Évaluer les risques associés au refactoring de la machine à états IBL pour implémenter la génération progressive des mipmaps.

## Score de risque global : 2.5/10

Le refactoring est **faible risque** selon l'analyse suivante.

## Facteurs de faible risque

### Couverture de tests existante

Les modules clés impliqués dans la machine à états IBL sont couverts par les tests unitaires :

- `test_async_loader` : Teste les transitions d'états du chargeur
- `test_app_env` : Teste les requêtes et transitions d'environnement
- `test_progressive_ibl` : Teste la génération incrémentielle

La confiance dans la couverture est **élevée** pour les chemins de code critiques.

### Isolation des changements

Le refactoring est confiné à :
- `src/app_env.c` — Machine à états principale
- `include/app_env.h` — Définitions des états

Les interfaces externes (app.c, tests) restent inchangées.

### Rollback facile

La machine à états originale (génération monolithique) peut être restaurée via un seul `#define` :

```c
#ifdef IBL_PROGRESSIVE
    // Nouvelle logique de génération progressive
#else
    // Ancienne logique monolithique
#endif
```

## Facteurs de risque mineurs

### Timing GPU

La génération progressive dépend de barrières mémoire correctement placées. Un oubli pourrait causer des artefacts visuels transitoires (pas un crash).

**Mitigation** : Test visuel systematique après chaque étape du refactoring.

### État pendant les transitions

Si un changement d'environnement est demandé pendant la génération progressive du premier, la file d'attente doit être gérée correctement.

**Mitigation** : Tests de stress avec transitions rapides (`just stress-env-change`).

## Conclusion

Le risque faible (2.5/10) justifie de procéder avec le refactoring. La couverture de tests existante et l'isolation du changement minimisent les risques de régression.

## Voir aussi

- [progressive_ibl.md](./progressive_ibl.md) — Implémentation résultante
- [ibl_architecture_ideas.md](./ibl_architecture_ideas.md) — Idées d'architecture IBL
