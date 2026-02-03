
# User Guide: RenderDoc on Debian 13 (Intel Iris Xe)

This guide explains how to install and use RenderDoc to profile and verify the GPU performance of `suckless-ogl`.

## 1. Installation on Debian 13

The `renderdoc` package has been removed from Debian Testing (Trixie) repositories. The most reliable (and up-to-date) method is to use the official binary:

1.  **Download**: Go to [renderdoc.org](https://renderdoc.org/builds) and download the latest stable version for **Linux 64-bit**.
1.  **Extract**:
    ```sh
    tar -xvf renderdoc_*.tar.gz
    cd renderdoc_*
    ```
2.  **Launch**:
    ```sh
    ./bin/qrenderdoc
    ```
    *(Optional) You can add the `bin` folder to your PATH or create a symbolic link to `/usr/local/bin/qrenderdoc`.*

## 2. Profiling Configuration

1.  Launch the GUI: `qrenderdoc`.
2.  Go to the **"Launch Application"** tab.
3.  Configure the paths:
    *   **Executable Path**: `./build-small/app`
    *   **Working Directory**: `.`
4.  Check the following options (recommended for profiling):
    *   `Capture Child Processes`
    *   `Ref All Resources` (useful to see all HDR textures even if not bound at that moment)

## 3. Capturing a Loading Event

Environment loading is asynchronous and is triggered via the `Page Up` / `Page Down` keys.

1.  Click **"Launch"** in RenderDoc.
2.  In your application, get ready to switch environments.
3.  Press **F12** (or `Print Screen`) in the application to capture a frame.
    *   *Note: Since IBL loading takes several frames (~500ms), you may need to take several successive captures to hit the exact frame where the Compute Shaders are running.*
4.  A thumbnail appears in RenderDoc. Double-click it to open.

## 4. Performance Analysis (Ground Truth)

Once the capture is open:

1.  Open the **"Event Browser"** window (`Window` -> `Event Browser`).
2.  Look for `glDispatchCompute` calls. These correspond to your IBL (Luminance, Specular, Irradiance).
3.  Click the **Clock** icon (Time durations) at the top of the Event Browser.
    *   RenderDoc will replay the frame multiple times to get a precise GPU measurement.
4.  **Verification**: Compare the value in the `Duration` column with your `perf.hybrid` logs.
    *   If RenderDoc indicates `325,450 us` (microseconds), this corresponds to `325.45 ms`.

## 5. Specific Intel / Mesa Tips

*   **Pipeline Details**: In the **"Pipeline State"** tab, you can see exactly which shader is used, the "Dispatch Thread Groups", and bound textures.
*   **HDR Visualization**: In the **"Texture Viewer"**, you can inspect your `RGBA16F` textures. Use the exposure slider at the top of the window to "see" details in very bright areas.
*   **Debug Shaders**: You can click "Edit" on a shader in RenderDoc, modify a formula, and "Refresh" to see the visual and performance impact instantly without recompiling your C project.
