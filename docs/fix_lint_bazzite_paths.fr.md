# Correctif : Chemins Bazzite dans les scripts de lint

**Statut** : Résolu
**Plateformes affectées** : Bazzite OS, Fedora Silverblue, et autres distributions avec `/var/home`

## Problème

Sur Bazzite et les distributions basées sur OSTree, le répertoire home des utilisateurs est `/var/home/user` plutôt que `/home/user`. Cette différence causait l'échec du script `run-clang-tidy` car il ne reconnaissait pas les chemins des fichiers sources.

## Cause : Lien symbolique `/home` → `/var/home`

Sur ces distributions :
```bash
$ ls -la /home
lrwxrwxrwx 1 root root 9 jan  1 00:00 /home -> var/home
```

Le script `run-clang-tidy` résolvait les chemins absolus et voyait `/var/home/user/projet/src/main.c`, mais le filtre de fichiers ne correspondait pas car il cherchait `/home/user/...`.

## Correctif : Détection du préfixe de chemin

Le script de lint détecte maintenant le préfixe réel du répertoire home et l'utilise pour les filtres :

```bash
#!/bin/bash
# Déterminer le préfixe de chemin (Bazzite utilise /var/home)
REAL_HOME=$(realpath "$HOME")
PROJECT_ROOT=$(realpath "$PWD")

# Utiliser le chemin réel pour le filtre run-clang-tidy
run-clang-tidy -p build/ \
    -header-filter="${PROJECT_ROOT}/include/.*" \
    "${PROJECT_ROOT}/src/.*"
```

En utilisant `realpath`, le script normalise les chemins indépendamment des liens symboliques.

## Validation

```bash
# Vérifier que le chemin réel est utilisé
realpath ~/projet  # Doit retourner /var/home/user/projet sur Bazzite

# Tester le lint
just lint
```

Le lint doit désormais fonctionner sur toutes les distributions sans configuration spéciale.

## Voir aussi

- [linting_strategy.md](./linting_strategy.md) — Stratégie de lint et mise en cache
- [build.md](./build.md) — Configuration de la compilation
