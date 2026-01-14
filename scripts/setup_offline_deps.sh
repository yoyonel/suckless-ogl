#!/bin/bash

# This script clones the project dependencies into a 'deps' directory
# to allow for offline builds via the FETCHCONTENT_SOURCE_DIR override.

set -e

DEPS_DIR="deps"
mkdir -p "$DEPS_DIR"

# cglm
if [ ! -d "$DEPS_DIR/cglm" ]; then
    echo "Cloning cglm..."
    git clone --depth 1 --branch v0.9.4 https://github.com/recp/cglm.git "$DEPS_DIR/cglm"
else
    echo "cglm already exists in $DEPS_DIR"
fi

# glad
if [ ! -d "$DEPS_DIR/glad" ]; then
    echo "Cloning glad..."
    git clone --depth 1 --branch v0.1.36 https://github.com/Dav1dde/glad.git "$DEPS_DIR/glad"
else
    echo "glad already exists in $DEPS_DIR"
fi

# stb
if [ ! -d "$DEPS_DIR/stb" ]; then
    echo "Cloning stb..."
    git clone --depth 1 https://github.com/nothings/stb.git "$DEPS_DIR/stb"
else
    echo "stb already exists in $DEPS_DIR"
fi

# Verification: GLAD needs its internal specs (gl.xml) which are included in the repo.
if [ ! -f "$DEPS_DIR/glad/glad/files/gl.xml" ]; then
    echo "Warning: GLAD specification file not found in $DEPS_DIR/glad/glad/files/gl.xml"
    echo "This might cause issues during offline builds."
fi

if [ ! -f "$DEPS_DIR/unity" ]; then
    echo "Cloning unity..."
    git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git "$DEPS_DIR/unity"
else
    echo "unity already exists in $DEPS_DIR"
fi

echo "Done! You can now build the project offline."
echo "CMake will automatically detect the '$DEPS_DIR' directory."
