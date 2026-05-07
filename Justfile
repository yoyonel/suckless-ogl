# SUCKLESS-OGL Justfile
# Use `just --list` to see available commands

set shell := ["bash", "-c"]

# Build variables
build_dir := "build"
# Job count: nproc - 2 locally (min 1), all cores in CI
nprocs := `
    if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
        nproc
    else
        N=$(nproc); if [ "$N" -gt 2 ]; then echo $((N - 2)); else echo 1; fi
    fi
`
distrobox := `
    if command -v distrobox >/dev/null 2>&1 && distrobox list --no-color 2>/dev/null | grep -w "clang-dev" >/dev/null; then
        echo "distrobox enter clang-dev --"
    else
        echo ""
    fi
`

# Smart detection for Python environment (Host vs Distrobox)
# Note: Justfile backticks are evaluated by shell, so we cannot use {{distrobox}} inside easily if variable interpolation is not supported in backticks.
# However, `just` DOES evaluate variables before backticks? No.
# So we must duplicate the check or use a simpler approach.
# We'll use a shell script snippet that mimics the makefile logic, checking for the container existence again or relying on a convention.
# Actually, we can rely on `command -v distrobox` again.
py_run := `
    if command -v distrobox >/dev/null 2>&1 && distrobox list --no-color 2>/dev/null | grep -w "clang-dev" >/dev/null; then
        echo "distrobox enter clang-dev -- python3"
    elif command -v uv >/dev/null 2>&1; then
        echo "uv run python3"
    elif [ -f .venv/bin/python3 ]; then
        echo ".venv/bin/python3"
    else
        echo "python3"
    fi
`

xvfb_wrapper := ".github/workflows/scripts/run_test_with_xvfb.sh"
extra_cmake_flags := ""

# ApiTrace configuration
apitrace_dir := env_var("HOME") / ".local/apitrace-latest-Linux"
apitrace_bin := `if [ -f "{{apitrace_dir}}/bin/apitrace" ]; then echo "{{apitrace_dir}}/bin/apitrace"; else echo "apitrace"; fi`

# Container engine detection
container_engine := `command -v docker >/dev/null 2>&1 && echo docker || echo podman`
image_name := "suckless-ogl"
ci_image_name := "suckless-ogl-ci:local"

# Default target
default:
    @just --list

# =============================================================================
# Build & Run Standard
# =============================================================================

# Configure CMake (Debug build)
configure:
    @{{distrobox}} cmake {{extra_cmake_flags}} -G "Unix Makefiles" -B {{build_dir}} -DCMAKE_BUILD_TYPE=Debug -DENABLE_NATIVE_ARCH=ON

# Build the project (Debug)
build:
    @if [ ! -d {{build_dir}} ]; then just configure; fi
    @{{distrobox}} cmake --build {{build_dir}} --parallel {{nprocs}}

# Completely remove the build directory
clean-all:
    @rm -rf {{build_dir}} build-*
    @rm -f build_*.log

# Build and run the application (Debug)
run: build
    @{{build_dir}}/app

# Build and run with software rendering (llvmpipe)
run-soft: build
    @LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe {{build_dir}}/app

# =============================================================================
# Build Variants
# =============================================================================

# Build for Maximum Speed (-O3, Native, FastMath, Stripped)
release:
    @{{distrobox}} cmake -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release -DENABLE_NATIVE_ARCH=ON
    @{{distrobox}} cmake --build {{build_dir}} --parallel {{nprocs}}

# Build and run in Release mode
run-release: release
    @{{build_dir}}/app

# Build for Extreme Performance (Unity Build, Native, Aggressive Math)
ultra-release:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-ultra -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_UNITY_BUILD=ON \
        -DENABLE_NATIVE_ARCH=ON \
        -DENABLE_AGGRESSIVE_MATH=ON
    @{{distrobox}} cmake --build build-ultra --parallel {{nprocs}}

# Build and run in UltraRelease mode
run-ultra-release: ultra-release
    @build-ultra/app

# Build for Minimum Size (-Os, Stripped)
small:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-small -DCMAKE_BUILD_TYPE=MinSizeRel
    @{{distrobox}} cmake --build build-small --parallel {{nprocs}}
    @ls -lh build-small/app

