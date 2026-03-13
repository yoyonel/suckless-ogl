# Cubemap Seam Resolution

## 🔍 Identified Problem

The edges of the cubemap are visible as lines or artifacts. This can be caused by several factors:

1. **LOD too high**: A high `blur_lod` (4.0) uses low-resolution mipmap levels.

2. **Insufficient Resolution**: 512x512 might be too small.

3. **Filtering without seamless**: Transitions between faces are not smoothed.

4. **Edge Sampling**: Interpolation between faces is poorly handled.

## 🏁 Definitive Solution: Equirectangular Mapping

Although previous solutions (Seamless Cubemap, Increased Resolution) improved the situation, the most robust solution for this project was to **completely remove the cubemap conversion step**.

### Why?

1. **No more faces**: An equirectangular texture is a single continuous 2D rectangle. There are no longer "face edges" where seams can appear.

2. **Simplified Pipelines**: We go directly from the HDR image (panoramic) to rendering, without passing through a conversion compute shader.

3. **Less Memory**: No need to allocate an additional cubemap texture.

4. **Max Quality**: We sample the source data directly.

### Visual Comparison

```graphviz
digraph CubemapVsEqui {
  rankdir=LR;
  bgcolor="transparent";
  dpi=72;

  // Suckless-Modern "Ghost" Design Tokens (Upscaled)
  node [
    shape=rect,
    style="rounded",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=16,
    fillcolor="none",
    color="#414868",
    fontcolor="#c0caf5",
    penwidth=2
  ];

  edge [
    color="#565f89",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=18,
    fontcolor="#9aa5ce",
    arrowsize=0.8,
    penwidth=1.2
  ];

  subgraph cluster_cube {
    label="Cubemap (Seams)";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#f7768e";
    style="rounded,dashed";
    color="#f7768e";
    margin=15;

    Face1 [label="Face +Z", color="#414868"];
    Face2 [label="Face +X", color="#414868"];
    Seam [label="Seam Artifact", shape=diamond, color="#f7768e", fontcolor="#f7768e"];
    Face1 -> Seam -> Face2 [style=dotted];
  }

  subgraph cluster_equi {
    label="Equirectangular (Seamless)";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#9ece6a";
    style="rounded";
    color="#9ece6a";
    margin=15;

    Map [label="Continuous 2D Map\n(0.0 to 1.0 UV)", width=2.5, color="#7aa2f7", fontcolor="#7aa2f7"];
    Sample [label="Math Sample\n(atan2/asin)", color="#9ece6a", fontcolor="#9ece6a", penwidth=3];
    Map -> Sample [label="Interpolated"];
  }
}

```

### Comparison: Cubemap vs Equirectangular

| Feature     | Cubemap (Old)         | Equirectangular (Current)    |
| ----------- | --------------------- | ---------------------------- |
| Seams       | Possible at edges     | **Impossible**               |
| Complexity  | High (Compute Shader) | **Low** (Direct)             |
| Artifacts   | Mipmapping at corners | **None** (Continuous linear) |
| Flexibility | Industry standard     | Ideal for HDR visualizers    |

### Software Implementation

Switching to equirectangular allowed removing:

- The compute shader `equirect2cube.glsl`.

- The functions `texture_create_env_cubemap` and `texture_build_env_cubemap`.

- The complexity of managing 6 faces.

### Conclusion

For skybox rendering where the fidelity of the source HDR image is paramount, direct equirectangular mapping is the most "suckless" solution: less code, higher quality.
