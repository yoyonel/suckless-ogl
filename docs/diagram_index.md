# Index des Diagrammes

*Cette page est générée automatiquement. **Survolez les titres** pour voir un aperçu du diagramme.*

<style>
.diagram-item { position: relative; display: block; padding: 12px 0; border-bottom: 1px solid var(--md-code-bg-color); }
.mermaid-preview {
  opacity: 0;
  visibility: hidden;
  position: absolute;
  left: max(300px, 30%);
  top: -80px;
  z-index: 999;
  background: #1a1b26;
  border: 2px solid #7aa2f7;
  padding: 24px;
  border-radius: 12px;
  box-shadow: 0 15px 55px rgba(0,0,0,0.9);
  width: 750px;
  max-height: 600px;
  overflow: auto;
  pointer-events: none;
  transition: opacity 0.3s cubic-bezier(0.4, 0, 0.2, 1), transform 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  transform: translateX(30px) scale(0.95);
}
.diagram-item:hover .mermaid-preview {
  opacity: 1;
  visibility: visible;
  transform: translateX(0) scale(1);
}
.mermaid-preview .mermaid { background: transparent !important; color: white !important; }
</style>

## [Asynchronous Environment Map Loader](../async_loader/)

<div class="diagram-item">
  <a href="../async_loader/#scheduling-sequence" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Scheduling Sequence</a> : <span style="opacity: 0.6; font-size: 0.85em;">The complexity of the PBO-based approach is handled by spliting the work across several frames:</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"primaryColor": "#24283b",
"primaryTextColor": "#ffffff",
"primaryBorderColor": "#7aa2f7",
"lineColor": "#7aa2f7",
"signalColor": "#ffffff",
"signalTextColor": "#ffffff",
"messageColor": "#ffffff",
"messageTextColor": "#ffffff",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"actorBorder": "#7aa2f7",
"actorBkg": "#24283b",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26"
}
}%%
sequenceDiagram
participant M as Main Thread
participant W as Worker Thread
participant G as GPU / VRAM
Note over M,W: Frame 1
M->>W: Request Load (path)
W->>W: I/O Read + Decode (CPU RAM)
Note over M,W: Frame N (Worker finishes I/O)
W-->>M: State = WAITING_FOR_PBO
M->>M: Map PBO (Unsynchronized)
M->>W: Pass PBO Pointer
Note over M,W: Frame N+1 (Worker does SIMD)
W->>W: SIMD Convert (F32 to F16) into PBO
W-->>M: State = READY
Note over M,W: Frame N+2 (Final Integration)
M->>M: Unmap PBO
M->>G: glTexSubImage2D (Fast DMA)
M->>G: glGenerateMipmap
M->>M: Start Progressive IBL
```

  </div>
</div>


## [Asynchronous Texture Upload Strategy](../async_pbo/)

<div class="diagram-item">
  <a href="../async_pbo/#architecture-overview" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Architecture Overview</a> : <span style="opacity: 0.6; font-size: 0.85em;">- Monolithic Upload: Even with PBOs, performing all GPU work (texture storage allocation, data upload, mipmap generation) in a single frame creates a ~60ms spike.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"signalTextColor": "#ffffff",
"messageTextColor": "#ffffff",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26",
"lineColor": "#7aa2f7"
}
}%%
sequenceDiagram
participant Main as Main Thread
participant Worker as Async Worker
participant GPU as GPU / Driver
Note over Main: Frame N - PBO Setup
Main->>GPU: texture_ensure_pbo() + texture_map_pbo()
Main->>Worker: async_loader_provide_pbo(mapped_ptr)
Note over Main: Frame N+1 - VRAM Pre-allocation
Main->>GPU: texture_preallocate_hdr()<br/>glTexImage2D(level 0, NULL)
Note over GPU: Allocate ~64MB base level only
Note over Worker: Frames N..N+M - Background Conversion
Worker->>Worker: float32 -> float16 (SIMD)<br/>directly into mapped PBO
Note over Main: Frame N+M - Upload & Mipmaps
Main->>GPU: glUnmapBuffer(PBO)
Main->>GPU: glTexSubImage2D(from PBO)
Main->>GPU: glGenerateMipmap()
Note over GPU: DMA transfer + mipmap chain
```

  </div>
</div>

<div class="diagram-item">
  <a href="../async_pbo/#the-strategy-spread-work-across-3-frames" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">The Strategy: Spread Work Across 3 Frames</a> : <span style="opacity: 0.6; font-size: 0.85em;">Instead of doing everything in one frame, we distribute the work using the async loader's multi-step protocol as natural frame boundaries:</span>
  <div class="mermaid-preview">

