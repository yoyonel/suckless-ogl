# Steam & Proton Integration: Deployment and Debugging

**Timestamp:** 2026-06-26 17:10:09 CEST
**Stack:** Custom C11 Engine (OpenGL), Linux, Flatpak Steam, Proton Experimental, Justfile.

This documentation consolidates best practices and necessary workarounds to reliably package, test, and profile a native Win64 application under a Linux environment via Steam Flatpak and Proton.

---

## 1. "Suckless" Binary Architecture (CWD & Logs)

The majority of silent crashes under Proton are caused by incorrect Current Working Directory (CWD) handling by the Steam interface, or by the suppression of `stdout`/`stderr` output streams when compiling with `-mwindows` (which disables the console).

To make the engine robust regardless of its launcher (CLI, Desktop Shortcut, Steam), it must self-manage its CWD and secure its own logs at startup.

In our project, this logic is delegated to platform utilities to keep the application entry point simple.

**Implementation in `src/platform/platform_fs.c`:**

```c
#include "platform/platform_fs.h"
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #include <windows.h>
    #define CHDIR _chdir
    #define ACCESS _access
    #define F_OK 0
#else
    #include <unistd.h>
    #define CHDIR chdir
    #define ACCESS access
    #define F_OK 0
#endif

void platform_setup_working_dir(const char* exec_path) {
    if (!exec_path || strlen(exec_path) == 0) {
        return;
    }

    // 1. If mandatory resources are already present, do nothing
    if (ACCESS("shaders", F_OK) == 0 && ACCESS("assets", F_OK) == 0) {
        return;
    }

    // 2. Resolve the absolute path of the executable
    char path[1024];
    strncpy(path, exec_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    // Find the last directory separator (Windows or Unix)
    char* last_slash = strrchr(path, '\\');
    if (!last_slash) {
        last_slash = strrchr(path, '/');
    }

    if (last_slash) {
        *last_slash = '\0';
        CHDIR(path);
    }

    // 3. Move up to 4 parent levels if needed (useful for development mode)
    for (int i = 0; i < 4; ++i) {
        if (ACCESS("shaders", F_OK) == 0 && ACCESS("assets", F_OK) == 0) {
            break;
        }
        CHDIR("..");
    }
}
```

**Secure Entry Point in `src/main.c`:**

```c
#include "platform/platform_fs.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    // 1. Secure working directory and locate resources
    if (argc > 0) {
        platform_setup_working_dir(argv[0]);
    }

    // 2. Preserve log streams (Crucial with -mwindows)
    // Steam removes terminal access. These logs will be generated
    // next to the executable to guarantee visibility on errors.
    FILE* dummy_err = freopen("suckless_crash.log", "w", stderr);
    FILE* dummy_out = freopen("suckless_output.log", "w", stdout);
    (void)dummy_err;
    (void)dummy_out;

    // Engine initialization and main rendering loop
    // ...
    return EXIT_SUCCESS;
}
```

---

## 2. Compilation and Automated CLI Validation (Justfile)

To build and validate a release without manually going through the Steam GUI, an environment-agnostic automated validation command is provided. The `run-package-win` target invokes a helper script (`scripts/run_proton.sh`) that automatically detects if your environment runs Steam via Flatpak or natively, configures the environment, ensures the Proton prefix exists, and launches the application under Proton.

During local testing of the package, the Proton prefix is created inside the git-ignored `test-dist/proton_pfx` directory to keep the testing environment self-contained. For the direct `run-proton` target, the prefix is defined in Steam's `compatdata` folder to prevent workspace pollution.

### Useful Commands

- **Configure cross-compilation**:
  ```bash
  just configure-win
  ```
- **Compilation**:
  ```bash
  just build-win
  ```
- **Package for distribution** (atomically compressed archive with Zstandard):
  ```bash
  just package-win
  ```
- **Automated release validation**:
  ```bash
  just run-package-win
  ```

**Proton Test Targets Definition in `Justfile`:**

```justfile
steam_root := env_var("HOME") + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"
proton_path := steam_root + "/steamapps/common/Proton - Experimental/proton"
proton_prefix := steam_root + "/steamapps/compatdata/suckless-ogl"

# Run the development Win64 binary directly under Proton (Flatpak only)
run-proton: build-win
    @echo "Création du préfixe Proton..."
    @mkdir -p "{{ proton_prefix }}"
    @echo "Lancement via Steam Flatpak..."
    @flatpak run \
        --filesystem="{{ justfile_directory() }}" \
        --env=STEAM_COMPAT_CLIENT_INSTALL_PATH="{{ steam_root }}" \
        --env=STEAM_COMPAT_DATA_PATH="{{ proton_prefix }}" \
        --command=python3 \
        com.valvesoftware.Steam \
        "{{ proton_path }}" run "{{ justfile_directory() }}/{{ build_win_dir }}/app.exe"

# Agnostic Automated Validation
run-package-win: package-win
    @echo "Lancement de l'environnement de test automatisé..."
    @scripts/run_proton.sh "{{ release_dir }}" "{{ release_name }}" "{{ test_dist_dir }}" "{{ justfile_directory() }}"
```

