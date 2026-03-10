# Lexique Technique

Ce lexique regroupe les termes et expressions techniques utilisés dans le projet, couvrant les aspects théoriques du rendu PBR, les optimisations géométriques et les concepts de bas niveau de l'API graphique.

---

## 🎨 Rendu & Physique (PBR / IBL)

| Terme | Définition Logicielle / Théorique |
| :--- | :--- |
| **PBR** | *Physically Based Rendering*. Modèle de rendu basé sur les lois de la physique pour simulant l'interaction réelle de la lumière avec les matériaux. |
| **IBL** | *Image Based Lighting*. Utilisation d'une image (cubemap HDR) pour simuler une illumination globale complexe. |
| **BRDF** | *Bidirectional Reflective Distribution Function*. Fonction définissant comment un matériau réfléchit la lumière (modèle de Cook-Torrance). |
| **NDF (GGX)** | *Normal Distribution Function*. Partie du PBR décrivant la micro-géométrie des surfaces (distribution des micro-facettes). |
| **Split-Sum** | Approximation mathématique (Epic Games) permettant de calculer l'IBL spéculaire en temps réel via une pré-intégration (Prefiltered Map + BRDF LUT). |
| **Irradiance / Radiance** | L'Irradiance est le flux lumineux incident total (diffus), la Radiance est le flux dans une direction précise (spéculaire). |
| **Normal Mapping / TBN** | Simulation de détails via une texture. La matrice **TBN** (Tangent, Bitangent, Normal) transforme les vecteurs du "Tangent Space" vers le "World Space". |

## 📐 Optimisations de Projection (Sphères / Billboards)

| Terme | Définition Logicielle |
| :--- | :--- |
| **Imposteurs** | Technique simulant une géométrie 3D complexe (sphère) sur un quad 2D via du ray-casting dans le shader. |
| **Analytic AA** | Anti-Aliasing mathématique sur le bord de la sphère calculé à partir de la dérivée du discriminant (`fwidth`), offrant des contours parfaits. |
| **Tangent Planes** | Méthode géométrique calculant la "bounding box" (AABB) parfaite d'une sphère à l'écran via les plans tangents passant par la caméra. |
| **Conservative Depth** | Positionnement du quad au point le plus proche de la sphère pour garantir un Z-test correct avant l'écriture de `gl_FragDepth`. |
| **Discriminant** | Valeur de l'équation Rayon-Sphère ($b^2 - ac$). Détermine si un pixel est à l'intérieur ($>0$) ou à l'extérieur ($<0$) de la sphère. |
| **Perspective Distortion** | Déformation elliptique d'une sphère lorsqu'elle s'éloigne du centre de l'écran, gérée ici par la projection exacte des tangents. |

## ⚙️ API Graphique & Flux de Données (GPU)

| Terme | Définition Logicielle |
| :--- | :--- |
| **PBO (Zero-Copy)** | *Pixel Buffer Object*. Utilisation de `GL_MAP_UNSYNCHRONIZED_BIT` pour uploader des données sans bloquer le CPU. |
| **Fence Sync** | Objet `glFenceSync` permettant de vérifier la complétion d'une tâche GPU sans forcer un vidage complet (stall) du pipeline. |
| **Flat Interpolation** | Qualificateur `flat` empêchant l'interpolation entre sommets, crucial pour la stabilité numérique des calculs de silouhette sur les sphères. |
| **Provoking Vertex** | Sommet dont la valeur est utilisée par l'ensemble de la primitive lors d'une interpolation `flat`. |
| **Pipeline Stall** | Latence critique où le CPU attend le GPU (causée par une lecture synchrone ou un `glFinish`). |

## 🚀 Architecture & Système

| Terme | Définition Logicielle |
| :--- | :--- |
| **Time Slicing** | Découpage d'opérations lourdes (IBL) en petites tranches réparties sur plusieurs frames pour maintenir un FPS constant. |
| **Double Buffering (Pending)** | Préparation d'un nouvel état (ex: environnement) en arrière-plan pendant que l'ancien est toujours en cours d'affichage. |
| **SIMD (AVX)** | Utilisation d'instructions processeur larges pour traiter plusieurs flottants simultanément (utilisé pour le tri des sphères). |
| **PAL** | *Platform Abstraction Layer*. Couche isolant le code métier des spécificités système (Linux/Windows). |
| **MkDocs / Doxygen** | Outils de génération de documentation (Narrative vs API Reference). |
| **Tracy** | Profileur hybride (CPU/GPU) utilisé pour analyser les performances en temps réel. |
