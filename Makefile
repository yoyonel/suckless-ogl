# Makefile wrapper for CMake-based build system

BUILD_DIR := build
CMAKE := cmake
BOX := clang-dev
DISTROBOX := distrobox enter $(BOX) --

.PHONY: all clean clean-all rebuild run help format lint deps-setup deps-clean offline-test docker-build

all: $(BUILD_DIR)/Makefile
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --parallel $(shell nproc)

$(BUILD_DIR)/Makefile:
	@mkdir -p $(BUILD_DIR)
	@$(DISTROBOX) $(CMAKE) -B $(BUILD_DIR) -G "Unix Makefiles"

clean:
	@if [ -d $(BUILD_DIR) ]; then $(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --target clean; fi

clean-all:
	@echo "Removing entire build directory..."
	@rm -rf $(BUILD_DIR)

rebuild: clean-all all

run: all
	@./$(BUILD_DIR)/app

format:
	$(DISTROBOX) sh -c "find src include -name \"*.c\" -o -name \"*.h\" | xargs clang-format -i"

# Resolve dependency paths for linting
# We check if 'deps' exists (offline mode), otherwise fall back to build/_deps
STB_INC := $(shell [ -d deps/stb ] && echo deps/stb || echo build/_deps/stb-src)
CGLM_INC := $(shell [ -d deps/cglm ] && echo deps/cglm/include || echo build/_deps/cglm-src/include)
GLAD_INC := build/_deps/glad-build/include

lint: $(BUILD_DIR)/Makefile
	@echo "Ensuring dependencies are generated..."
	@$(DISTROBOX) $(CMAKE) --build $(BUILD_DIR) --target glad
	$(DISTROBOX) clang-tidy -header-filter="^$(CURDIR)/(src|include)/.*" $(shell find src -name "*.c" ! -name "stb_image_impl.c") -- -D_POSIX_C_SOURCE=199309L -Isrc -Iinclude -isystem $(CURDIR)/$(STB_INC) -isystem $(CURDIR)/$(GLAD_INC) -isystem $(CURDIR)/$(CGLM_INC)

deps-setup:
	@chmod +x scripts/setup_offline_deps.sh
	@./scripts/setup_offline_deps.sh

deps-clean:
	@echo "Removing offline dependency cache..."
	@rm -rf deps/

offline-test:
	@echo "Running build in a simulated offline environment (using bogus proxy)..."
	@http_proxy=http://127.0.0.1:0 https_proxy=http://127.0.0.1:0 $(MAKE) rebuild

# Docker Integration
# Auto-detect container engine (podman or docker)
CONTAINER_ENGINE := $(shell command -v docker 2> /dev/null || echo podman)
IMAGE_NAME := suckless-ogl

docker-build:
	$(CONTAINER_ENGINE) build -t $(IMAGE_NAME) .

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
	@echo "  docker-build - Build the Docker image"
	@echo "  help       - Show this help message"
