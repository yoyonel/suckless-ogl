# Asynchronous Environment Map Loader

This document describes the asynchronous loading system implemented to handle heavy HDR environment maps without blocking the main application loop.

## Overview

Loading high-resolution HDR textures (e.g., 4k or 8k `.hdr` files) can take several hundred milliseconds or even seconds depending on disk speed. Performing this operation on the main thread causes the entire application to freeze (stop rendering and processing input), leading to a poor user experience.

The **Async Loader** decouples the **Disk I/O and CPU decompression** steps from the **Main Thread**, moving them to a background worker thread.

## Architecture

The system consists of three main components:

1.  **Async Loader Module** (`src/async_loader.c`)
    *   Manages a background worker thread (pthread).
    *   Maintains a single "request slot" protected by a mutex.
    *   Handles the `stbi_loadf` operation (Disk -> RAM).

2.  **Texture Loading Split** (`src/texture.c`)
    *   `texture_load_pixels`: Pure CPU function (Thread-safe). Loads raw float data.
    *   `texture_upload_hdr`: Pure OpenGL function (Main thread only). Uploads data to GPU.

3.  **Application Integration** (`src/app.c`)
    *   Initiates requests without blocking.
    *   **GPU Stall**: While Disk I/O is offloaded, the final OpenGL upload and IBL map generation (Prefiltering, Irradiance) still occur on the main thread. This may cause a slight frame drop *after* the loading finishes. This is expected behavior for this implementation phase.
    *   **Integrated Graphics**: On some integrated GPUs (e.g., Intel Iris Xe), the Compute Shader may time out for the 1024x1024 level (Mip 0). An optimization has been added to `spmap.glsl` to skip convolution for roughness ~0 and perform a direct copy instead.
    *   Polls for completion in the main loop.
    *   Finalizes the upload and generation pipeline on the main thread.

### Data Flow

```graphviz
digraph AsyncSequence {
  rankdir=TB;
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

  subgraph cluster_main {
    label="Main Thread";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#7aa2f7";
    style="rounded";
    color="#7aa2f7";
    margin=20;
    AppUpdate [label="app_update()"];
    Request [label="Async Request\n(Path)", color="#7aa2f7", fontcolor="#7aa2f7", penwidth=3];
    Poll [label="Check Status?", shape=diamond, color="#e0af68", fontcolor="#e0af68"];
    Upload [label="Upload to GPU\n(Main Context)", color="#9ece6a", fontcolor="#9ece6a", penwidth=3];
  }

  subgraph cluster_worker {
    label="Async Worker";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#f7768e";
    style="rounded,dashed";
    color="#f7768e";
    margin=20;
    Idle [label="Idle Wait", color="#414868"];
    Load [label="File I/O\n(stbi_load)", color="#f7768e", fontcolor="#f7768e"];
    Decode [label="Decode RGB", color="#f7768e", fontcolor="#f7768e"];
    Ready [label="Set State=READY", color="#9ece6a", fontcolor="#9ece6a"];
  }

  AppUpdate -> Request [label="User Input"];
  Request -> Idle [label="Signal (Mutex)", style=dashed, color="#f7768e"];
  Idle -> Load;
  Load -> Decode;
  Decode -> Ready;

  Poll -> Ready [label="Is Ready?", style=dotted];
  Ready -> Upload [label="Data Transfer", color="#9ece6a", penwidth=2];

  Upload -> GenIBL [label="Start Progressive\nGeneration"];
  GenIBL [label="PBR compute...", color="#bb9af7", fontcolor="#bb9af7", style="rounded,dashed"];
}
```