# Build and run the application in small mode
run-small: small
    @build-small/app

# Build with SSBO rendering (alternative path)
build-ssbo:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-ssbo -DCMAKE_BUILD_TYPE=Debug -DUSE_SSBO=ON
    @{{distrobox}} cmake --build build-ssbo --parallel {{nprocs}}

# Build and run with SSBO rendering
run-ssbo: build-ssbo
    @build-ssbo/app

# Clean SSBO-specific build
clean-ssbo:
    @rm -rf build-ssbo

# Build with Synchronous Debug (SLOW but safe)
build-sync:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-sync -DCMAKE_BUILD_TYPE=Debug -DENABLE_Gx_SYNC=ON
    @{{distrobox}} cmake --build build-sync --parallel {{nprocs}}

# Build and run with Synchronous Debug
run-sync: build-sync
    @build-sync/app

# Clean Sync Debug build
clean-sync:
    @rm -rf build-sync

# Build with optimizations and debug symbols (for profiling)
profile:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-profile -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_NATIVE_ARCH=ON
    @{{distrobox}} cmake --build build-profile --parallel {{nprocs}}
    @echo ""
    @echo "📊 Profile Build Verification Summary:"
    @{{distrobox}} sh -c ' \
    if [ -f build-profile/app ]; then \
        if file build-profile/app | grep -q "not stripped"; then echo "  ✓ Symbols: Not stripped"; else echo "  ✗ Symbols: Stripped!"; fi; \
        if nm -C build-profile/app 2>/dev/null | grep -q " T main"; then echo "  ✓ Symbols: Function names found"; else echo "  ✗ Symbols: Function names missing!"; fi; \
        if readelf -S build-profile/app 2>/dev/null | grep -q ".debug_info"; then echo "  ✓ Debug: .debug_info section present"; else echo "  ✗ Debug: .debug_info section missing!"; fi; \
        if readelf -S build-profile/app 2>/dev/null | grep -q ".debug_line"; then echo "  ✓ Debug: .debug_line section present"; else echo "  ✗ Debug: .debug_line section missing!"; fi; \
    else \
        echo "  ✗ Error: build-profile/app not found."; \
    fi'
    @echo ""

# Build and run Linux 'perf' profiler (requires root/capabilities)
perf: profile
    @{{distrobox}} perf record -g build-profile/app
    @{{distrobox}} perf report

# Clean profiling build
clean-profile:
    @rm -rf build-profile

# =============================================================================
# Testing & QA
# =============================================================================

# Run all tests via ctest (verbose on failure)
test-all: build
    @{{distrobox}} ctest --test-dir {{build_dir}} --output-on-failure

# Run tests. Usage: just test [pattern] (runs all if empty)
test pattern="": build
    @if [ -z "{{pattern}}" ]; then \
        just test-all; \
        just test-python; \
    else \
        {{distrobox}} sh -c '\
            TEST_BIN="{{build_dir}}/tests/{{pattern}}"; \
            if [ -f "$$TEST_BIN" ]; then \
                {{xvfb_wrapper}} "$$TEST_BIN"; \
            else \
                if ctest --test-dir {{build_dir}} -N -R "{{pattern}}" 2>/dev/null | grep -q "Total Tests: 0"; then \
                    echo "No tests found matching pattern: {{pattern}}"; \
                    echo "Available tests:"; \
                    ctest --test-dir {{build_dir}} -N 2>/dev/null | grep "Test #" | sed "s/.*: //"; \
                    exit 1; \
                else \
                    ctest --test-dir {{build_dir}} -R "{{pattern}}" --output-on-failure --verbose; \
                fi; \
            fi'; \
    fi

# Regenerate test reference images
test-gen-refs: build
    @GEN_REFS=1 {{xvfb_wrapper}} {{build_dir}}/tests/test_app
    @echo "[INFO] Optimizing reference images..."
    @if command -v mogrify >/dev/null 2>&1; then \
        mogrify -strip tests/references/ref_*.png; \
        echo "[SUCCESS] PNG references optimized."; \
    fi

