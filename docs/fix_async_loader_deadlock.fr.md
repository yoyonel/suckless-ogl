# Correctif : Blocage du chargeur asynchrone

**Statut** : Résolu
**Composant** : `AsyncLoader` (chargement HDR asynchrone)

## Problème

Le chargeur asynchrone se bloquait aléatoirement lors de la demande d'un changement d'environnement pendant qu'un chargement était déjà en cours.

## Cause : Double verrouillage de mutex

Le bug était un verrouillage double (`double-lock`) de mutex avec `pthread`. La séquence problématique était :

```c
// Thread principal
pthread_mutex_lock(&loader->mutex);           // 1. Verrou acquis
loader->state = LOADING;

// Dans app_env_request (appelée depuis le callback clavier)
pthread_mutex_lock(&loader->mutex);           // 2. BLOCAGE — déjà verrouillé !
```

Le thread principal verrouillait le mutex, puis appelait indirectement une fonction qui tentait de le re-verrouiller depuis le même thread — un interblocage.

## Correctif : Re-verrouillage conditionnel

La solution préserve la cohérence en vérifiant si le mutex est déjà verrouillé avant de tenter l'acquisition :

```c
void async_loader_request(AsyncLoader* loader, const char* path)
{
    int already_locked = (loader->mutex_owner == pthread_self());

    if (!already_locked) {
        pthread_mutex_lock(&loader->mutex);
    }

    // ... logique de requête ...

    if (!already_locked) {
        pthread_mutex_unlock(&loader->mutex);
    }
}
```

`loader->mutex_owner` est mis à jour à chaque acquisition du mutex :

```c
pthread_mutex_lock(&loader->mutex);
loader->mutex_owner = pthread_self();
```

## Tests

Un test de stress vérifie la robustesse : `just stress-env-change` envoie des changements rapides d'environnement pendant une session de 30 secondes. Aucun blocage ne doit être observé.

## Voir aussi

- [async_loader.md](./async_loader.md) — Architecture du chargeur
- [synchronization_overview.md](./synchronization_overview.md) — Philosophie de synchronisation
