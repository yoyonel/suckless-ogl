# Effet de Banding (Quantisation Couleurs)

L'effet de **Banding** (ou réduction de profondeur de couleur) est un filtre artistique qui réduit volontairement la précision des couleurs pour créer des styles allant du rétro-informatique au schéma technique.

## 🚀 Fonctionnement Rapide

Le système propose **5 modes distincts**, accessibles via un cycle sur la **touche '7'**. Chaque mode utilise une approche mathématique différente pour compresser l'espace colorimétrique.

```graphviz
digraph BandingFlow {
    rankdir=TD;
    bgcolor="transparent";
    margin=0.1;
    node [shape=rect, style="filled,rounded", fontname="Helvetica", fillcolor="#24283b", color="#414868", fontcolor="#c0caf5", penwidth=2];
    edge [color="#565f89", fontcolor="#9aa5ce", fontsize=10];

    A [label="Image HDR"];
    B [label="Banding Mode?", shape=diamond, fillcolor="#1a1b26", color="#7dcfff", fontcolor="#7dcfff"];
    C [label="0: Linear"];
    D [label="1: Dithered"];
    E [label="2: Perceptual"];
    F [label="3: Channel"];
    G [label="4: Luminance"];
    H [label="Image LDR finale", fillcolor="#1a1b26", color="#9ece6a", fontcolor="#9ece6a"];

    A -> B;
    B -> C; B -> D; B -> E; B -> F; B -> G;
    C -> H; D -> H; E -> H; F -> H; G -> H;
}

```

---

## 🎨 Les 5 Styles Artistiques

### 1. Pop Art (Linear)

Le mode le plus simple. Il divise l'espace colorimétrique en paliers égaux.

- **Usage** : Pour un look "Cel-shaded" ou "Comic book".

- **Maths** :

\f[ result = \frac{\lfloor color \cdot levels \rfloor}{levels} \f]

### 2. Retro Computing (Dithered)

Utilise une matrice de **Bayer 4x4** pour simuler des nuances intermédiaires via une grille de seuils.

- **Usage** : Style Macintosh, GameBoy ou vieux moniteurs CGA.

- **Principe** :

```graphviz
digraph DitherFlow {
    rankdir=TD;
    bgcolor="transparent";
    margin=0.1;
    node [shape=rect, style="filled,rounded", fontname="Helvetica", fillcolor="#24283b", color="#414868", fontcolor="#c0caf5", penwidth=2];
    edge [color="#565f89"];

    P [label="Scene Color"];
    Add [label="Bayer 4x4 Noise", fillcolor="#1a1b26", color="#bb9af7", fontcolor="#bb9af7"];
    Q [label="Quantization", fillcolor="#1a1b26", color="#e0af68", fontcolor="#e0af68"];
    Out [label="Retro Render", fillcolor="#1a1b26", color="#9ece6a", fontcolor="#9ece6a"];

    P -> Add -> Q -> Out;
}

```

### 3. Analog (Perceptual)

Applique une courbe de gamma avant la quantisation pour préserver plus de détails dans les zones sombres.

- **Usage** : Look "capteur vidéo vintage".

- **Avantage** : Évite les aplats noirs massifs dans les ombres.

\f[ result = \left( \frac{\lfloor color^{\gamma} \cdot levels \rfloor}{levels} \right)^{\frac{1}{\gamma}} \f]

### 4. CGA/VGA Style (Channel)

Réduit la précision de chaque canal RGB indépendamment.

- **Usage** : Simuler des palettes matérielles limitées (ex: 8 niveaux de rouge, 8 de vert, 4 de bleu).

### 5. Blueprint (Luminance)

Quantise la luminance perçue de l'image, puis applique une teinte colorée.

- **Usage** : Schémas techniques, hologrammes, interfaces futuristes.

---

## ⚙️ Paramètres (PostProcessPreset)

| Paramètre | Description |
| :-------- | :---------- |

| `mode` | Sélecteur du style (0 à 4). |
| `levels` | Nombre de niveaux de couleurs (ex: 2.0 = Noir/Blanc). |
| `dither_strength` | Intensité du grain (Mode 1 uniquement). |
| `perceptual_gamma` | Courbe de contraste (Mode 2 uniquement). |
| `channel_levels` | Canaux RGB (Mode 3) ou Couleur de Teinte (Mode 4). |

---

## 🛠 Intégration Technique

L'effet est implémenté dans le pipeline PBR via un Uber-shader optimisé.

```graphviz
digraph TechnicalFlow {
    rankdir=TD;
    bgcolor="transparent";
    margin=0.1;
    node [shape=rect, style="filled,rounded", fontname="Helvetica", fillcolor="#24283b", color="#414868", fontcolor="#c0caf5", penwidth=2];
    edge [color="#565f89"];

    C [label="C Code (App)"];
    U [label="UBO (Settings)", fillcolor="#1a1b26", color="#7aa2f7", fontcolor="#7aa2f7"];
    S [label="GLSL Shader", fillcolor="#1a1b26", color="#bb9af7", fontcolor="#bb9af7"];

    C -> U [label="Update"];
    U -> S [label="Uniforms"];
}

```