# List all available tests
test-list:
    @{{distrobox}} ctest --test-dir {{build_dir}} -N 2>/dev/null | grep "Test #" | sed "s/.*: //"

# Run simple UI integration test (Default Debug build)
test-integration: build
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh {{build_dir}}/app

# Run UI integration test on Release build
test-integration-release: release
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh {{build_dir}}/app

# Run UI integration test on Profile build (RelWithDebInfo)
test-integration-profile: profile
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh build-profile/app

# Run UI integration test with SSBO rendering enabled
test-integration-ssbo: build-ssbo
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh build-ssbo/app

# Run UI integration test on Ultra Release build (LTO, Unity, Aggressive Math)
test-integration-ultra: ultra-release
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh build-ultra/app

# Run UI integration test on Small build (MinSizeRel)
test-integration-small: small
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh build-small/app

# Run UI integration test on Synchronous Debug build
test-integration-sync: build-sync
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh build-sync/app

# Run UI integration test with Tracy enabled (Debug)
test-integration-tracy: build-tracy
    @chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh ./build-tracy/app

# Run UI integration test under Valgrind (Minimal scenario, no -march=native)
test-integration-valgrind: build-valgrind
    @{{distrobox}} chmod +x scripts/test_integration_valgrind.sh
    @{{distrobox}} env VALGRIND_BUILD_DIR=build-valgrind bash scripts/test_integration_valgrind.sh

# Run UI integration test under Valgrind (Full scenario, no -march=native)
test-integration-valgrind-full: build-valgrind
    @{{distrobox}} chmod +x scripts/test_integration_valgrind.sh
    @{{distrobox}} env VALGRIND_BUILD_DIR=build-valgrind bash scripts/test_integration_valgrind.sh full

# Run full UI integration test under ASan
test-integration-asan: asan
    @{{distrobox}} chmod +x scripts/test_integration_asan.sh
    @{{distrobox}} bash scripts/test_integration_asan.sh

# Stress test: rapid fullscreen/windowed toggling to find deadlocks
stress-fullscreen iterations="100" delay="50": build
    @chmod +x scripts/test_stress_fullscreen.sh
    @{{distrobox}} bash scripts/test_stress_fullscreen.sh {{build_dir}}/app {{iterations}} {{delay}}

# Stress test fullscreen under ASan (slower but catches memory bugs)
stress-fullscreen-asan iterations="50" delay="100": asan
    @chmod +x scripts/test_stress_fullscreen.sh
    @{{distrobox}} bash scripts/test_stress_fullscreen.sh ./build-asan/app {{iterations}} {{delay}}

# Run programmatic ApiTrace performance verification
test-apitrace: build
    @{{distrobox}} chmod +x scripts/verify_apitrace_perf.sh
    @{{distrobox}} ./scripts/verify_apitrace_perf.sh {{apitrace_bin}}

# Run full integration test with ApiTrace analysis
test-integration-apitrace: build
    @{{distrobox}} chmod +x scripts/test_integration_apitrace.sh
    @{{distrobox}} ./scripts/test_integration_apitrace.sh {{apitrace_bin}}