```mermaid
gantt
title Frame Time Distribution
dateFormat X
axisFormat %s ms
section Before (1 frame)
PBO Setup + TexStorage + Upload + Mipmap + 3×glGetError :done, 0, 60
section After (3 frames)
Frame N  - PBO Setup & Map        :active, 0, 5
Frame N+1 - TexPrealloc (level 0) :active, 8, 15
Frame N+M - Upload + Mipmap       :active, 18, 38
```

  </div>
</div>

<div class="diagram-item">
  <a href="../async_pbo/#deferred-pre-allocation-flow" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Deferred Pre-allocation Flow</a> : <span style="opacity: 0.6; font-size: 0.85em;">Cost: ~20-30ms (irreducible GPU work)</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"primaryColor": "#24283b",
"primaryTextColor": "#ffffff",
"primaryBorderColor": "#7aa2f7",
"lineColor": "#7aa2f7",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"actorBorder": "#7aa2f7",
"actorBkg": "#24283b",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26"
}
}%%
flowchart TD
A["app_update() called"] --> B{"pending_prealloc_w > 0?"}
B -- Yes --> C["texture_preallocate_hdr()"]
C --> D{"recycled_hdr_tex matches?"}
D -- Yes --> E["Zero-cost reuse (OK)"]
D -- No --> F["glTexImage2D(level 0, NULL)"]
F --> G["Store in app->recycled_hdr_tex"]
B -- No --> H["async_loader_poll()"]
E --> H
G --> H
H --> I{"req.state?"}
I -- WAITING_FOR_PBO --> J["PBO Setup & Map"]
J --> K["Schedule pending_prealloc_w/h"]
I -- ASYNC_READY --> L["texture_upload_hdr_from_pbo()"]
L --> M{"reuse_tex matches?"}
M -- Yes --> N["Skip glTexStorage2D (OK)"]
M -- No --> O["Fallback: glTexStorage2D"]
N --> P["glUnmapBuffer + glTexSubImage2D"]
O --> P
P --> Q["glGenerateMipmap"]
```

  </div>
</div>

<div class="diagram-item">
  <a href="../async_pbo/#why-glgeterror-stalls-the-pipeline" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Why `glGetError()` Stalls the Pipeline</a> : <span style="opacity: 0.6; font-size: 0.85em;">`glGetError()` is a synchronous query: the CPU must wait for the GPU to process all pending commands before returning the error state. In a pipelined architecture, this defeats the purpose of asynchronous uploads.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"signalTextColor": "#ffffff",
"messageTextColor": "#ffffff",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26",
"lineColor": "#7aa2f7"
}
}%%
sequenceDiagram
participant CPU
participant CmdQueue as GPU Command Queue
participant GPU
CPU->>CmdQueue: glTexSubImage2D (async, returns immediately)
CPU->>CmdQueue: glGetError() -> STALL
Note over CPU: (Waiting) Blocked waiting for GPU
CmdQueue->>GPU: Execute TexSubImage...
GPU-->>CmdQueue: Done
CmdQueue-->>CPU: GL_NO_ERROR
Note over CPU: Can finally continue
```

  </div>
</div>


## [Environment Transitions](../env_transitions/)

<div class="diagram-item">
  <a href="../env_transitions/#state-machine" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">State Machine</a> : <span style="opacity: 0.6; font-size: 0.85em;">Transitions are governed by a state machine in `src/appenv.c`.</span>
  <div class="mermaid-preview">

```mermaid
stateDiagram-v2
state "  IDLE  " as idle
state "  LOADING  " as loading
state "  FADE_OUT  " as fade_out
state "  FADE_IN  " as fade_in
state "  WAIT_IBL  " as wait_ibl
idle --> loading : app_trigger_env_transition
loading --> fade_out : IBL Done (Black Screen Mode)
loading --> fade_in : IBL Done (Crossfade Mode)
fade_out --> fade_in : Alpha >= 1.0 (Swap Textures)
fade_in --> idle : Alpha <= 0.0
idle --> wait_ibl : Initial Startup
wait_ibl --> fade_in : IBL Done (Initial Load)
```

  </div>
</div>


## [Aggressive test with ASan](../fullscreen_deadlock/)

