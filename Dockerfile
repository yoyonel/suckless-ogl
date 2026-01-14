# Stage 1: Builder
# We use the full Fedora image to have all development tools
FROM fedora:41 AS builder

# Install build dependencies
# clang, cmake, ninja-build: Build system
# git: For version info (optional) or if deps need it
# glfw-devel: System dependency for the app
RUN dnf -y update && dnf -y install \
    clang \
    cmake \
    git \
    make \
    ninja-build \
    python3 \
    glfw-devel \
    && dnf clean all

WORKDIR /src

# Copy project files
# We use COPY . . which includes deps/ if it exists (thanks to .dockerignore)
# allowing for offline builds if the host cache is populated.
COPY . .

# Configure and Build
# We use Ninja for faster builds
# FETCHCONTENT_FULLY_DISCONNECTED=OFF ensures we can download if deps/ is missing
# (Though our CMake logic sets it to ON if deps/ exists)
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    && cmake --build build --parallel

# Stage 2: Runtime
# A lighter image just containing the binary and assets
FROM fedora:41

# Install minimal runtime dependencies (just enough to link)
RUN dnf -y update && dnf -y install \
    glfw \
    mesa-libGL \
    mesa-dri-drivers \
    libglvnd-glx \
    && dnf clean all

# Create a non-root user for security
RUN useradd -u 1000 -m appuser

WORKDIR /app

# Copy artifacts from the builder stage
COPY --from=builder /src/build/app .
COPY --from=builder /src/assets ./assets

# Ensure permissions
RUN chown -R appuser:appuser /app

# Switch to non-root user
USER appuser

# Run the application
CMD ["./app"]