# Generate HTML code coverage report (llvm-cov)
coverage:
    @echo "Building with coverage instrumentation..."
    @{{distrobox}} cmake {{extra_cmake_flags}} -G "Unix Makefiles" -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    @{{distrobox}} cmake --build build-coverage --parallel {{nprocs}}
    @echo "Running tests to generate profile data..."
    @{{distrobox}} sh -c "LLVM_PROFILE_FILE='{{justfile_directory()}}/build-coverage/test_%p.profraw' LIBGL_ALWAYS_SOFTWARE='1' GALLIUM_DRIVER='llvmpipe' ctest --test-dir build-coverage --output-on-failure"
    @echo "Merging profile data..."
    @{{distrobox}} llvm-profdata merge -sparse build-coverage/*.profraw -o build-coverage/coverage.profdata
    @echo "Generating HTML report..."
    @mkdir -p build-coverage/coverage_report
    @{{distrobox}} llvm-cov show -format=html \
        -instr-profile=build-coverage/coverage.profdata \
        build-coverage/app \
        $(find build-coverage/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
        -output-dir=build-coverage/coverage_report \
        -ignore-filename-regex='(tests/|include/|external/|deps/)'
    @echo "Summary Report:"
    @{{distrobox}} llvm-cov report \
        -instr-profile=build-coverage/coverage.profdata \
        build-coverage/app \
        $(find build-coverage/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
        -ignore-filename-regex='(tests/|include/|external/|deps/)'
    @echo "Coverage report generated in build-coverage/coverage_report/index.html"

# Generate visual regression report and serve it locally via HTTP (avoids file:// image loading issues)
# Uses tests/references/ref_*.png (and tests/references/failed_*.png if present) as input
# Press Ctrl+C to stop the server
visual-report port="8765":
    @echo "Generating visual regression report..."
    @mkdir -p build-coverage/coverage_report/visual_tests
    @{{py_run}} .github/workflows/scripts/generate_visual_report.py \
        "$(git rev-parse --short HEAD)" \
        "$(git remote get-url origin 2>/dev/null | sed 's|.*github.com[:/]||;s|\.git$$||' || echo 'local/repo')"
    @echo "Serving at http://localhost:{{port}}/ — press Ctrl+C to stop"
    @xdg-open "http://localhost:{{port}}/" 2>/dev/null &
    @cd build-coverage/coverage_report/visual_tests && python3 -m http.server {{port}}

# Build with AddressSanitizer (ASan)
asan:
    @echo "Building with AddressSanitizer (ASan)..."
    @mkdir -p build-asan
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UNITY_BUILD=OFF
    @{{distrobox}} cmake --build build-asan --parallel {{nprocs}}

# Build for Valgrind (no -march=native to avoid unsupported GFNI/AVX instructions)
build-valgrind:
    @echo "Building for Valgrind (no native arch)..."
    @mkdir -p build-valgrind
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-valgrind -DCMAKE_BUILD_TYPE=Debug -DENABLE_NATIVE_ARCH=OFF
    @{{distrobox}} cmake --build build-valgrind --parallel {{nprocs}}

# Run Valgrind (Default) to detect leaks/errors
memcheck: build-valgrind
    @{{distrobox}} valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose build-valgrind/app

# Run AddressSanitizer (ASan) to detect leaks/errors
memcheck-asan: asan
    @echo "Running Memory Check with AddressSanitizer (ASan)..."
    @{{distrobox}} env ASAN_OPTIONS="exitcode=1:detect_leaks=1:symbolize=1:halt_on_error=1" LSAN_OPTIONS="suppressions=lsan.supp" ./build-asan/app

# Full clean and rebuild from scratch
rebuild:
    @just clean-all
    @just build

# =============================================================================
# Dependencies & Tools
# =============================================================================

# Download dependencies for offline build
deps-setup:
    @chmod +x scripts/setup_offline_deps.sh
    @./scripts/setup_offline_deps.sh

# Remove the local dependency cache
deps-clean:
    @rm -rf deps/

# Verify build works without internet (requires unshare)
offline-test:
    @echo "Running build in a simulated offline environment..."
    @http_proxy=http://127.0.0.1:0 https_proxy=http://127.0.0.1:0 just rebuild

# Run Python script tests
test-python:
    @echo "Running Python script tests..."
    @{{py_run}} .github/workflows/scripts/test_trace_analyze.py

# Build the Docker image
docker-build:
    @{{container_engine}} build -t {{image_name}} .

# Build the Docker image without cache
docker-build-no-cache:
    @{{container_engine}} build --no-cache -t {{image_name}} .

# Build the CI Docker image
ci-docker-build:
    @{{container_engine}} build -t {{ci_image_name}} -f .github/workflows/Dockerfile.ci .

# Run shader test inside the CI container (verifying non-root permissions)
ci-docker-test: ci-docker-build
    @echo "Running shader test inside the CI container (verifying non-root permissions)..."
    @{{container_engine}} run --rm -v {{justfile_directory()}}:/workspace {{ci_image_name}} \
        sh -c "cmake {{extra_cmake_flags}} -B /tmp/build-ci -DCMAKE_C_FLAGS=-Wno-unused-variable && cmake --build /tmp/build-ci --target test_shader && cd /tmp/build-ci && xvfb-run -a ./tests/test_shader"

# Run the application container with X11 forwarding (software rendering)
docker-run:
    @echo "Running Container with X11 forwarding (software rendering)..."
    @xhost +local: > /dev/null 2>&1 || true
    @{{container_engine}} run --rm -it \
        --cap-add=SYS_NICE \
        --ulimit rtprio=99 \
        --security-opt label=disable \
        --network host \
        -e DISPLAY={{env_var("DISPLAY")}} \
        -e DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus" \
        -v /run/user/$(id -u)/bus:/run/user/$(id -u)/bus \
        -v /var/lib/dbus/machine-id:/var/lib/dbus/machine-id:ro \
        -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
        {{image_name}} /bin/bash -c "export DISPLAY={{env_var("DISPLAY")}} && ./app"

# Run the application container with host GPU passthrough (Intel/AMD via DRI)
docker-run-gpu:
    @echo "Running Container with GPU passthrough..."
    @xhost +local: > /dev/null 2>&1 || true
    @test -d /dev/dri || (echo "ERROR: /dev/dri not found — no GPU available" && exit 1)
    @{{container_engine}} run --rm -it \
        --device /dev/dri \
        --group-add video \
        --cap-add=SYS_NICE \
        --ulimit rtprio=99 \
        --security-opt label=disable \
        --network host \
        -e DISPLAY={{env_var("DISPLAY")}} \
        -e DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus" \
        -v /run/user/$(id -u)/bus:/run/user/$(id -u)/bus \
        -v /var/lib/dbus/machine-id:/var/lib/dbus/machine-id:ro \
        -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
        {{image_name}} /bin/bash -c "export DISPLAY={{env_var("DISPLAY")}} && ./app"

# Clean dangling images
docker-clean:
    @echo "Cleaning dangling images..."
    @{{container_engine}} image prune -f

# Clean all unused images and build cache
docker-clean-all:
    @echo "Cleaning all unused images and cache..."
    @{{container_engine}} system prune -a -f

# Show disk usage for the container engine
docker-usage:
    @echo "{{container_engine}} disk usage:"
    @{{container_engine}} system df

# Generate and verify Doxygen documentation
docs:
    @echo "Building MkDocs documentation..."
    @{{py_run}} -m mkdocs --version > /dev/null 2>&1 || ( \
        CMD=$(command -v uv > /dev/null 2>&1 && echo "uv pip" || echo "pip"); \
        echo "❌ Error: mkdocs not found. Install it with: $CMD install -r requirements-dev.txt" && exit 1)
    @{{py_run}} -m mkdocs build
    @echo "Generating Doxygen API Reference..."
    @{{distrobox}} command -v doxygen > /dev/null 2>&1 || ( \
        PKGMGR=$(command -v nala > /dev/null 2>&1 && echo "nala" || echo "apt"); \
        echo "❌ Error: doxygen not found. Install it with: sudo $PKGMGR install doxygen" && exit 1)
    @{{distrobox}} doxygen Doxyfile
    @echo "Documentation built in 'site/' directory (API at 'site/doxygen/html')."
    @echo "Verifying Documentation Quality..."
    @{{py_run}} scripts/verify_docs.py docs site/doxygen/html

# Clean all documentation build artifacts
docs-clean:
    @echo "Cleaning all documentation build artifacts..."
    @rm -rf site/
    @rm -rf docs/doxygen/
    @rm -rf docs/html/ docs/latex/ docs/xml/
    @echo "✓ Documentation artifacts removed"

# Build documentation in a strict offline environment
docs-offline:
    @echo "Building documentation in a strictly OFFLINE environment (unshare -rn)..."
    @unshare -rn just docs-clean docs
    @echo "✓ Offline build successful. No network was used."

# Verify Documentation Quality (ISO Makefile)
docs-verify:
    @echo "Verifying Documentation Quality..."
    @{{py_run}} scripts/verify_docs.py docs site/doxygen/html

# Serve full static site (MkDocs + Doxygen)
docs-serve:
    @echo "Serving full static site (MkDocs + Doxygen)..."
    @echo "Open http://localhost:8000"
    @{{py_run}} -m http.server -d site 8000

# Format code using clang-format and ruff
format:
    @echo "Formatting C and Shader files..."
    @{{distrobox}} sh -c 'find src include tests shaders -name "_deps" -prune -o -name "*.c" -print -o -name "*.h" -print -o -name "*.glsl" -print -o -name "*.vert" -print -o -name "*.frag" -print | xargs -P {{nprocs}} clang-format -i'
    @echo "Formatting Python scripts..."
    @{{distrobox}} ruff format scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py

# Lint code using clang-tidy, ruff, and GLSL validation
lint:
    @if [ ! -f {{build_dir}}/compile_commands.json ]; then {{distrobox}} cmake -B {{build_dir}} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; fi
    @{{distrobox}} python3 {{justfile_directory()}}/scripts/lint_incremental.py {{build_dir}}
    @{{distrobox}} ruff check scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py
    @{{distrobox}} bash scripts/lint_shaders.sh

# IWYU full scan (all src/*.c files — CI grade, ~2-3 min)
iwyu:
    @bash scripts/iwyu_check.sh --full --verbose {{build_dir}}

# IWYU check on files changed vs a base ref (default: origin/master)
iwyu-check base_ref="origin/master":
    @BASE_REF={{base_ref}} bash scripts/iwyu_check.sh --changed {{build_dir}}

# Lint shaders with strict SPIR-V validation (surfaces RenderDoc-class issues)
lint-shaders-strict:
    @{{distrobox}} bash scripts/lint_shaders.sh --strict

# Full linting with all features enabled (Tracy, SSBO, etc.)
lint-full:
    @if [ ! -f .lint_full/compile_commands.json ] || [ CMakeLists.txt -nt .lint_full/compile_commands.json ]; then \
        echo "Generating compile_commands.json with all features enabled..."; \
        mkdir -p .lint_full; \
        {{distrobox}} cmake -B .lint_full \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -DENABLE_TRACY=ON \
            -DUSE_SSBO_RENDERING=ON \
            -G "Unix Makefiles" > /dev/null; \
        {{distrobox}} cmake --build .lint_full --target glad-generate-files > /dev/null; \
    fi
    @echo "Linting C code (Full Coverage)..."
    @{{distrobox}} python3 {{justfile_directory()}}/scripts/lint_incremental.py .lint_full
    @echo "✓ Full linting passed"

# Check for new NOLINT suppressions introduced vs a base ref (default: origin/master)
check-nolint base_ref="origin/master":
    @bash scripts/check_nolint.sh {{base_ref}}

# Check dependency metrics (include fan-out thresholds)
dep-metrics:
    @bash scripts/check_dep_metrics.sh

# Trace Performance Analysis
trace-perf:
    @echo "Analyzing GPU performance (Advanced Analysis)..."
    @{{py_run}} scripts/trace_analyze.py {{build_dir}}/app.trace apitrace

# Clean build directory
clean:
    @if [ -f {{build_dir}}/Makefile ]; then {{distrobox}} cmake --build {{build_dir}} --target clean; else echo "Build directory not configured, skipping clean."; fi

# =============================================================================
# Tracy Profiler (v0.13.1)
# =============================================================================

tracy_legacy := `if [ "$XDG_SESSION_TYPE" = "wayland" ] || [ -n "$WAYLAND_DISPLAY" ]; then echo OFF; else echo ON; fi`
tracy_port := env_var_or_default("TRACY_PORT", `ss -tlnH sport = :8086 2>/dev/null | grep -q . && echo 8087 || echo 8086`)

# Build Tracy Server (X11 by default on Linux if LEGACY=ON)
build-tracy-server:
    @echo "Building Tracy Profiler Server (Legacy/X11: {{tracy_legacy}})..."
    @mkdir -p deps/tracy/profiler/build
    @{{distrobox}} cmake -B deps/tracy/profiler/build -S deps/tracy/profiler -DCMAKE_BUILD_TYPE=Release -DLEGACY={{tracy_legacy}}
    @{{distrobox}} cmake --build deps/tracy/profiler/build --parallel {{nprocs}}

# Run Tracy Server (auto-detects free port, override with TRACY_PORT=N)
tracy-server:
    @echo "Tracy server listening on port {{tracy_port}}"
    @./deps/tracy/profiler/build/tracy-profiler -a 127.0.0.1 -p {{tracy_port}}

# Build and run application with Tracy enabled (auto-detects free port)
run-tracy: build-tracy
    @echo "Tracy client connecting on port {{tracy_port}}"
    @TRACY_PORT={{tracy_port}} ./build-tracy/app

# Build application with Tracy enabled
build-tracy:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-tracy -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TRACY=ON
    @{{distrobox}} cmake --build build-tracy --parallel {{nprocs}}

# Build with Tracy AND AddressSanitizer
build-tracy-asan:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-tracy-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TRACY=ON -DENABLE_ASAN=ON -DENABLE_UNITY_BUILD=OFF
    @{{distrobox}} cmake --build build-tracy-asan --parallel {{nprocs}}

# Run integration test with Tracy AND ASan
test-integration-tracy-asan: build-tracy-asan
    @{{distrobox}} chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} bash scripts/test_integration_generic.sh ./build-tracy-asan/app

# Build with Tracy in Release mode
build-tracy-release:
    @{{distrobox}} cmake -G "Unix Makefiles" -B build-tracy-release -DCMAKE_BUILD_TYPE=Release -DENABLE_TRACY=ON
    @{{distrobox}} cmake --build build-tracy-release --parallel {{nprocs}}

# Run integration test with Tracy in Release mode
test-integration-tracy-release: build-tracy-release
    @{{distrobox}} chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} bash scripts/test_integration_generic.sh ./build-tracy-release/app

# Run application with Tracy enabled in Release mode
run-tracy-release: build-tracy-release
    @echo "Tracy client connecting on port {{tracy_port}}"
    @TRACY_PORT={{tracy_port}} ./build-tracy-release/app

# =============================================================================
# RenderDoc (Frame Analysis)
# =============================================================================

renderdoc_dir := env_var_or_default("RENDERDOC_DIR", "/usr/bin")

# Build the application in Debug mode for RenderDoc analysis
build-debug-renderdoc: configure
    @echo "Building Debug (RenderDoc profile)..."
    @{{distrobox}} cmake --build {{build_dir}} --parallel {{nprocs}}

# Launch qrenderdoc GUI with Debug build (Usage: just renderdoc_dir=/path/to/renderdoc renderdoc)
renderdoc: build-debug-renderdoc
    @{{renderdoc_dir}}/qrenderdoc --working-dir . ./{{build_dir}}/app

# Capture a frame via renderdoccmd CLI (Usage: just renderdoc_dir=/path/to/renderdoc renderdoc-capture)
renderdoc-capture: build-debug-renderdoc
    @{{renderdoc_dir}}/renderdoccmd capture --working-dir . ./{{build_dir}}/app

# =============================================================================
# Windows / Cross-Compilation (MinGW + Wine)
# =============================================================================

build_win_dir := "build-win"

# Configure for Windows (MinGW)
configure-win:
    @{{distrobox}} cmake -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw.cmake -B {{build_win_dir}} -DBUILD_TESTS=ON .

# Build for Windows
build-win:
    @if [ ! -d {{build_win_dir}} ]; then just configure-win; fi
    @{{distrobox}} cmake --build {{build_win_dir}} --parallel {{nprocs}}

# Run for Windows via Wine
run-win: build-win
    @{{distrobox}} wine {{build_win_dir}}/app.exe

# Run integration tests for Windows via Wine
test-win: build-win
    @{{distrobox}} chmod +x scripts/test_integration_generic.sh
    @{{distrobox}} ./scripts/test_integration_generic.sh wine {{build_win_dir}}/app.exe

# Run unit tests for Windows via Wine (parity with CI)
test-win-unit: build-win
    @{{distrobox}} env WINEPREFIX=$HOME/.wine WINEDEBUG=-all TEST_RUNNER_PREFIX="wine64" ctest --test-dir {{build_win_dir}} --output-on-failure
