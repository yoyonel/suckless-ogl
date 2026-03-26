# Profil de Caméra : Sony Alpha 7S III (ILCE-7SM3)

Référence technique pour le POC cinématique et l'intégration future du profil de capteur Sony Alpha 7S III.

## 📊 Spécifications Techniques

| Paramètre | Valeur | Définition & Rôle Moteur |
| :--- | :--- | :--- |
| **Type de Capteur** | CMOS Plein Format 35mm | Impacte les multiplicateurs de focale et la profondeur de champ (DOF). |
| **Taille du Capteur** | 35,6 x 23,8 mm | Surface physique utilisée pour le cercle de confusion et l'échelle de bokeh. |
| **Simulation DOF** | 35mm f/1,8 (Ciné) | DOF faible avec bokeh ovale vertical anamorphique de 2,0x. |
| **Résolution de Sortie** | 12,1 MP (4240 x 2832) | Référence de pixel pitch pour la fréquence du grain/bruit. |
| **Plage Dynamique** | 15+ stops (S-Log3) | Plafond HDR élevé ; mappe au "roll-off" ACES. |
| **Double ISO natif** | 640 / 12 800 | Points de rapport signal/bruit optimal ; déclenche les décalages de gain. |
| **Espace Couleur** | S-Gamut3.Cine | Primaires à large gamme ; cible pour les transitions de gradation. |
| **Courbes Gamma** | S-Log3, S-Cinetone | Fonctions de transfert logarithmiques vs Cine-style (EOTF). |

## 🔍 Définitions des Paramètres

### 1. Taille du Capteur & Facteur de Forme

Dans notre moteur, le format **Plein Format** sert de référence « 1,0x ». Il dicte la relation entre la distance focale (mm) et le champ de vision (FOV). Un objectif de 35mm sur ce capteur offre une perspective cinématique « standard » sans facteur de recadrage.

### 2. S-Log3 (Courbe Gamma)

Une courbe quasi-logarithmique conçue pour préserver le maximum de plage dynamique du capteur. Elle compresse les hautes lumières et soulève les ombres pour faire tenir plus d'informations dans le conteneur 10-bit/12-bit. En rendu, c'est notre étape intermédiaire « Linéaire vers Log » avant le grading colorimétrique.

### 3. Double ISO natif (Dual Gain)

L'Alpha 7S III utilise deux circuits analogiques distincts. Bas Gain (ISO 640) pour les scènes lumineuses et Haut Gain (ISO 12 800) pour la basse lumière. Dans le shader, cela devrait affecter le **Plancher de Bruit** et l'intensité du grain dans les zones sombres.

### 4. S-Cinetone

Une science des couleurs dérivée de la caméra de cinéma Sony VENICE. Elle se concentre sur des tons chair agréables (tons moyens naturels) et un "roll-off" des hautes lumières plus doux par rapport au Rec.709 standard.

### 5. Bokeh Anamorphique (2.0x)

Le profil simule désormais les optiques anamorphiques de cinéma haut de gamme en appliquant un **étirement vertical de 2,0x** au noyau de bokeh. Cela produit les hautes lumières floues de forme "ovale" et l'étirement d'arrière-plan en "cascade" typiques des objectifs de cinéma professionnels.

## 📚 Sources & Ressources Techniques

- **Spécifications Officielles** : [Sony Alpha 7S III Product Page](https://www.sony.com/electronics/interchangeable-lens-cameras/ilce-7sm3)
- **Tests de Capteur** : [PhotonsToPhotos (Read Noise/DR)](https://www.photonstophotos.net/Charts/PDR.htm#Sony%20ILCE-7SM3)
- **Analyse Plage Dynamique** : [DXOMark Sensor Review](https://www.dxomark.com/Cameras/Sony/A7S-III)
- **Logiciel & Log** : [Sony Help Guide - S-Log3/S-Gamut3](https://helpguide.sony.net/di/pp/v1/en/contents/TP0000909108.html)
- **Référence Visuelle** : `docs/camera_references/preview_sony_a7siii_reference.jpg`

## ⌨️ Commandes Temps Réel

- **Appliquer le Profil Sony A7S III** : Appuyez sur `F8` en cours d'exécution.
