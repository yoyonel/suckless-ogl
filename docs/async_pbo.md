# Asynchronous Texture Upload Strategy

This document details the implementation of the asynchronous high-resolution texture upload system in `suckless-ogl`, specifically focusing on the **Double-Buffered Persistent Pixel Buffer Object (PBO)** strategy used to eliminate main-thread stalling.

## The Problem

Uploading large 4K HDR textures (approx. 64MB) to the GPU is a heavy operation.

- **Direct Upload (`glTexImage2D`)**: Blocks the driver and main thread until the copy is complete (~50ms+), causing massive frame drops.
- **Naive PBO**: Using a single PBO allows asynchronous DMA transfer, but the *mapping* of that PBO (`glMapBufferRange`) can still block if the GPU is currently reading from it (Implicit Synchronization).

## The Solution: Double-Buffered Persistent PBOs

To ensure the Main Thread *never* waits for the GPU, we use a **Ping-Pong** strategy with two persistent PBOs.

### Architecture

1. **Async Worker Thread**:
    - Loads the HDR file from disk (I/O).
    - Decodes to float buffer.
    - **Waits** for the Main Thread to provide a mapped GPU pointer.
    - Converts Float -> Half-Float (FP16) *directly* into the mapped memory.

2. **Main Thread (`app_update`)**:
    - Checks if the worker is waiting.
    - Selects the **next available PBO** (index `frame % 2`).
    - **Maps** the PBO with `GL_MAP_UNSYNCHRONIZED_BIT`.
    - Passes the pointer to the worker.
    - When worker finishes, **Unmaps** and calls `glTexSubImage2D`.

### Key Optimizations

#### 1. Double Buffering & Unsynchronized Mapping

By alternating between `upload_pbo[0]` and `upload_pbo[1]`, we guarantee that while the GPU is reading from PBO 0 (for the previous texture), we are mapping and writing to PBO 1.
This allows us to use `GL_MAP_UNSYNCHRONIZED_BIT`, which tells the driver: *"I promise I am not overwriting data you are currently using, so don't check, just give me the pointer immediately."*

#### 2. Persistent Allocation (No Orphaning)

Previously, we used `glBufferData(NULL)` (Orphaning) to force the driver to give us a new memory chunk. While this avoids synchronization, the *allocation itself* for 64MB took ~26-40ms on certain drivers.
**Current Approach**: We allocate the PBOs once (or resize only if larger textures are loaded). We reuse the existing VRAM storage, eliminating allocation overhead.

#### 3. 2-Step Upload

Instead of fully converting on the specific thread and then copying, we:

1. **Load** (Worker)
2. **Map** (Main Thread)
3. **Convert & Write** (Worker, directly into PBO)
4. **Upload** (Main Thread, DMA)

This prevents the Main Thread from ever touching the pixel data on the CPU, and prevents the Worker from needing a GL context.

## Evolution of the Implementation

### Phase 1: Naive Async (Blocked)

Initially, the worker converted data to a CPU buffer, and the Main Thread called `glBufferData`.

- **Result**: Main thread blocked for ~45ms during allocation/copy.

### Phase 2: Single PBO + Orphaning (Stalled)

We moved to PBOs, but reused a single PBO. We tried forcing `glBufferData(NULL)` to orphan.

- **Result**: `PBO Setup` allocated memory every frame, taking ~26-40ms. Frame time exploded.

### Phase 3: Single PBO + Sync (Blocked)

We tried removing orphaning.

- **Result**: `glMapBufferRange` blocked heavily because the GPU was still reading the previous upload. Implicit synchronization kicked in.

### Phase 4: Double-Buffered Persistent (Final)

We implemented `upload_pbo[2]`.

- **Result**: `PBO Setup` dropped to **< 0.1ms**. The stall is completely gone, and we maintain 4k HDR streaming at full framerate.

## Code References

- **`src/app.c`**: Manages the PBO array loop in `app_update`.
- **`src/texture.c`**: `texture_ensure_pbo` (sizing) and `texture_map_pbo` (flags).
- **`src/async_loader.c`**: Handles the threading state machine (`WAITING_FOR_PBO`).
