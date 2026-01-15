# Makefile wrapper for CMake-based build system

BUILD_DIR := build
CMAKE := cmake

# On définit BOX pour garder la configuration de ton env local
BOX := clang-dev

# Si CONTAINER_RUN n'est pas défini, on utilise distrobox par défaut (ton usage local) [cite: 1]
# Si on veut désactiver distrobox, on pourra passer CONTAINER_RUN=""
CONTAINER_RUN ?= distrobox enter $(BOX) --

# On remplace l'ancienne variable par la nouvelle
DISTROBOX := $(CONTAINER_RUN)

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
	@echo "Removing entire build directory..."
	@rm -rf $(BUILD_DIR)

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
	$(DISTROBOX) clang-tidy -header-filter="^$(CURDIR)/(src|include)/.*" $(shell find src -name "*.c" ! -name "stb_image_impl.c") -- -D_POSIX_C_SOURCE=199309L -Isrc -Iinclude -isystem $(CURDIR)/$(STB_INC) -isystem $(CURDIR)/$(GLAD_INC) -isystem $(CURDIR)/$(CGLM_INC) -isystem $(CURDIR)/$(CJSON_INC)

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

# Code Coverage
BUILD_COV_DIR := build-coverage
REPORT_DIR := $(BUILD_COV_DIR)/coverage_report

coverage:
	@echo "Building with coverage instrumentation..."
	@mkdir -p $(BUILD_COV_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_COV_DIR) -DCODE_COVERAGE=ON -DCMAKE_C_COMPILER=clang
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_COV_DIR) --parallel $(shell nproc)
	
	@echo "Running tests to generate profile data..."
	@$(DISTROBOX) sh -c "LLVM_PROFILE_FILE='$(CURDIR)/$(BUILD_COV_DIR)/test_%p.profraw' ctest --test-dir $(BUILD_COV_DIR) --output-on-failure"
	
	@echo "Merging profile data..."
	@$(DISTROBOX) llvm-profdata merge -sparse $(BUILD_COV_DIR)/*.profraw -o $(BUILD_COV_DIR)/coverage.profdata
	
	@echo "Generating HTML report..."
	@mkdir -p $(REPORT_DIR)
	@$(DISTROBOX) llvm-cov show -format=html \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-output-dir=$(REPORT_DIR) \
		-ignore-filename-regex="(generated|deps|tests)"
	@echo "Report generated at: $(REPORT_DIR)/index.html"
	
	@echo "Coverage Summary:"
	@$(DISTROBOX) llvm-cov report \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-ignore-filename-regex="(generated|deps|tests)"

# Docker Integration
# Auto-detect container engine (podman or docker)
CONTAINER_ENGINE := $(shell command -v docker 2> /dev/null || echo podman)
IMAGE_NAME := suckless-ogl

docker-build:
	$(CONTAINER_ENGINE) build \
		--layers=true \
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

help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  clean      - Clean build artifacts using CMake"
	@echo "  clean-all  - Completely remove the build directory"
	@echo "  rebuild    - Full clean and rebuild from scratch"
	@echo "  run        - Build and run the application"
	@echo "  format     - Format code using clang-format"
	@echo "  lint       - Lint code using clang-tidy"
	@echo "  deps-setup - Download dependencies for offline build"
	@echo "  deps-clean - Remove the local dependency cache"
	@echo "  offline-test - Verify build works without internet (requires unshare)"
	@echo "  test       - Run unit tests with ctest"
	@echo "  coverage   - Generate HTML code coverage report (llvm-cov)"
	@echo "  docker-build - Build the Docker image"
	@echo "  help       - Show this help message"