<div class="diagram-item">
  <a href="../fullscreen_deadlock/#sequence-before-deadlock" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Sequence Before (DEADLOCK)</a> : <span style="opacity: 0.6; font-size: 0.85em;">   The Application is blocked inside the resize callback, trying to allocate/delete GPU resources, but the driver's command queue is often locked or stalled during the mode switch handshake.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "base",
"themeVariables": {
"primaryColor": "#7aa2f7",
"primaryTextColor": "#ffffff",
"primaryBorderColor": "#7aa2f7",
"lineColor": "#9aa5ce",
"secondaryColor": "#f7768e",
"tertiaryColor": "#1a1b26",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26",
"actorBkg": "#24283b",
"actorBorder": "#7aa2f7",
"actorTextColor": "#ffffff",
"actorLineColor": "#7aa2f7",
"labelBoxBkgColor": "#1a1b26",
"labelBoxBorderColor": "#7aa2f7",
"labelTextColor": "#ffffff",
"loopTextColor": "#ffffff",
"messageTextColor": "#ffffff",
"signalTextColor": "#ffffff",
"activationBkgColor": "#414868",
"sequenceNumberColor": "#ffffff"
}
}%%
sequenceDiagram
participant Main as Main Thread
participant GLFW as GLFW
participant Driver as NVIDIA Driver
participant GPU as GPU Pipeline
Main->>GLFW: glfwPollEvents()
GLFW->>Main: key_callback(F)
Main->>GLFW: glfwSetWindowMonitor()
GLFW->>Driver: Mode switch request
Note over Driver: Waits for GPU fence...
Driver-->>GLFW: Resize event
GLFW->>Main: framebuffer_size_callback()
Main->>GPU: glDeleteTextures / glGenTextures
Note over GPU,Driver: GPU blocked by pending swap
Note over Main,GPU: (DEADLOCK)
```

  </div>
</div>

<div class="diagram-item">
  <a href="../fullscreen_deadlock/#sequence-after-fixed" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Sequence After (FIXED)</a> : <span style="opacity: 0.6; font-size: 0.85em;">The solution is a Deferred Resize pattern, which decouples the window manager's resize event from the expensive GPU resource recreation.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "base",
"themeVariables": {
"primaryColor": "#7aa2f7",
"primaryTextColor": "#ffffff",
"primaryBorderColor": "#7aa2f7",
"lineColor": "#9aa5ce",
"secondaryColor": "#f7768e",
"tertiaryColor": "#1a1b26",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26",
"actorBkg": "#24283b",
"actorBorder": "#7aa2f7",
"actorTextColor": "#ffffff",
"actorLineColor": "#7aa2f7",
"labelBoxBkgColor": "#1a1b26",
"labelBoxBorderColor": "#7aa2f7",
"labelTextColor": "#ffffff",
"loopTextColor": "#ffffff",
"messageTextColor": "#ffffff",
"signalTextColor": "#ffffff",
"activationBkgColor": "#414868",
"sequenceNumberColor": "#ffffff"
}
}%%
sequenceDiagram
participant Main as Main Thread
participant GLFW as GLFW
participant Driver as NVIDIA Driver
participant GPU as GPU Pipeline
Main->>GPU: glFinish() - drain pipeline
GPU-->>Main: All commands complete
Main->>GLFW: glfwSetWindowMonitor()
GLFW->>Driver: Mode switch request
Driver-->>GLFW: Resize event
GLFW->>Main: framebuffer_size_callback()
Note over Main: Only stores dimensions + flag
Main-->>GLFW: Return immediately
GLFW-->>Main: glfwSetWindowMonitor() returns
Note over Main: Next frame begins...
Main->>Main: app_run: resize_pending? YES
Main->>GPU: postprocess_resize() - safe context
GPU-->>Main: FBOs recreated (OK)
```

  </div>
</div>


## [Global Illumination (1-Bounce)](../global_illumination/)

<div class="diagram-item">
  <a href="../global_illumination/#architecture-de-limplementation" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Architecture de l'Implémentation</a> : <span style="opacity: 0.6; font-size: 0.85em;">Le système combine un calcul CPU asynchrone et un échantillonnage GPU sans aucune interruption (stall) du thread de rendu principal.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"signalTextColor": "#ffffff",
