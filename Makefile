# User-friendly wrapper Makefile for suckless-ogl
# This provides convenient debug/release targets while preserving CMake functionality

# Custom user targets
.PHONY: debug release run help-user

# Show user-friendly help
help-user:
	@echo "=== User-Friendly Build Targets ==="
	@echo "  make debug      - Build with dynamic shaders (development)"
	@echo "  make release    - Build with optimized static shaders (performance)"
	@echo "  make run        - Run the application (debug build)"
	@echo "  make run-release - Run the application (release build)"
	@echo "  make rebuild    - Clean and rebuild from scratch"
	@echo ""
	@echo "=== Test Targets ==="
	@echo "  make test       - Run all C tests (via CMake/ctest)"
	@echo "  make test-python - Run Python tests"
	@echo "  make test-integration - Run integration tests with Xvfb"
	@echo "  make memcheck   - Run memory check with Valgrind"
	@echo ""
	@echo "=== Profiling Targets ==="
	@echo "  make profile    - Build with profiling symbols (Profiling + LTO)"
	@echo "  make perf       - Build and run Linux 'perf' profiler"
	@echo "  make valgrind   - Build and run Valgrind memory checker"
	@echo ""
	@echo "=== Code Quality ==="
	@echo "  make format     - Format code with clang-format"
	@echo "  make lint       - Lint code with clang-tidy"
	@echo "  make coverage   - Generate HTML code coverage report"
	@echo ""
	@echo "=== Dependencies ==="
	@echo "  make deps-setup - Download dependencies for offline build"
	@echo "  make deps-clean - Remove local dependency cache"
	@echo "  make offline-test - Verify build works without internet"
	@echo ""
	@echo "=== CMake Targets (from build/) ==="
	@echo "  make app        - Build the main application"
	@echo "  make clean      - Clean build artifacts"
	@echo "  make clean-all  - Remove all build directories"
	@echo ""
	@echo "Run 'make help' for full CMake target list"

debug:
	@echo "==> Configuring DEBUG build (dynamic shaders)..."
	@mkdir -p build
	@cd build && cmake -DENABLE_SHADER_OPTIMIZATION=OFF .. && $(MAKE) app

release:
	@echo "==> Configuring RELEASE build (optimized shaders)..."
	@mkdir -p build
	@cd build && cmake -DENABLE_SHADER_OPTIMIZATION=ON .. && $(MAKE) app

run:
	@./build/app

run-release: release
	@./build/app

# === Test Targets ===
.PHONY: test-python test-integration memcheck

test-python:
	@echo "==> Running Python tests..."
	@pytest tests/ -v

test-integration:
	@echo "==> Running integration tests with Xvfb..."
	@chmod +x scripts/test_integration_valgrind.sh
	@./scripts/test_integration_valgrind.sh

memcheck: release
	@echo "==> Running memory check with Valgrind..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./build/app

# === Profiling Targets ===
BUILD_PROF_DIR := build-prof

.PHONY: profile perf valgrind

profile:
	@echo "==> Building for profiling (Profiling + LTO)..."
	@mkdir -p $(BUILD_PROF_DIR)
	@cd $(BUILD_PROF_DIR) && cmake -DCMAKE_BUILD_TYPE=Profiling .. && $(MAKE) -j$$(nproc) app

perf: profile
	@echo "==> Running perf record..."
	@perf record --call-graph dwarf ./$(BUILD_PROF_DIR)/app
	@perf report

valgrind: profile
	@echo "==> Running Valgrind (very slow to start)..."
	@valgrind --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=definite ./$(BUILD_PROF_DIR)/app

# === Code Quality Targets ===
.PHONY: format lint

format:
	@echo "==> Formatting code with clang-format..."
	@find src include tests -name "*.c" -o -name "*.h" | xargs clang-format -i

# Resolve dependency paths for linting
STB_INC := $(shell [ -d deps/stb ] && echo deps/stb || echo build/_deps/stb-src)
CGLM_INC := $(shell [ -d deps/cglm ] && echo deps/cglm/include || echo build/_deps/cglm-src/include)
GLAD_INC := build/_deps/glad-build/include
CJSON_INC := $(shell [ -d deps/cjson ] && echo deps/cjson || echo build/_deps/cjson-src)

lint: build/Makefile
	@echo "==> Ensuring dependencies are generated..."
	@cd build && $(MAKE) glad
	@echo "==> Running clang-tidy..."
	@clang-tidy -header-filter="^$(CURDIR)/(src|include)/.*" $$(find src -name "*.c" ! -name "stb_image_impl.c") -- -D_POSIX_C_SOURCE=199309L -Isrc -Iinclude -isystem $(CURDIR)/$(STB_INC) -isystem $(CURDIR)/$(GLAD_INC) -isystem $(CURDIR)/$(CGLM_INC) -isystem $(CURDIR)/$(CJSON_INC)

# === Dependency Management ===
.PHONY: deps-setup deps-clean offline-test

deps-setup:
	@echo "==> Setting up offline dependencies..."
	@chmod +x scripts/setup_offline_deps.sh
	@./scripts/setup_offline_deps.sh

deps-clean:
	@echo "==> Removing offline dependency cache..."
	@rm -rf deps/

offline-test:
	@echo "==> Running build in simulated offline environment..."
	@http_proxy=http://127.0.0.1:0 https_proxy=http://127.0.0.1:0 $(MAKE) rebuild

# === Coverage ===
BUILD_COV_DIR := build-coverage
REPORT_DIR := $(BUILD_COV_DIR)/coverage_report

.PHONY: coverage

coverage:
	@echo "==> Building with coverage instrumentation..."
	@mkdir -p $(BUILD_COV_DIR)
	@cd $(BUILD_COV_DIR) && cmake -DCODE_COVERAGE=ON -DCMAKE_C_COMPILER=clang ..
	@cd $(BUILD_COV_DIR) && $(MAKE) -j$$(nproc)

	@echo "==> Running tests to generate profile data..."
	@LLVM_PROFILE_FILE='$(CURDIR)/$(BUILD_COV_DIR)/test_%p.profraw' ctest --test-dir $(BUILD_COV_DIR) --output-on-failure

	@echo "==> Merging profile data..."
	@llvm-profdata merge -sparse $(BUILD_COV_DIR)/*.profraw -o $(BUILD_COV_DIR)/coverage.profdata

	@echo "==> Generating HTML report..."
	@mkdir -p $(REPORT_DIR)
	@llvm-cov show -format=html \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-output-dir=$(REPORT_DIR) \
		-ignore-filename-regex="(generated|deps|tests)"
	@echo "==> Report generated at: $(REPORT_DIR)/index.html"

	@echo "==> Coverage Summary:"
	@llvm-cov report \
		-instr-profile=$(BUILD_COV_DIR)/coverage.profdata \
		$$(find $(BUILD_COV_DIR)/tests -maxdepth 1 -name "test_*" -type f -executable -printf "-object %p ") \
		-ignore-filename-regex="(generated|deps|tests)"

# === Clean Targets ===
.PHONY: clean-all rebuild

clean-all:
	@echo "==> Removing all build directories..."
	@rm -rf build $(BUILD_COV_DIR) $(BUILD_PROF_DIR)

rebuild: clean-all debug

# Delegate all other targets to the build directory Makefile
%:
	@if [ -d build ]; then \
		$(MAKE) -C build $@; \
	else \
		echo "Error: build/ directory not found. Run 'make debug' or 'make release' first."; \
		exit 1; \
	fi
