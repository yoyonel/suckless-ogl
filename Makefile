# Makefile wrapper for CMake-based build system

BUILD_DIR := build
CMAKE := cmake
EXTRA_CMAKE_FLAGS :=

# On définit BOX pour garder la configuration de ton env local
BOX := clang-dev

# Smart detection: Use distrobox only if available and if the container exists
DISTROBOX_CMD := $(shell command -v distrobox 2> /dev/null)
ifneq ($(DISTROBOX_CMD),)
    # Check if the specific box exists
    BOX_EXISTS := $(shell distrobox list --no-color 2>/dev/null | grep -w "$(BOX)")
    ifneq ($(BOX_EXISTS),)
         CONTAINER_RUN_DEFAULT := distrobox enter $(BOX) --
    endif
endif

# If CONTAINER_RUN is not defined, use the detected default (or empty/local)
# To disable distrobox even if available, pass CONTAINER_RUN=""
CONTAINER_RUN ?= $(CONTAINER_RUN_DEFAULT)

# On remplace l'ancienne variable par la nouvelle
DISTROBOX := $(CONTAINER_RUN)

# Smart detection for Python environment (Host vs Distrobox)
# Priority: Distrobox > UV (host) > .venv (host) > system
ifeq ($(DISTROBOX),)
    UV_CMD := $(shell command -v uv 2> /dev/null)
    ifneq ($(UV_CMD),)
        PY_RUN := uv run python3
        TOOL_RUN := uv run
    else ifneq ($(wildcard .venv/bin/python3),)
        PY_RUN := .venv/bin/python3
        TOOL_RUN := .venv/bin/
    else
        PY_RUN := python3
        TOOL_RUN :=
    endif
else
    PY_RUN := $(DISTROBOX) python3
    TOOL_RUN := $(DISTROBOX)
endif

#
APITRACE_DIR := $(HOME)/Téléchargements/apitrace-latest-Linux
APITRACE_WRAPPERS := $(APITRACE_DIR)/lib/apitrace/wrappers
APITRACE_BIN := $(APITRACE_DIR)/bin/apitrace

#
BUILD_PROF_DIR := build-prof
BUILD_REL_DIR := build-release
BUILD_SMALL_DIR := build-small
BUILD_ASAN_DIR := build-asan

.PHONY: all clean clean-all rebuild run help format lint deps-setup deps-clean offline-test docker-build test test-one test-list test-integration coverage release small debug-release docs docs-clean asan build-win test-win

# Job count: nproc - 2 locally (min 1), all cores in CI
ifneq ($(CI),)
    NPROCS := $(shell nproc)
else ifneq ($(GITHUB_ACTIONS),)
    NPROCS := $(shell nproc)
else
    NPROCS := $(shell N=$$(nproc); if [ "$$N" -gt 2 ]; then echo $$(($$N - 2)); else echo 1; fi)
endif

all: $(BUILD_DIR)/Makefile
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --parallel $(NPROCS)

$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_DIR) -G "Unix Makefiles"

