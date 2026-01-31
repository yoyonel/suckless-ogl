
# Texture Pipeline Optimization (Immutable Storage)

## 1. Problem: Mutable Storage
The historical function `glTexImage2D`:
1.  Allows resizing the texture at any time.
2.  Allows changing the format (RGB -> RGBA) at any time.
3.  **Consequence**: The driver must check the "completeness" of the texture *at every Draw Call*.
4.  **Overhead**: Validation CPU cost.

## 2. Solution: Immutable Storage
Since OpenGL 4.2, `glTexStorage2D`:
1.  Allocates all mipmap levels in one go.
2.  Sizes and formats are frozen (Immutable).
3.  Data is uploaded via `glTexSubImage2D`.
4.  **Benefit**: The driver knows the texture is valid. No validation at Draw time.

## 3. Implementation in Engine

### Code (texture.c)

```c
// Old Method (BAD)
// glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

// New Method (GOOD)
// 1. Determine number of levels
int levels = 1 + floor(log2(max(width, height)));

// 2. Allocate storage (VRAM)
glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA8, width, height);

// 3. Upload data
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

// 4. Generate Mipmaps (Hardware)
glGenerateMipmap(GL_TEXTURE_2D);
```

## 4. Alignment Constraint (Unpack Alignment)
When `stb_image` loads an image with 3 channels (RGB), the rows may not be aligned to 4 bytes (standard OpenGL default).
*   **Symptom**: Skewed or slanted image.
*   **Fix**:
    \code{.c}
    if (channels == 3) glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    else               glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    \endcode

## 5. Memory Layout Comparison

\dot
digraph TextureStorage {
  rankdir=LR;
  bgcolor="transparent";
  dpi=96;

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

  subgraph cluster_mutable {
    label="Mutable (glTexImage2D)";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#f7768e";
    style="rounded,dashed";
    color="#f7768e";
    margin=20;
    Def [label="Definition?", color="#414868"];
    Mip0 [label="Malloc Mip0", color="#414868"];
    Mip1 [label="Malloc Mip1", color="#414868"];
    Check [label="Check Completeness\n(Every Draw!)", shape=diamond, color="#f7768e", fontcolor="#f7768e"];

    Def -> Mip0;
    Mip0 -> Mip1;
    Mip1 -> Check;
  }

  subgraph cluster_immutable {
    label="Immutable (glTexStorage2D)";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#9ece6a";
    style="rounded";
    color="#9ece6a";
    margin=20;
    Alloc [label="Single Allocation\n(Immutable Structure)", color="#7aa2f7", fontcolor="#7aa2f7", penwidth=3];
    Sub0 [label="Upload Mip0", color="#414868"];
    Sub1 [label="Upload Mip1", color="#414868"];
    Draw [label="Draw\n(No Checks)", color="#9ece6a", fontcolor="#9ece6a", penwidth=3];

    Alloc -> Sub0;
    Alloc -> Sub1;
    Sub0 -> Draw;
    Sub1 -> Draw;
  }
}
\enddot

## 6. Performance Gains
-   Significantly reduced driver overhead.
-   Better memory locality (Mipmaps are often packed together).
-   Eliminates "Incomplete Texture" errors.
