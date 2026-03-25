# Système de profilage GPU

Le système de profilage GPU de `suckless-ogl` fournit une mesure temporelle haute précision pour les différentes étapes du pipeline de rendu. Il utilise les requêtes de minuterie OpenGL (`GL_TIMESTAMP`) pour mesurer le temps d'exécution réel sur le GPU.

## Vue d'ensemble de l'implémentation

### Composants principaux

* **`GPUProfiler`** : La structure principale gérant la collection d'échantillons de performance.
* **`AdaptiveSampler`** : Pour chaque étape, maintient un historique d'échantillons afin de fournir une « moyenne » lissée et filtrer le bruit.
* **Double tampon** : Utilise deux ensembles de tampons de requêtes pour permettre au CPU de lire les résultats de l'image précédente (N-1) pendant l'enregistrement de l'image courante (N).

### Mesure synchronisée (attente bloquante)

Pour garantir qu'**aucune donnée n'est perdue** (même lorsque le GPU est fortement chargé ou en retard), le profileur utilise une **attente bloquante** lors de la récupération des résultats :

1. Au début de l'image N, le double tampon standard vérifie si les résultats de l'image N-1 sont prêts.
2. Si non prêts, les approches standards ignorent la donnée (créant des lacunes).
3. **Notre approche** : Nous attendons explicitement (`glGetQueryObjectui64v`), forçant le CPU à se synchroniser avec la fin de l'image précédente. Cela garantit une ligne temporelle continue et sans lacunes même lors de pics de performance.

### Profilage conditionnel (overhead nul)

Comme les attentes bloquantes impactent les performances, le système est **entièrement conditionnel** :

* **Activé** : Lorsque l'interface Timeline est visible (F3), que la journalisation des métriques est active (F4), ou que le **Benchmark d'effets** est en cours.
* **Désactivé** : Aucune requête OpenGL n'est émise et aucune vérification de synchronisation n'est effectuée. L'impact sur les performances est effectivement nul.

## Intégration

Le profilage est intégré dans la boucle principale `app_render`.

### Utilisation dans le code

**Modèle RAII (recommandé) :**
L'utilisation de la macro RAII simplifie le code et garantit que les étapes sont fermées correctement.

```c
/* Début d'image (contrôlé par la visibilité de l'App) */
gpu_profiler_begin_frame(&app->gpu_profiler, frame_index);

/* ... */

/* Enregistrement d'une étape spécifique via RAII */
{
    GPU_STAGE_PROFILER(&app->gpu_profiler, "Geometry Pass", COLOR_HEX);
    // ... Commandes de rendu ...
} // L'étape se termine automatiquement ici
```

**Modèle manuel :**

```c
gpu_profiler_start_stage(&app->gpu_profiler, "Geometry Pass", COLOR_HEX);
    // ... Commandes de rendu ...
gpu_profiler_end_stage(&app->gpu_profiler);
```

## Visualisation

Les métriques de performance sont affichées dans l'interface de l'application (superposition Timeline) :

* **Barres** : Représentation visuelle de la durée d'exécution relative au temps total d'image.
* **Hiérarchie** : Les étapes imbriquées sont indentées.
* **Texte** : Durée exacte en millisecondes (alignée à droite pour la lisibilité).

Contrôles :

* **F3** : Basculer la visibilité de la Timeline.
* **SHIFT+F3** : Basculer la position de la Timeline (Haut/Bas).
* **F4** : Basculer la journalisation des métriques dans la console.

## Journal des modifications

* **2026-02-08** :
  * **Support RAII** : Ajout de la macro `GPU_STAGE_PROFILER` pour la gestion automatique des étapes (code plus propre, retours anticipés plus sûrs).
  * **Instrumentation** : Instrumentation de l'étape **UI Overlay** pour combler le dernier manque dans la timeline GPU.
  * **Robustesse** : Correction de la disparition de la superposition lors des redimensionnements/blocages en maintenant indéfiniment le dernier état d'image valide.
  * **Intégrité des données** : Passage à la récupération **bloquante** des requêtes pour une fiabilité à 100 % même sous charge.
  * **Performance** : Implémentation du **Profilage conditionnel** pour un overhead nul lorsque masqué.
  * **UX** : Métriques temporelles alignées à droite dans la superposition Timeline pour une comparaison facilitée.
  * **Correctifs** : Résolution des avertissements d'état de texture OpenGL dans le backend de rendu de l'interface.
  * **Intégration Benchmark** : Le profileur s'active automatiquement lorsque `effect_benchmark` est en cours d'exécution pour éviter les blocages.
