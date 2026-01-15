# Stage 1: Builder
FROM fedora:41 AS builder

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
COPY . .

# Build with static libraries
RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DBUILD_SHARED_LIBS=OFF \
    && cmake --build build --parallel

# Verify static linking
RUN echo "=========================================" && \
    echo "Checking binary dependencies:" && \
    echo "=========================================" && \
    ldd build/app && \
    echo "========================================="

# Test that all custom libs are statically linked (no libglad, libcjson, libcglm)
RUN ! ldd build/app | grep -E '(libglad|libcjson|libcglm)' && \
    echo "✓ All custom libraries are statically linked" || \
    (echo "✗ Some libraries are NOT statically linked" && exit 1)

# Copy binary to temp location
RUN cp build/app /tmp/app

# Stage 2: Runtime
FROM fedora:41

RUN dnf -y update && dnf -y install \
    glfw \
    mesa-libGL \
    mesa-libEGL \
    mesa-dri-drivers \
    libglvnd-glx \
    libglvnd-egl \
    xorg-x11-server-Xvfb \
    && dnf clean all

RUN useradd -u 1000 -m appuser

WORKDIR /app

# Copy from temp location
COPY --from=builder /tmp/app .
COPY --from=builder /src/assets ./assets
COPY --from=builder /src/shaders ./shaders
COPY entrypoint.sh .

RUN chmod +x /app/entrypoint.sh && \
    chmod +x /app/app && \
    chown -R appuser:appuser /app

# Verify again in runtime stage
RUN echo "Runtime verification:" && \
    ldd /app/app && \
    ! ldd /app/app | grep -E '(libglad|libcjson|libcglm)' || \
    (echo "ERROR: Dynamic libraries detected!" && exit 1)

USER appuser

CMD ["/app/entrypoint.sh"]