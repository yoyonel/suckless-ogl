# Standards Photographiques pour le Rendering Temps Réel

Guide complet des valeurs et concepts photographiques utilisés dans les moteurs de jeu modernes et le tone mapping.

---

## 📸 Le 18% Middle Gray - La Pierre Angulaire

### Origine Photographique

Le **18% gray** (0.18 en linéaire) est le **standard universel de la photométrie** depuis les années 1940.

```
Valeur: 0.18 (linéaire) = 18% de réflectance
sRGB: ~119/255 = 0.466
Hex: #777777
```

### Pourquoi 18% ?

1. **Perception humaine** : Notre œil perçoit 18% de réflectance comme "middle tone" neutre
2. **Moyenne statistique** : La majorité des scènes réelles ont une réflectance moyenne de ~12-20%
3. **Zone System d'Ansel Adams** : Zone V (milieu de l'échelle 0-X) = 18%

### Applications

- **Cartes grises** (gray cards) pour calibration caméra
- **Posemètres** calibrés sur 18% pour calculer l'exposition correcte
- **Lightroom/Photoshop** : Target pour auto-exposure
- **Unreal Engine, Unity** : Key value par défaut pour eye adaptation

---

## 🎚️ Échelle de Valeurs Photographiques

### Exposition Values (EV)

L'échelle EV mesure la quantité de lumière :

| EV | Condition | Luminance (cd/m²) | Usage Rendering |
|----|-----------|-------------------|-----------------|
| **-6** | Clair de lune | 0.001 | Scènes très sombres |
| **-4** | Intérieur faiblement éclairé | 0.01 | Donjons, caves |
| **0** | Intérieur bien éclairé | 1.0 | Pièces standards |
| **+4** | Ombre extérieure | 16 | Scènes extérieures ombragées |
| **+10** | Plein soleil | 1,000 | Journée ensoleillée |
| **+15** | Neige en plein soleil | 32,000 | Environnements très lumineux |
| **+20** | Soleil direct (surface) | 1,000,000 | HDR extrême |

### Formule Auto-Exposure

```glsl
targetExposure = keyValue / sceneLuminance
```

**Exemples** (avec keyValue = 0.18) :

- Scène sombre (0.01 cd/m²) → Exposure = 18.0 (boost ×18)
- Middle gray (0.18 cd/m²) → Exposure = 1.0 (neutre)
- Scène brillante (10 cd/m²) → Exposure = 0.018 (atténuation)

---

## 🌈 Valeurs de Réflectance Matériaux

### Albedo Standards (Physically Based)

| Matériau | Réflectance | Linéaire | sRGB (8-bit) |
|----------|-------------|----------|--------------|
| **Charbon** | 4% | 0.04 | 61 |
| **Peau sombre** | 12% | 0.12 | 105 |
| **18% Gray Card** | 18% | 0.18 | 119 |
| **Peau claire** | 35% | 0.35 | 160 |
| **Béton** | 50% | 0.50 | 186 |
| **Neige fraîche** | 90% | 0.90 | 241 |
| **Maximum (physique)** | 100% | 1.0 | 255 |

> ⚠️ En PBR, les albedos dépassant 0.90 sont non-physiques (sauf matériaux spéciaux)

---

## 📊 Tone Mapping - Courbes Standards

### 1. Linear (Naive)

```glsl
output = input * exposure
```

**Problème** : Clipping brutal à 1.0, perte de détails HDR.

### 2. Reinhard (Simple)

```glsl
output = input / (input + 1.0)
```

**Avantages** : Compression douce, jamais de clipping.
**Inconvénient** : Atténue trop les couleurs saturées.

### 3. Uncharted 2 / Hable (Filmic)

```glsl
// Reproduit la réponse des films argentiques
vec3 FilmicToneMapping(vec3 x) {
    float A = 0.15; // Shoulder strength
    float B = 0.50; // Linear strength
    float C = 0.10; // Linear angle
    float D = 0.20; // Toe strength
    float E = 0.02; // Toe numerator
    float F = 0.30; // Toe denominator

    return ((x * (A * x + C * B) + D * E) /
            (x * (A * x + B) + D * F)) - E / F;
}
```

**Caractéristiques** :
- **Toe** (pied) : Relève les ombres
- **Shoulder** (épaule) : Compresse les hautes lumières
- **Linear section** : Préserve les mid-tones

### 4. ACES (Academy Color Encoding System)

**LE standard de Hollywood** (utilisé par Unreal Engine par défaut)

```glsl
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
```

**Avantages** :
- Couleurs saturées préservées
- Transition douce vers le blanc
- Standard industriel (films, jeux AAA)

---

## 🎮 Valeurs Recommandées pour Jeux

### Auto-Exposure Settings

