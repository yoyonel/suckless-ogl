# Histogramme de débogage de l'exposition automatique

Ce document décrit le système d'histogramme de luminance utilisé pour le débogage de l'exposition automatique.

## Vue d'ensemble

L'exposition automatique repose sur une mesure de la luminance moyenne de la scène. Pour déboguer ce système, un histogramme de luminance est calculé chaque image et rendu accessible au CPU via un readback PBO asynchrone.

## Histogramme de luminance

L'histogramme est calculé dans le shader compute `lum_histogram.comp` :

1. Chaque pixel de l'image post-traitement contribution son niveau de luminance dans l'un des 256 seaux
2. Les seaux sont accumulés par atomicAdd dans un SSBO
3. Le résultat est téléchargé vers le CPU via PBO

### Calcul de la luminance

```glsl
// Conversion RGB en luminance perceptuelle
float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
// Compression logarithmique pour couvrir la plage HDR
float log_luma = (log2(luma + 0.0001) - LOG_MIN) / (LOG_MAX - LOG_MIN);
int bucket = int(clamp(log_luma, 0.0, 1.0) * 255.0);
```

## Readback PBO asynchrone

Le téléchargement de l'histogramme utilise le protocole PBO non bloquant :

1. `glReadPixels` vers le PBO est déclenché (non bloquant)
2. L'image suivante vérifie la clôture
3. Si le GPU a terminé, les données sont lues sans attente

## Mise en cache et continuité

Pour éviter les artefacts when le readback n'est pas encore prêt, le dernier histogramme valide est conservé en cache :

```c
// Conserver le dernier histogramme valide
if (histogram_ready) {
    memcpy(app->histogram_cache, histogram_data, sizeof(app->histogram_cache));
}
// Toujours utiliser le cache pour l'affichage
render_histogram(app->histogram_cache);
```

Cela garantit que l'affichage déboggage ne clignote jamais, même sous charge GPU élevée.

## Cas de test

| Scénario | Histogramme attendu | Comportement de l'exposition |
|----------|--------------------|-----------------------------|
| Scène sombre uniforme | Pic dans les seaux bas | Compensation positive |
| Scène claire uniforme | Pic dans les seaux hauts | Compensation négative |
| Scène mixte | Distribution étalée | Exposition centrée |
| Surexposition HDR | Pics dans les seaux > 200 | Plafonnement EV max |

## Voir aussi

- [exposure_analysis.md](./exposure_analysis.md) — Analyse détaillée de l'exposition
- [async_pbo.md](./async_pbo.md) — Protocole PBO asynchrone
