# Makefile wrapper for CMake-based build system

BUILD_DIR := build
CMAKE := cmake
BOX := clang-dev
DISTROBOX := distrobox enter $(BOX) --

.PHONY: all clean clean-all rebuild run help

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

help:
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  clean      - Clean build artifacts using CMake"
	@echo "  clean-all  - Completely remove the build directory"
	@echo "  rebuild    - Full clean and rebuild from scratch"
	@echo "  run        - Build and run the application"
	@echo "  help       - Show this help message"