"messageTextColor": "#ffffff",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26",
"lineColor": "#7aa2f7"
}
}%%
sequenceDiagram
participant Main as Thread Principal (CPU)
participant Worker as GI Worker Thread (CPU)
participant GPU as SSBO & Shaders (GPU)
Main->>Worker: Envoie une copie de la scène (Positions, Couleurs)
Main->>Worker: Signale une mise à jour (CondVar)
activate Worker
Note over Worker: Calcule le Form Factor et projette en Harmoniques Sphériques (SH)
Worker-->>Main: Signale que les calculs sont terminés (results_ready)
deactivate Worker
Main->>GPU: Upload des données SH vers le SSBO (glBufferSubData)
Main->>GPU: Appel de dessin (Instanced ou SSBO)
Note over GPU: Les fragments échantillonnent l'irradiance des 8 sondes adjacentes (Trilinear Filtering)
```

  </div>
</div>


## [Performance Mode & Notifications](../perf_and_notifications/)

<div class="diagram-item">
  <a href="../perf_and_notifications/#architecture" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Architecture</a> : <span style="opacity: 0.6; font-size: 0.85em;">2.  Native: Fallback using Linux scheduling syscalls (`schedsetscheduler`, `setpriority`).</span>
  <div class="mermaid-preview">

```mermaid
classDiagram
class App {
+perf_context
+perf_mode_active
+init()
+cleanup()
}
class PerfModeContext {
+state
+backend
+original_policy
+original_param
+original_nice
+initialized
}
class Backend
<<interface>> Backend
Backend : +activate()
Backend : +deactivate()
class GameModeBackend {
+libgamemode_init
}
class NativeBackend {
+sched_setscheduler_FIFO
+setpriority_nice
}
App *-- PerfModeContext
PerfModeContext ..> GameModeBackend : Tries_First
PerfModeContext ..> NativeBackend : Fallback
```

  </div>
</div>

<div class="diagram-item">
  <a href="../perf_and_notifications/#design" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Design</a> : <span style="opacity: 0.6; font-size: 0.85em;">-   FIFO Replacement: If the buffer is full, the oldest active notification is overwritten.</span>
  <div class="mermaid-preview">

```mermaid
sequenceDiagram
participant User
participant AppInput
participant ActionNotifier
participant UI
User->>AppInput: Press Key (e.g., F9)
AppInput->>AppInput: Toggle Feature (PerfMode)
AppInput->>ActionNotifier: action_notifier_push("Perf Mode: ON", 2.0s)
activate ActionNotifier
ActionNotifier->>ActionNotifier: Find free slot / Overwrite oldest
ActionNotifier->>ActionNotifier: Copy text (safe_strncpy)
ActionNotifier-->>AppInput: Done
deactivate ActionNotifier
loop Every Frame
AppInput->>ActionNotifier: action_notifier_update(dt)
ActionNotifier->>ActionNotifier: Increase lifetime, deactivate if expired
AppInput->>ActionNotifier: action_notifier_draw(ui_ctx)
ActionNotifier->>UI: ui_draw_text_ex(...)
end
```

  </div>
</div>


## [Platform Abstraction Layer (PAL)](../portability_pal/)

<div class="diagram-item">
  <a href="../portability_pal/#architecture" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Architecture</a> : <span style="opacity: 0.6; font-size: 0.85em;">The PAL acts as an intermediary between the core application logic and the underlying Operating System.</span>
  <div class="mermaid-preview">

```mermaid
graph TD
subgraph Core Application
A[log.c]
B[scene.c]
C[perf_mode.c]
end
subgraph PAL [Platform Abstraction Layer]
D[platform_utils.h]
E[platform_time.h]
F[platform_fs.h]
end
subgraph OS Backends
G[Linux / POSIX]
H[Windows API]
I[macOS / Darwin]
end
A --> D
A --> E
B --> F
B --> D
C --> E
D -.-> G
D -.-> H
E -.-> G
E -.-> H
F -.-> G
F -.-> H
```

  </div>
</div>


## [Progressive & Asynchronous IBL Architecture](../progressive_ibl/)

<div class="diagram-item">
  <a href="../progressive_ibl/#d-specular-map-1024x1024" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">D. Specular Map (1024x1024)</a> : <span style="opacity: 0.6; font-size: 0.85em;">Total &quot;Tail Grouping&quot;: Grouping small mips (3 to 10) avoids wasting 7 frames of latency for tiny jobs (&lt;1ms each).</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"primaryColor": "#24283b",
"primaryTextColor": "#ffffff",
"primaryBorderColor": "#7aa2f7",
"lineColor": "#7aa2f7",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"actorBorder": "#7aa2f7",
"actorBkg": "#24283b",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26"
}
}%%
flowchart LR
subgraph HeavyGroup["Heavy Workload Sliced"]
Mip0["Mip 0 4 Frames"]
Mip1["Mip 1 2 Frames"]
end
subgraph LightGroup["Fast Workload Grouped"]
Mip2["Mip 2 1 Frame"]
Tail["Mips 3-10 1 Frame"]
end
Start(["Start"]) --> Mip0
Mip0 --> Mip1
Mip1 --> Mip2
Mip2 --> Tail
Tail --> End(["Done"])
style HeavyGroup fill:#24283b,stroke:#f7768e,stroke-dasharray: 5, 5
style LightGroup fill:#24283b,stroke:#9ece6a
style Start fill:#7aa2f7,color:#ffffff
style End fill:#9ece6a,color:#ffffff
style Mip0 fill:#414868,stroke:#f7768e
style Mip1 fill:#414868,stroke:#f7768e
style Mip2 fill:#414868,stroke:#9ece6a
style Tail fill:#414868,stroke:#9ece6a
```

  </div>