**Steam Environment Detection Wrapper (`scripts/run_proton.sh`):**

```bash
#!/usr/bin/env bash
# scripts/run_proton.sh
set -e

# Arguments passed by the Justfile
RELEASE_DIR="$1"
RELEASE_NAME="$2"
TEST_DIST_DIR="$3"
PROJECT_ROOT="$4"

EXTRACTED_APP="${TEST_DIST_DIR}/${RELEASE_NAME}/app.exe"
PROTON_PFX="${TEST_DIST_DIR}/proton_pfx"

echo "==> 1. Cleaning and extracting release archive..."
rm -rf "${TEST_DIST_DIR}"
mkdir -p "${TEST_DIST_DIR}"
tar -I 'zstd' -xf "${RELEASE_DIR}/../${RELEASE_NAME}.tar.zst" -C "${TEST_DIST_DIR}"

# Ensure prefix directory exists before running Proton (Flatpak or Native)
mkdir -p "${PROTON_PFX}"

echo "==> 2. Detecting Steam environment..."

# Test A: Presence of a Flatpak environment
if command -v flatpak &>/dev/null && flatpak info com.valvesoftware.Steam &>/dev/null; then
    echo "    [i] Detected environment: Steam Flatpak (Sandbox)"
    STEAM_ROOT="${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam"
    PROTON_PATH="${STEAM_ROOT}/steamapps/common/Proton - Experimental/proton"

    echo "==> 3. Launching engine via Flatpak..."
    cd "${TEST_DIST_DIR}/${RELEASE_NAME}"
    flatpak run \
        --filesystem="${PROJECT_ROOT}" \
        --env=STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT}" \
        --env=STEAM_COMPAT_DATA_PATH="${PROTON_PFX}" \
        --command=python3 \
        com.valvesoftware.Steam \
        "${PROTON_PATH}" run "${EXTRACTED_APP}"

# Test B: Presence of a Native environment (Bazzite, Fedora, Arch...)
elif [ -d "${HOME}/.local/share/Steam" ]; then
    echo "    [i] Detected environment: Steam Native"
    STEAM_ROOT="${HOME}/.local/share/Steam"
    PROTON_PATH="${STEAM_ROOT}/steamapps/common/Proton - Experimental/proton"

    echo "==> 3. Launching engine via Native Proton..."

    cd "${TEST_DIST_DIR}/${RELEASE_NAME}"
    STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT}" \
        STEAM_COMPAT_DATA_PATH="${PROTON_PFX}" \
        "${PROTON_PATH}" run "${EXTRACTED_APP}"

else
    echo "    [x] Error: No Steam installation found (neither Flatpak nor Native)."
    exit 1
fi
```


---

## 3. Importing into Steam GUI & The Flatpak Portal Pitfall

When adding a non-Steam game via the Steam client GUI (using the "Add a Game" button), understanding Flatpak's FUSE file permission handling is crucial.

### The FUSE "Black Hole" (Error 193)
Never use recent shortcuts or the virtual `Documents/` folder in the Steam file browser. Doing so invokes the *XDG Desktop Portal*, which mounts the target file as read-only inside `/run/user/1000/doc/...`.

This causes two major issues:
1. Proton cannot resolve the actual directory structure containing the shaders and assets, leading to an immediate crash at startup.
2. Read access to the executable can be truncated to **0 bytes**, triggering Win32 error `193` (`ERROR_BAD_EXE_FORMAT`).

### The Reliable Method
1. In the Steam browser, physically navigate from the root `/` to the absolute path under your user folder:
   `/home/USER/Prog/.../test-dist/suckless-ogl-windows-v0.1.0/app.exe`
2. Open the shortcut's **Properties > Compatibility** settings in Steam, check **Force the use of a specific Steam Play compatibility tool**, and select **Proton Experimental** (or Proton 9.x).
3. In **Properties > Shortcut**, ensure that absolute paths in the **Target** and **Start In** fields are correctly wrapped in double quotes `" "`.

---

## 4. Advanced Profiling and Overlay (MangoHud / OpenGL)

Unlike Vulkan applications, MangoHud does not automatically inject into OpenGL processes via simple global environment variables. Library injection (`LD_PRELOAD`) must be forced.

To analyze engine performance (frametimes, CPU/GPU load, VRAM utilization) using the MangoHud overlay under Steam, configure the following line in the **Launch Options** of your Steam shortcut:

```text
STEAM_COMPAT_MOUNTS="/absolute/path/to/test-dist/" WINEPREFIX="%compat%" mangohud %command%
```

* **`STEAM_COMPAT_MOUNTS`**: Forces Valve's secure container runtime (*Pressure Vessel*) to mount the development directory on the host, ensuring that Proton can read from and write to it freely (especially to write the `suckless_output.log` and `suckless_crash.log` files).
* **`mangohud %command%`**: Explicitly runs the MangoHud wrapper prefix before the Proton execution command, successfully intercepting the OpenGL context created by the Windows binary.
