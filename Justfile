# SUCKLESS-OGL Justfile
# Use `just --list` to see available commands

set shell := ["bash", "-c"]

# Build variables
build_dir := "build"
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

# Default target
default:
    @just --list

# =============================================================================
# Build & Run Standard
# =============================================================================

# Configure CMake (Debug build)
configure:
    @{{distrobox}} cmake -B {{build_dir}} -DCMAKE_BUILD_TYPE=Debug

# Build the project (Debug)
build:
    @{{distrobox}} cmake --build {{build_dir}} --parallel

# Completely remove the build directory
clean-all:
    @rm -rf {{build_dir}} build-ssbo build-sync build-small build-docs build-coverage

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
    @{{distrobox}} cmake -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release
    @{{distrobox}} cmake --build {{build_dir}} --parallel

# Build and run in Release mode
run-release: release
    @{{build_dir}}/app

# Build for Minimum Size (-Os, Stripped)
small:
    @{{distrobox}} cmake -B build-small -DCMAKE_BUILD_TYPE=MinSizeRel
    @{{distrobox}} cmake --build build-small --parallel
    @ls -lh build-small/app

# Build and run the application in small mode
run-small: small
    @build-small/app

# Build with SSBO rendering (alternative path)
build-ssbo:
    @{{distrobox}} cmake -B build-ssbo -DCMAKE_BUILD_TYPE=Debug -DUSE_SSBO=ON
    @{{distrobox}} cmake --build build-ssbo --parallel

# Build and run with SSBO rendering
run-ssbo: build-ssbo
    @build-ssbo/app

# Clean SSBO-specific build
clean-ssbo:
    @rm -rf build-ssbo

# Build with Synchronous Debug (SLOW but safe)
build-sync:
    @{{distrobox}} cmake -B build-sync -DCMAKE_BUILD_TYPE=Debug -DENABLE_Gx_SYNC=ON
    @{{distrobox}} cmake --build build-sync --parallel

# Build and run with Synchronous Debug
run-sync: build-sync
    @build-sync/app

# Clean Sync Debug build
clean-sync:
    @rm -rf build-sync

# Build with optimizations and debug symbols (for profiling)
profile:
    @{{distrobox}} cmake -B {{build_dir}} -DCMAKE_BUILD_TYPE=RelWithDebInfo
    @{{distrobox}} cmake --build {{build_dir}} --parallel

# Build and run Linux 'perf' profiler (requires root/capabilities)
perf: profile
    @{{distrobox}} perf record -g {{build_dir}}/app
    @{{distrobox}} perf report

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

# List all available tests
test-list:
    @{{distrobox}} ctest --test-dir {{build_dir}} -N 2>/dev/null | grep "Test #" | sed "s/.*: //"

# Run full UI integration test under Valgrind (Default)
test-integration: release
    @{{distrobox}} chmod +x scripts/test_integration_valgrind.sh
    @{{distrobox}} bash scripts/test_integration_valgrind.sh

# Run full UI integration test under ASan
test-integration-asan: asan
    @{{distrobox}} chmod +x scripts/test_integration_asan.sh
    @{{distrobox}} bash scripts/test_integration_asan.sh

# Generate HTML code coverage report (llvm-cov)
coverage:
    @echo "Building with coverage instrumentation..."
    @{{distrobox}} cmake -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE=ON -DCMAKE_C_COMPILER=clang
    @{{distrobox}} cmake --build build-coverage --parallel
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
        -ignore-filename-regex='(tests/|include/|external/)'
    @echo "Coverage report generated in build-coverage/coverage_report/index.html"

# Build with AddressSanitizer (ASan)
asan:
    @echo "Building with AddressSanitizer (ASan)..."
    @mkdir -p build-asan
    @{{distrobox}} cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UNITY_BUILD=OFF
    @{{distrobox}} cmake --build build-asan --parallel

# Run Valgrind (Default) to detect leaks/errors
memcheck: build
    @{{distrobox}} valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose {{build_dir}}/app

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
    @docker build -t suckless-ogl .

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

# Verify Documentation Quality (ISO Makefile)
docs-verify:
    @echo "Verifying Documentation Quality..."
    @{{py_run}} scripts/verify_docs.py docs site/doxygen/html

# Format code using clang-format and ruff
format:
    @find src include tests -name "*.[ch]" | xargs clang-format -i
    @{{distrobox}} ruff format scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py

# Lint code using clang-tidy and ruff
lint:
    @{{distrobox}} cmake -B {{build_dir}} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    @{{distrobox}} run-clang-tidy -p {{build_dir}} -header-filter='.*' -checks='-*,readability-*,bugprone-*,performance-*,portability-*,modernize-*' src/*.c include/*.h tests/*.c
    @{{distrobox}} ruff check scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py

# Trace Performance Analysis
trace-perf:
    @echo "Analyzing GPU performance (Advanced Analysis)..."
    @{{py_run}} scripts/trace_analyze.py {{build_dir}}/app.trace apitrace

# Clean build directory
clean:
    @{{distrobox}} cmake --build {{build_dir}} --target clean

# =============================================================================
# Tracy Profiler (v0.13.1)
# =============================================================================

tracy_legacy := `if [ "$XDG_SESSION_TYPE" = "wayland" ] || [ -n "$WAYLAND_DISPLAY" ]; then echo OFF; else echo ON; fi`

# Build Tracy Server (X11 by default on Linux if LEGACY=ON)
build-tracy-server:
    @echo "Building Tracy Profiler Server (Legacy/X11: {{tracy_legacy}})..."
    @mkdir -p deps/tracy/profiler/build
    @{{distrobox}} cmake -B deps/tracy/profiler/build -S deps/tracy/profiler -DCMAKE_BUILD_TYPE=Release -DLEGACY={{tracy_legacy}}
    @{{distrobox}} cmake --build deps/tracy/profiler/build --parallel

# Run Tracy Server
tracy-server:
    @./deps/tracy/profiler/build/tracy-profiler

# Build and run application with Tracy enabled
run-tracy: build-tracy
    @./build-tracy/app

# Build application with Tracy enabled
build-tracy:
    @{{distrobox}} cmake -B build-tracy -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TRACY=ON
    @{{distrobox}} cmake --build build-tracy --parallel