| Paramètre | Valeur Conservatrice | Valeur Dynamique | Usage |
|-----------|---------------------|------------------|-------|
| **keyValue** | 0.18 | 0.14 - 0.20 | Standard / FPS rapide |
| **minLuminance** | 1.0 | 0.5 - 2.0 | Pas de boost / Donjons |
| **maxLuminance** | 5000 | 1000 - 10000 | Journée / HDR extrême |
| **speedUp** | 2.0 | 1.0 - 3.0 | Adaptation lente / rapide |
| **speedDown** | 1.0 | 0.5 - 2.0 | Adaptation pupille |

### Exemples par Genre

**FPS Réaliste** (comme Call of Duty)
```c
keyValue = 0.15        // Légèrement plus sombre (tactique)
minLuminance = 0.8
maxLuminance = 8000
speedUp = 3.0          // Adaptation rapide (gameplay)
speedDown = 1.5
```

**RPG Fantaisie** (comme Skyrim)
```c
keyValue = 0.20        // Plus lumineux (exploration)
minLuminance = 0.5     // Boost pour donjons
maxLuminance = 10000   // Ciel magique HDR
speedUp = 1.5          // Adaptation douce
speedDown = 1.0
```

**Horreur** (comme Resident Evil)
```c
keyValue = 0.12        // Très sombre (atmosphère)
minLuminance = 2.0     // Pas de boost (oppressant)
maxLuminance = 1000
speedUp = 0.5          // Adaptation très lente
speedDown = 2.0        // Retour rapide au noir
```

---

## 🔧 Valeurs Alternatives Intéressantes

### Key Value Alternatifs

| Valeur | Nom | Effet | Usage |
|--------|-----|-------|-------|
| **0.18** | Standard photographique | Neutre, équilibré | Défaut universel |
| **0.14** | Unreal Engine (UE4/5) | Légèrement plus sombre | Jeux AAA |
| **0.12** | Low-Key | Sombre, dramatique | Cinematic, horreur |
| **0.25** | High-Key | Lumineux, aéré | Cartoon, fantasy léger |
| **0.50** | Very High-Key | Très clair | Surexposition artistique |

### Middle Gray en sRGB

Attention : 18% **linéaire** ≠ 18% **sRGB** !

```
Linéaire 0.18  → sRGB 0.466  (gamma 2.2)
Linéaire 0.18  → 8-bit 119/255
```

**Piège courant** : Utiliser directement 0.18 en sRGB donne un gris trop sombre !

---

## 📐 Formules Utiles

### Conversion Luminance

```glsl
// Rec. 709 (HD TV standard)
float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

// Alternative (Rec. 601 - SD TV)
float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
```

### Conversion EV ↔ Luminance

```glsl
float ev = log2(luminance);
float luminance = exp2(ev);
```

### Adaptation Temporelle

```glsl
float adaptationSpeed = (target > current) ? speedUp : speedDown;
float factor = 1.0 - exp(-deltaTime * adaptationSpeed);
float newExposure = mix(current, target, factor);
```

---

## 🎨 Workflow Pratique

### 1. Calibration Initial

```c
// Démarrer avec valeurs neutres
keyValue = 0.18
minLuminance = 1.0
maxLuminance = 5000.0
```

### 2. Tester avec Scènes Types

- **Intérieur sombre** : Vérifier boost pas excessif
- **Extérieur jour** : Vérifier pas de sur-exposition
- **Transition rapide** : Ajuster vitesses adaptation

### 3. Tweaking Artistique

- **Trop clair** → Réduire keyValue (0.14 - 0.16)
- **Trop sombre** → Augmenter keyValue (0.20 - 0.25)
- **Flashy/instable** → Réduire speedUp/Down
- **Trop lent** → Augmenter speedUp

---

## 📚 Ressources Recommandées

1. **Film Lighting Simulator** (Unreal Engine docs)
2. **"Physically Based Rendering"** - Matt Pharr, Greg Humphreys
3. **Tone Mapping Comparison** - John Hable (Filmic Worlds blog)
4. **ACES Documentation** - Academy of Motion Picture Arts
5. **"Real Shading in Unreal Engine 4"** - Brian Karis (SIGGRAPH 2013)

---

## ✨ Bonus : Valeurs Exotiques

### Scènes Lunaires

```c
keyValue = 0.09       // Adaptation scotopique (bâtonnets)
minLuminance = 5.0    // Aucun boost, vision nocturne
```

### Underwater (Sous-Marin)

```c
keyValue = 0.22       // Compensation diffusion lumière
speedUp = 0.3         // Adaptation très lente (eau)
```

### Space (Espace)

```c
minLuminance = 10.0   // Contraste extrême
maxLuminance = 100000 // Soleil direct sans atmosphère
```