</div>

<div class="diagram-item">
  <a href="../progressive_ibl/#43-solution-single-deferred-barrier" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">4.3 Solution: Single Deferred Barrier</a> : <span style="opacity: 0.6; font-size: 0.85em;">coherency path is flushed.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"signalTextColor": "#ffffff",
"messageTextColor": "#ffffff",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26",
"lineColor": "#7aa2f7"
}
}%%
sequenceDiagram
participant CPU
participant GPU
Note over CPU,GPU: Old approach (per-slice barrier)
loop Each Slice
CPU->>GPU: glDispatchCompute()
CPU->>GPU: glMemoryBarrier(ALL_BARRIER_BITS)
Note right of GPU: Pipeline drain + cache flush
end
Note over CPU,GPU: New approach (deferred barrier)
loop Each Slice
CPU->>GPU: glDispatchCompute()
Note right of GPU: Work queued, no stall
end
CPU->>GPU: glMemoryBarrier(IMAGE_ACCESS_BIT)
Note right of GPU: Single flush before sampling
```

  </div>
</div>


## [Synchronization & Asynchrony Overview](../synchronization_overview/)

<div class="diagram-item">
  <a href="../synchronization_overview/#frame-scheduling-task-interleaving" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Frame Scheduling & Task Interleaving</a> : <span style="opacity: 0.6; font-size: 0.85em;">The following diagram illustrates how the various asynchronous and progressive tasks are interleaved within the main application loop to avoid frame spikes.</span>
  <div class="mermaid-preview">

```mermaid
%%{init: {
"theme": "dark",
"themeVariables": {
"primaryColor": "#24283b",
"primaryTextColor": "#ffffff",
"primaryBorderColor": "#7aa2f7",
"lineColor": "#7aa2f7",
"signalColor": "#ffffff",
"signalTextColor": "#ffffff",
"messageColor": "#ffffff",
"messageTextColor": "#ffffff",
"labelTextColor": "#ffffff",
"actorTextColor": "#ffffff",
"actorBorder": "#7aa2f7",
"actorBkg": "#24283b",
"noteBkgColor": "#e0af68",
"noteTextColor": "#1a1b26"
}
}%%
sequenceDiagram
participant M as Main Thread
participant W as Worker Threads
participant G as GPU
Note over M: Frame N starts
M->>M: 1. Poll Async Loader (5ms)
M->>M: 2. Update IBL Slice (10ms)
par Parallel Execution
W->>W: Background I/O & SH Projection
M->>G: 3. Upload GI Probes (5ms)
end
M->>G: 4. Render Scene (15ms)
M->>G: 5. Swap Buffers (5ms)
Note over M: Frame N ends (~40ms)
Note over M: Frame N+1 starts
M->>M: Poll & Process...
```

  </div>
</div>


## [UI Visual Parameters Reference](../ui_visual_parameters/)

<div class="diagram-item">
  <a href="../ui_visual_parameters/#hover-decay-stabilization" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">Hover Decay Stabilization</a> : <span style="opacity: 0.6; font-size: 0.85em;">Logic parameters that ensure a smooth &quot;Premium&quot; feel during mouse or keyboard usage.</span>
  <div class="mermaid-preview">

```mermaid
graph LR
A[Mouse over Key] --> B[Target Dim: 0.3]
B --> C{Mouse leaves?}
C -- Yes --> D[Wait 150ms]
D -- Still Empty --> E[Target Dim: 1.0]
D -- Enters New Key --> B
```

  </div>
</div>
