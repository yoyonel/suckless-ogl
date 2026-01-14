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

# Build with cache mount for CMake build dir
RUN --mount=type=cache,target=/src/build \
    cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    && cmake --build build --parallel \
    && cp build/app /tmp/app

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
    chown -R appuser:appuser /app

USER appuser

CMD ["/app/entrypoint.sh"]