clean:
	@if [ -d $(BUILD_DIR) ]; then $(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --target clean; fi
	@$(DISTROBOX) rm -rf $(BUILD_DIR)
	@rm -rf .lint_cache

docs:
	@echo "Building MkDocs documentation..."
	@$(PY_RUN) -m mkdocs --version > /dev/null 2>&1 || ( \
		CMD=$$(command -v uv > /dev/null 2>&1 && echo "uv pip" || echo "pip"); \
		echo "❌ Error: mkdocs not found. Install it with: $$CMD install -r requirements-dev.txt" && exit 1)
	@$(PY_RUN) -m mkdocs build
	@echo "Generating Doxygen API Reference..."
	@$(DISTROBOX) command -v doxygen > /dev/null 2>&1 || ( \
		PKGMGR=$$(command -v nala > /dev/null 2>&1 && echo "nala" || echo "apt"); \
		echo "❌ Error: doxygen not found. Install it with: sudo $$PKGMGR install doxygen" && exit 1)
	@$(DISTROBOX) doxygen Doxyfile
	@echo "Documentation built in 'site/' directory (API at 'site/doxygen/html')."
	@echo "Verifying Documentation Quality..."
	@$(PY_RUN) scripts/verify_docs.py docs site/doxygen/html

docs-serve:
	@echo "Serving full static site (MkDocs + Doxygen)..."
	@echo "Open http://localhost:8000"
	@$(PY_RUN) -m http.server -d site 8000

docs-dev:
	@echo "Starting MkDocs Live Preview (No Doxygen integration)..."
	@$(PY_RUN) -m mkdocs serve

docs-legacy:
	@echo "Generating Doxygen documentation (Legacy)..."
	@$(DISTROBOX) doxygen Doxyfile

docs-clean:
	@echo "Cleaning all documentation build artifacts..."
	@rm -rf site/
	@rm -rf docs/doxygen/
	@rm -rf docs/html/ docs/latex/ docs/xml/
	@echo "✓ Documentation artifacts removed"

clean-all: docs-clean
	@echo "Removing all build directories..."
	@rm -rf $(BUILD_DIR) $(BUILD_COV_DIR) $(BUILD_PROF_DIR) $(BUILD_ASAN_DIR) build-ssbo build-tracy
	@rm -f build_*.log

rebuild: clean-all all

run: all
	@./$(BUILD_DIR)/app

run-release: release
	@./$(BUILD_REL_DIR)/app

run-small: small
	@./$(BUILD_SMALL_DIR)/app

run-software: all
	LIBGL_ALWAYS_SOFTWARE=1 ./$(BUILD_DIR)/app

format:
	$(DISTROBOX) sh -c "find src include tests shaders -name \"_deps\" -prune -o -name \"*.c\" -print -o -name \"*.h\" -print -o -name \"*.glsl\" -print -o -name \"*.vert\" -print -o -name \"*.frag\" -print | xargs clang-format -i"
	@echo "Formatting Python scripts..."
	@$(TOOL_RUN) ruff format scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py

format-check:
	@echo "Checking C and Shader formatting..."
	@$(DISTROBOX) sh -c "find src include tests shaders -name \"_deps\" -prune -o -name \"*.c\" -print -o -name \"*.h\" -print -o -name \"*.glsl\" -print -o -name \"*.vert\" -print -o -name \"*.frag\" -print | xargs clang-format --dry-run --Werror"
	@echo "Checking Python scripts formatting..."
	@$(TOOL_RUN) ruff format --check scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py
	@echo "✓ Formatting check passed"

# Resolve dependency paths for linting
# We check if 'deps' exists (offline mode), otherwise fall back to build/_deps
STB_INC := $(shell [ -d deps/stb ] && echo deps/stb || echo build/_deps/stb-src)
CGLM_INC := $(shell [ -d deps/cglm ] && echo deps/cglm/include || echo build/_deps/cglm-src/include)
GLAD_INC := build/_deps/glad-build/include
CJSON_INC := $(shell [ -d deps/cjson ] && echo deps/cjson || echo build/_deps/cjson-src)

# Static analysis wrapper (handle cltcache if present)
CLT_CMD := $(shell $(DISTROBOX) command -v cltcache 2>/dev/null)
CLANG_TIDY := $(if $(CLT_CMD),$(CLT_CMD) clang-tidy,clang-tidy)

lint-deps: $(BUILD_DIR)/compile_commands.json
	@echo "Ensuring generated headers are ready..."
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --target glad --parallel $(NPROCS)

lint: lint-deps
	@echo "Linting C code (Incremental)..."
	@$(DISTROBOX) python3 scripts/lint_incremental.py $(BUILD_DIR)
	@echo "Linting Python scripts..."
	@$(TOOL_RUN) ruff check scripts/trace_analyze.py .github/workflows/scripts/test_trace_analyze.py || (echo "⚠️  Install ruff: $$CMD install ruff" && exit 1)
	@echo "✓ All linting passed"

lint-clean:
	@echo "Cleaning lint cache..."
	@rm -rf .lint_cache*

# Ensure compile_commands.json is up to date before linting
$(BUILD_DIR)/compile_commands.json: $(BUILD_DIR)/Makefile
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Full linting with all features enabled (Tracy, SSBO, etc.)
LINT_FULL_DIR := .lint_full
LINT_FULL_JSON := $(LINT_FULL_DIR)/compile_commands.json

$(LINT_FULL_JSON): CMakeLists.txt Makefile
	@echo "Generating compile_commands.json with all features enabled..."
	@mkdir -p $(LINT_FULL_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(LINT_FULL_DIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DENABLE_TRACY=ON \
		-DUSE_SSBO_RENDERING=ON \
		-G "Unix Makefiles" > /dev/null

lint-full: $(LINT_FULL_JSON)
	@echo "Ensuring generated headers are ready..."
	@$(DISTROBOX) $(CMAKE) --build $(LINT_FULL_DIR) --target glad --parallel $(NPROCS) > /dev/null
	@echo "Linting C code (Full Coverage)..."
	@$(DISTROBOX) python3 scripts/lint_incremental.py $(LINT_FULL_DIR)
	@echo "✓ Full linting passed"

deps-setup:
	@chmod +x scripts/setup_offline_deps.sh
	@./scripts/setup_offline_deps.sh

deps-clean:
	@echo "Removing offline dependency cache..."
	@rm -rf deps/

offline-test:
	@echo "Running build in a simulated offline environment (using bogus proxy)..."
	@http_proxy=http://127.0.0.1:0 https_proxy=http://127.0.0.1:0 $(MAKE) rebuild

test-python:
	@echo "Running Python script tests..."
	@$(PY_RUN) .github/workflows/scripts/test_trace_analyze.py

test: all test-python
	@echo "Running C/C++ unit tests..."
	@$(DISTROBOX) ctest --test-dir $(BUILD_DIR) --output-on-failure

test-list:
	@$(DISTROBOX) ctest --test-dir $(BUILD_DIR) -N 2>/dev/null | grep "Test #" | sed "s/.*: //"

test-one:
	@$(MAKE) --no-print-directory all > /dev/null 2>&1
ifndef TEST
	@echo "Usage: make test-one TEST=<name>  (or: make test/<name>)"
	@echo ""
	@echo "Available tests:"
	@$(MAKE) --no-print-directory test-list
else
	@$(MAKE) --no-print-directory test/$(TEST)
endif

test/%:
	@$(MAKE) --no-print-directory all > /dev/null 2>&1
	@$(DISTROBOX) sh -c '\
		TEST_BIN="$(BUILD_DIR)/tests/$*"; \
		if [ -f "$$TEST_BIN" ]; then \
			.github/workflows/scripts/run_test_with_xvfb.sh "$$TEST_BIN"; \
		else \
			if ctest --test-dir $(BUILD_DIR) -N -R "$*" 2>/dev/null | grep -q "Total Tests: 0"; then \
				echo "No tests found matching pattern: $*"; \
				exit 1; \
			else \
				ctest --test-dir $(BUILD_DIR) -R "$*" --output-on-failure --verbose; \
			fi; \
		fi' || $(MAKE) --no-print-directory test-list

# Regenerate test reference images
test-gen-refs: all
	@GEN_REFS=1 .github/workflows/scripts/run_test_with_xvfb.sh $(BUILD_DIR)/tests/test_app

# Code Coverage (improved version with summary)
BUILD_COV_DIR := build-coverage
REPORT_DIR := $(BUILD_COV_DIR)/coverage_report

$(BUILD_COV_DIR):
	@mkdir -p $(BUILD_COV_DIR)

coverage: $(BUILD_COV_DIR)
	@echo "Building with coverage instrumentation..."
	@$(DISTROBOX) $(CMAKE) $(EXTRA_CMAKE_FLAGS) -B $(BUILD_COV_DIR) -DCODE_COVERAGE=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_COV_DIR) --parallel $(NPROCS)

	@echo "Running tests to generate profile data..."
	@$(DISTROBOX) sh -c "LLVM_PROFILE_FILE='$(CURDIR)/$(BUILD_COV_DIR)/test_%p.profraw' LIBGL_ALWAYS_SOFTWARE='1' GALLIUM_DRIVER='llvmpipe' ctest --test-dir $(BUILD_COV_DIR) --output-on-failure"

	@echo "Merging profile data..."
	@$(DISTROBOX) llvm-profdata merge -sparse $(BUILD_COV_DIR)/*.profraw -o $(BUILD_COV_DIR)/coverage.profdata

	@echo "Generating HTML report..."
	@mkdir -p $(REPORT_DIR)
	@$(DISTROBOX) llvm-cov show -format=html \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$(BUILD_COV_DIR)/app \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-output-dir=$(REPORT_DIR) \
		-ignore-filename-regex='(tests/|include/|external/|deps/)'
	@echo "Report generated at: $(REPORT_DIR)/index.html"

	@echo "Coverage Summary:"
	@$(DISTROBOX) llvm-cov report \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$(BUILD_COV_DIR)/app \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-ignore-filename-regex='(tests/|include/|external/|deps/)' | tee $(BUILD_COV_DIR)/coverage_summary.txt

	@echo "Running Python coverage..."
	@$(TOOL_RUN) pytest .github/workflows/scripts/test_trace_analyze.py --cov=scripts --cov-report=html:$(REPORT_DIR)/python_coverage --cov-report=term || \
		(echo "⚠️  Install pytest-cov: $$CMD install pytest-cov" && exit 1)
	@echo "Python coverage report: $(REPORT_DIR)/python_coverage/index.html"

apitrace: profile
	@echo "Running Apitrace..."
	LD_PRELOAD=$(APITRACE_WRAPPERS)/glxtrace.so \
		$(APITRACE_BIN) \
			trace \
			--api gl \
			--output $(BUILD_PROF_DIR)/app.trace \
			./$(BUILD_PROF_DIR)/app

trace-perf:
	@echo "Analyzing GPU performance (Advanced Analysis)..."
	python3 scripts/trace_analyze.py $(BUILD_PROF_DIR)/app.trace $(APITRACE_BIN)

qapitrace:
	@echo "Running Qapitrace..."
	$(DISTROBOX) qapitrace ./$(BUILD_PROF_DIR)/app.trace

# --- Profiling Build (Optimized + Debug Symbols) ---
.PHONY: profile perf

profile:
	@echo "Building for profiling (RelWithDebInfo + LTO)..."
	@mkdir -p $(BUILD_PROF_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_PROF_DIR) -DCMAKE_BUILD_TYPE=Profiling
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_PROF_DIR) --parallel $(NPROCS)

perf: profile
	@echo "Running perf record..."
	@# On utilise --call-graph dwarf pour avoir des stack traces propres avec les symboles
	@$(DISTROBOX) perf record --call-graph dwarf ./$(BUILD_PROF_DIR)/app
	@$(DISTROBOX) perf report

asan:
	@echo "Building with AddressSanitizer (ASan)..."
	@mkdir -p $(BUILD_ASAN_DIR)
	@$(DISTROBOX) $(CMAKE) $(EXTRA_CMAKE_FLAGS) -B $(BUILD_ASAN_DIR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_ASAN=ON \
		-DENABLE_UNITY_BUILD=OFF \
		-G "Unix Makefiles"
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_ASAN_DIR) --parallel $(NPROCS)

run-asan: asan
	@echo "Running with AddressSanitizer..."
	@$(DISTROBOX) env ASAN_OPTIONS="exitcode=1:detect_leaks=1:symbolize=1:halt_on_error=1" LSAN_OPTIONS="suppressions=lsan.supp" ./$(BUILD_ASAN_DIR)/app

# Docker Integration
# Auto-detect container engine (podman or docker)
CONTAINER_ENGINE := $(shell command -v docker 2> /dev/null || echo podman)
IMAGE_NAME := suckless-ogl

docker-build:
	$(CONTAINER_ENGINE) build \
		-t $(IMAGE_NAME) .

# --- CI Docker Integration ---
CI_IMAGE_NAME := suckless-ogl-ci:local

ci-docker-build:
	$(CONTAINER_ENGINE) build \
		-t $(CI_IMAGE_NAME) \
		-f .github/workflows/Dockerfile.ci .

ci-docker-test: ci-docker-build
	@echo "Running shader test inside the CI container (verifying non-root permissions)..."
	@$(CONTAINER_ENGINE) run --rm -v $(CURDIR):/workspace $(CI_IMAGE_NAME) \
		sh -c "cmake $(EXTRA_CMAKE_FLAGS) -B /tmp/build-ci -DCMAKE_C_FLAGS=-Wno-unused-variable && cmake --build /tmp/build-ci --target test_shader && cd /tmp/build-ci && xvfb-run -a ./tests/test_shader"

docker-build-no-cache:
	$(CONTAINER_ENGINE) build \
		--no-cache \
		-t $(IMAGE_NAME) .

docker-run:
	@echo "Running Container with X11 forwarding..."
	xhost +local: > /dev/null 2>&1 || true
	$(CONTAINER_ENGINE) run --rm -it \
		--cap-add=SYS_NICE \
		--ulimit rtprio=99 \
		--security-opt label=disable \
		--network host \
		-e DISPLAY=$(DISPLAY) \
		-e DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(shell id -u)/bus" \
		-v /run/user/$(shell id -u)/bus:/run/user/$(shell id -u)/bus \
		-v /var/lib/dbus/machine-id:/var/lib/dbus/machine-id:ro \
		-v /tmp/.X11-unix:/tmp/.X11-unix:rw \
		suckless-ogl /bin/bash -c "export DISPLAY=$(DISPLAY) && ./app"

# Clean dangling images
docker-clean:
	@echo "Cleaning dangling images..."
	$(CONTAINER_ENGINE) image prune -f

# Clean all unused images and build cache
docker-clean-all:
	@echo "Cleaning all unused images and cache..."
	$(CONTAINER_ENGINE) system prune -a -f

# Show disk usage
docker-usage:
	@echo "Docker/Podman disk usage:"
	$(CONTAINER_ENGINE) system df

# Build avec SSBO
.PHONY: build-ssbo run-ssbo clean-ssbo

build-ssbo:
	@echo "Building with SSBO rendering enabled..."
	@mkdir -p build-ssbo
	@$(DISTROBOX) $(CMAKE) -B build-ssbo -DUSE_SSBO_RENDERING=ON -G "Unix Makefiles"
	@$(DISTROBOX) $(CMAKE) --build build-ssbo --parallel $(NPROCS)

run-ssbo: build-ssbo
	@./build-ssbo/app

clean-ssbo:
	@echo "Cleaning SSBO build..."
	@rm -rf build-ssbo

# Build avec Sync Debug
.PHONY: build-sync run-sync clean-sync

build-sync:
	@echo "Building with Synchronous Debug enabled (SLOW)..."
	@mkdir -p build-sync
	@$(DISTROBOX) $(CMAKE) -B build-sync -DDEBUG_SYNCHRONOUS=ON -G "Unix Makefiles"
	@$(DISTROBOX) $(CMAKE) --build build-sync --parallel $(NPROCS)

run-sync: build-sync
	@./build-sync/app

clean-sync:
	@echo "Cleaning Sync Debug build..."
	@rm -rf build-sync

help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  clean      - Clean build artifacts using CMake"
	@echo "  clean-all  - Completely remove the build directory"
	@echo "  rebuild    - Full clean and rebuild from scratch"
	@echo "  run        - Build and run the application"
	@echo "  run-release - Build and run the application in release mode"
	@echo "  run-small   - Build and run the application in small mode"
	@echo "  build-ssbo - Build with SSBO rendering (alternative path)"
	@echo "  run-ssbo   - Build and run with SSBO rendering"
	@echo "  clean-ssbo - Clean SSBO-specific build"
	@echo "  build-sync - Build with Synchronous Debug (SLOW)"
	@echo "  run-sync   - Build and run with Synchronous Debug"
	@echo "  clean-sync - Clean Sync Debug build"
	@echo "  format     - Format code using clang-format"
	@echo "  lint       - Lint code using clang-tidy"
	@echo "  deps-setup - Download dependencies for offline build"
	@echo "  deps-clean - Remove the local dependency cache"
	@echo "  offline-test - Verify build works without internet (requires unshare)"
	@echo "  test       - Run unit tests with ctest"
	@echo "  test-one   - Run a single test (make test-one TEST=name or make test/name)"
	@echo "  test-list  - List all available test names"
	@echo "  test-integration - Run full UI integration test under Valgrind (Default)"
	@echo "  test-integration-asan - Run full UI integration test under ASan"
	@echo "  coverage   - Generate HTML code coverage report (llvm-cov)"
	@echo "  docker-build - Build the Docker image"
	@echo "  profile    - Build with optimizations and debug symbols (for profiling)"
	@echo "  perf       - Build and run Linux 'perf' profiler"
	@echo "  release    - Build for Maximum Speed (-O3, Native, FastMath, Stripped)"
	@echo "  memcheck   - Run Valgrind (Default) to detect leaks/errors"
	@echo "  memcheck-asan - Run AddressSanitizer (ASan) to detect leaks/errors"
	@echo "  small      - Build for Minimum Size (-Os, Stripped)"
	@echo "  docs       - Generate and verify Doxygen documentation (with diagrams)"
	@echo "  test-gen-refs - Regenerate test reference images"
	@echo "  help       - Show this help message"

# --- Release Build (Max Speed) ---
release:
	@echo "Building for Release (Speed at all costs)..."
	@mkdir -p $(BUILD_REL_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_REL_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DENABLE_NATIVE_ARCH=ON \
		-DENABLE_AGGRESSIVE_MATH=ON \
		-DENABLE_UNITY_BUILD=ON \
		-DENABLE_SHADER_OPTIMIZATION=ON \
		-G "Unix Makefiles"
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_REL_DIR) --parallel $(NPROCS)
	@echo "Stripping binary..."
	@$(DISTROBOX) strip --strip-all $(BUILD_REL_DIR)/app
	@echo "Done. Binary is at $(BUILD_REL_DIR)/app"
	@du -h $(BUILD_REL_DIR)/app

# --- Memory Check (Valgrind - Default) ---
valgrind: release
	@echo "Running Valgrind on Release build..."
	@$(DISTROBOX) valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --suppressions=valgrind.supp ./$(BUILD_REL_DIR)/app

memcheck: valgrind

# --- Memory Check (ASan - Optional) ---
memcheck-asan: asan
	@echo "Running Memory Check with AddressSanitizer (ASan)..."
	@$(DISTROBOX) env ASAN_OPTIONS="exitcode=1:detect_leaks=1:symbolize=1:halt_on_error=1" LSAN_OPTIONS="suppressions=lsan.supp" ./$(BUILD_ASAN_DIR)/app

# --- Integration Test (Scenario-based with xdotool) ---
test-integration: release
	@chmod +x scripts/test_integration_valgrind.sh
	@bash scripts/test_integration_valgrind.sh

test-integration-asan: asan
	@chmod +x scripts/test_integration_asan.sh
	@bash scripts/test_integration_asan.sh

# --- Performance Verification (ApiTrace) ---
test-apitrace: all
	@chmod +x scripts/verify_apitrace_perf.sh
	@bash scripts/verify_apitrace_perf.sh "$(APITRACE_BIN)"

test-integration-apitrace: all
	@chmod +x scripts/test_integration_apitrace.sh
	@bash scripts/test_integration_apitrace.sh "$(APITRACE_BIN)"

# --- Debug Release Build (For Segfault Hunting) ---
debug-release:
	@echo "Building for Debug Release (Release + Symbols + No Strip)..."
	@mkdir -p $(BUILD_REL_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_REL_DIR) \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DENABLE_NATIVE_ARCH=ON \
		-DENABLE_AGGRESSIVE_MATH=ON \
		-DENABLE_UNITY_BUILD=ON \
		-G "Unix Makefiles"
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_REL_DIR) --parallel $(NPROCS)
	@echo "Done. Binary is at $(BUILD_REL_DIR)/app (Not stripped)"
	@du -h $(BUILD_REL_DIR)/app

# --- Small Build (Min Size) ---
small:
	@echo "Building for Size (MinSizeRel)..."
	@mkdir -p $(BUILD_SMALL_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_SMALL_DIR) \
		-DCMAKE_BUILD_TYPE=MinSizeRel \
		-DENABLE_NATIVE_ARCH=OFF \
		-DENABLE_AGGRESSIVE_MATH=ON \
		-G "Unix Makefiles"
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_SMALL_DIR) --parallel $(NPROCS)
	@echo "Stripping binary..."
	@$(DISTROBOX) strip --strip-all $(BUILD_SMALL_DIR)/app
	@echo "Done. Binary is at $(BUILD_SMALL_DIR)/app"
	@du -h $(BUILD_SMALL_DIR)/app
# --- Windows / Cross-Compilation (MinGW + Wine) ---
BUILD_WIN_DIR := build-win

build-win:
	@echo "Building for Windows (MinGW)..."
	@mkdir -p $(BUILD_WIN_DIR)
	@$(CMAKE) -B $(BUILD_WIN_DIR) -DCMAKE_TOOLCHAIN_FILE=toolchain-mingw.cmake -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
	@$(CMAKE) --build $(BUILD_WIN_DIR) --parallel $(NPROCS)

test-win: build-win
	@echo "Running Windows unit tests under Wine..."
	@WINEPREFIX=$(HOME)/.wine WINEDEBUG=-all TEST_RUNNER_PREFIX="wine64" ctest --test-dir $(BUILD_WIN_DIR) --output-on-failure
