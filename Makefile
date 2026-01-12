# =========================
# Makefile pour suckless-ogl (refactorisé)
# =========================

# Compilateur
CC := clang

# Dossiers
SRC_DIR := src
INCLUDE_DIR := include
DEPS_DIR := deps
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

# Fichiers sources
SOURCES := $(SRC_DIR)/main.c \
           $(SRC_DIR)/app.c \
           $(SRC_DIR)/icosphere.c \
           $(SRC_DIR)/shader.c \
           $(SRC_DIR)/texture.c \
           $(SRC_DIR)/skybox.c

GLAD_SRC := $(DEPS_DIR)/glad/glad.c

# Fichiers objets
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
GLAD_OBJ := $(OBJ_DIR)/glad.o

ALL_OBJS := $(OBJECTS) $(GLAD_OBJ)

# Includes
INCLUDES := -I$(SRC_DIR) -I$(INCLUDE_DIR) -I$(DEPS_DIR)/glad
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
GLFW_LIBS := $(shell pkg-config --libs glfw3)
CGLM_CFLAGS := -I$(HOME)/.local/include
CGLM_LIBS := $(HOME)/.local/lib64/libcglm.a

# Options compilation
CFLAGS := -std=c99 -Wall -Wextra -O2 $(INCLUDES) $(GLFW_CFLAGS) $(CGLM_CFLAGS)
LIBS := $(GLFW_LIBS) $(CGLM_LIBS) -lm -ldl

# Binaire
TARGET := $(BUILD_DIR)/app

# =========================
# Lint & format
# =========================
CLANG_FORMAT := clang-format
CLANG_TIDY := clang-tidy
TIDY_ARGS := --extra-arg=-std=c99 $(addprefix --extra-arg=,$(INCLUDES)) --extra-arg=$(GLFW_CFLAGS) --quiet

# =========================
# Règles principales
# =========================

.PHONY: all clean lint format run

all: $(TARGET)

$(TARGET): $(ALL_OBJS) | $(BUILD_DIR)
	@echo "Linking $(TARGET)..."
	@$(CC) $(ALL_OBJS) -o $(TARGET) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Compilation des fichiers objets
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Compilation de GLAD
$(OBJ_DIR)/glad.o: $(GLAD_SRC) | $(OBJ_DIR)
	@echo "Compiling GLAD..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Création des dossiers
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# =========================
# Dépendances
# =========================
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(INCLUDE_DIR)/app.h
$(OBJ_DIR)/app.o: $(SRC_DIR)/app.c $(INCLUDE_DIR)/app.h $(INCLUDE_DIR)/gl_common.h \
                  $(INCLUDE_DIR)/icosphere.h $(INCLUDE_DIR)/shader.h \
                  $(INCLUDE_DIR)/texture.h $(INCLUDE_DIR)/skybox.h
$(OBJ_DIR)/icosphere.o: $(SRC_DIR)/icosphere.c $(INCLUDE_DIR)/icosphere.h
$(OBJ_DIR)/shader.o: $(SRC_DIR)/shader.c $(INCLUDE_DIR)/shader.h $(INCLUDE_DIR)/gl_common.h
$(OBJ_DIR)/texture.o: $(SRC_DIR)/texture.c $(INCLUDE_DIR)/texture.h $(INCLUDE_DIR)/gl_common.h
$(OBJ_DIR)/skybox.o: $(SRC_DIR)/skybox.c $(INCLUDE_DIR)/skybox.h $(INCLUDE_DIR)/gl_common.h

# =========================
# Lint & Format
# =========================

# Lint tous les fichiers sources
lint:
	@echo "Running clang-tidy..."
	@$(CLANG_TIDY) $(SOURCES) $(TIDY_ARGS)

# Format tous les fichiers sources et headers
format:
	@echo "Formatting source files..."
	@$(CLANG_FORMAT) -i $(SOURCES)
	@echo "Formatting header files..."
	@$(CLANG_FORMAT) -i $(INCLUDE_DIR)/*.h
	@echo "Formatting complete."

# =========================
# Utilitaires
# =========================

# Exécuter l'application
run: $(TARGET)
	@echo "Running $(TARGET)..."
	@./$(TARGET)

# Nettoyage
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Rebuild complet
rebuild: clean all

# Afficher les informations de compilation
info:
	@echo "==========================="
	@echo "Build Information"
	@echo "==========================="
	@echo "CC:        $(CC)"
	@echo "CFLAGS:    $(CFLAGS)"
	@echo "LIBS:      $(LIBS)"
	@echo "TARGET:    $(TARGET)"
	@echo "SOURCES:   $(SOURCES)"
	@echo "OBJECTS:   $(ALL_OBJS)"
	@echo "==========================="
