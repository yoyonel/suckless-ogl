# Makefile wrapper for CMake-based build system

BUILD_DIR := build
CMAKE := cmake

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

#
APITRACE_DIR := $(HOME)/Téléchargements/apitrace-latest-Linux
APITRACE_WRAPPERS := $(APITRACE_DIR)/lib/apitrace/wrappers
APITRACE_BIN := $(APITRACE_DIR)/bin/apitrace

#
BUILD_PROF_DIR := build-prof

.PHONY: all clean clean-all rebuild run help format lint deps-setup deps-clean offline-test docker-build test coverage

all: $(BUILD_DIR)/Makefile
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --parallel $(shell nproc)

$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_DIR) -G "Unix Makefiles"

clean:
	@if [ -d $(BUILD_DIR) ]; then $(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --target clean; fi
	@$(DISTROBOX) rm -rf $(BUILD_DIR)

clean-all:
	@echo "Removing all build directories..."
	@rm -rf $(BUILD_DIR) $(BUILD_COV_DIR) $(BUILD_PROF_DIR) build-ssbo

rebuild: clean-all all

run: all
	@./$(BUILD_DIR)/app

format:
	$(DISTROBOX) sh -c "find src include tests -name \"*.c\" -o -name \"*.h\" | xargs clang-format -i"

# Resolve dependency paths for linting
# We check if 'deps' exists (offline mode), otherwise fall back to build/_deps
STB_INC := $(shell [ -d deps/stb ] && echo deps/stb || echo build/_deps/stb-src)
CGLM_INC := $(shell [ -d deps/cglm ] && echo deps/cglm/include || echo build/_deps/cglm-src/include)
GLAD_INC := build/_deps/glad-build/include
CJSON_INC := $(shell [ -d deps/cjson ] && echo deps/cjson || echo build/_deps/cjson-src)

lint: $(BUILD_DIR)/Makefile
	@echo "Ensuring dependencies are generated..."
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --target glad
	$(DISTROBOX) clang-tidy -header-filter="^$(CURDIR)/(src|include)/.*" $(shell find src -name "*.c" ! -name "stb_image_impl.c") -- -D_POSIX_C_SOURCE=200809L -Isrc -Iinclude -isystem $(CURDIR)/$(STB_INC) -isystem $(CURDIR)/$(GLAD_INC) -isystem $(CURDIR)/$(CGLM_INC) -isystem $(CURDIR)/$(CJSON_INC)

deps-setup:
	@chmod +x scripts/setup_offline_deps.sh
	@./scripts/setup_offline_deps.sh

deps-clean:
	@echo "Removing offline dependency cache..."
	@rm -rf deps/

offline-test:
	@echo "Running build in a simulated offline environment (using bogus proxy)..."
	@http_proxy=http://127.0.0.1:0 https_proxy=http://127.0.0.1:0 $(MAKE) rebuild

test:
	@$(DISTROBOX) ctest --test-dir $(BUILD_DIR) --output-on-failure

# Code Coverage (version améliorée avec résumé)
BUILD_COV_DIR := build-coverage
REPORT_DIR := $(BUILD_COV_DIR)/coverage_report

coverage:
	@echo "Building with coverage instrumentation..."
	@mkdir -p $(BUILD_COV_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_COV_DIR) -DCODE_COVERAGE=ON -DCMAKE_C_COMPILER=clang
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_COV_DIR) --parallel $(shell nproc)
	
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
		-ignore-filename-regex="(generated|deps|tests)"
	@echo "Report generated at: $(REPORT_DIR)/index.html"
	
	@echo "Coverage Summary:"
	@$(DISTROBOX) llvm-cov report \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$(BUILD_COV_DIR)/app \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-ignore-filename-regex="(generated|deps|tests)" | tee $(BUILD_COV_DIR)/coverage_summary.txt

apitrace:
	@echo "Running Apitrace..."
	LD_PRELOAD=$(APITRACE_WRAPPERS)/egltrace.so \
		$(APITRACE_BIN) \
			trace \
			--api egl \
			--output $(BUILD_PROF_DIR)/app.trace \
			./$(BUILD_PROF_DIR)/app

qapitrace:
	@echo "Running Qapitrace..."
	$(DISTROBOX) qapitrace ./$(BUILD_PROF_DIR)/app.trace

# --- Profiling Build (Optimized + Debug Symbols) ---
.PHONY: profile perf

profile:
	@echo "Building for profiling (RelWithDebInfo + LTO)..."
	@mkdir -p $(BUILD_PROF_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_PROF_DIR) -DCMAKE_BUILD_TYPE=Profiling
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_PROF_DIR) --parallel $(shell nproc)

perf: profile
	@echo "Running perf record..."
	@# On utilise --call-graph dwarf pour avoir des stack traces propres avec les symboles
	@$(DISTROBOX) perf record --call-graph dwarf ./$(BUILD_PROF_DIR)/app
	@$(DISTROBOX) perf report

valgrind:
	@echo "Running Valgrind (very slow to start)..."
	@$(DISTROBOX) valgrind --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite ./$(BUILD_PROF_DIR)/app
	
# Docker Integration
# Auto-detect container engine (podman or docker)
CONTAINER_ENGINE := $(shell command -v docker 2> /dev/null || echo podman)
IMAGE_NAME := suckless-ogl

docker-build:
	$(CONTAINER_ENGINE) build \
		-t $(IMAGE_NAME) .

docker-build-no-cache:
	$(CONTAINER_ENGINE) build \
		--no-cache \
		-t $(IMAGE_NAME) .

docker-run:
	@echo "Running Container with X11 forwarding..."
	xhost +local: > /dev/null 2>&1 || true
	$(CONTAINER_ENGINE) run --rm -it \
		--security-opt label=disable \
		--network host \
		-e DISPLAY=$(DISPLAY) \
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
	@$(DISTROBOX) $(CMAKE) --build build-ssbo --parallel $(shell nproc)

run-ssbo: build-ssbo
	@./build-ssbo/app

clean-ssbo:
	@echo "Cleaning SSBO build..."
	@rm -rf build-ssbo

help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  clean      - Clean build artifacts using CMake"
	@echo "  clean-all  - Completely remove the build directory"
	@echo "  rebuild    - Full clean and rebuild from scratch"
	@echo "  run        - Build and run the application"
	@echo "  build-ssbo - Build with SSBO rendering (alternative path)"
	@echo "  run-ssbo   - Build and run with SSBO rendering"
	@echo "  clean-ssbo - Clean SSBO-specific build"
	@echo "  format     - Format code using clang-format"
	@echo "  lint       - Lint code using clang-tidy"
	@echo "  deps-setup - Download dependencies for offline build"
	@echo "  deps-clean - Remove the local dependency cache"
	@echo "  offline-test - Verify build works without internet (requires unshare)"
	@echo "  test       - Run unit tests with ctest"
	@echo "  coverage   - Generate HTML code coverage report (llvm-cov)"
	@echo "  docker-build - Build the Docker image"
	@echo "  profile    - Build with optimizations and debug symbols (for profiling)"
	@echo "  perf       - Build and run Linux 'perf' profiler"
	@echo "  help       - Show this help message"
