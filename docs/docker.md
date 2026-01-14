# Docker Support

This project includes a containerization setup to build the application in an isolated environment, ensuring consistency across different Linux systems.

## Prerequisites

- **Docker** or **Podman** installed.

## Building the Image

To compile the application inside a clean Fedora environment:

```bash
make docker-build
```
This command:
1. Copies your local source code.
2. Uses your local `deps/` cache (if present) to avoid re-downloading dependencies.
3. Compiles the project using `clang` and `ninja`.
4. Packages the result into a lightweight image (verifying the installation process).

## image Details

The build uses a **Multi-stage** strategy:

1.  **Builder (`fedora:41`)**: Full development environment (~500MB+).
2.  **Runtime (`fedora:41`)**: Minimal image containing only the application and assets.

## image Details

The build uses a **Multi-stage** strategy:

1.  **Builder (`fedora:41`)**: Full development environment (~500MB+).
2.  **Runtime (`fedora:41`)**: Minimized image containing only the application, assets, and runtime libraries.

The final image runs as a non-root user (`appuser`) for better security.
