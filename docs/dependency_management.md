# Dependency Management Strategy

This project follows a **"Source + Patch"** pattern for managing external tools and dependencies that require local modifications, rather than using traditional Git submodules for everything.

## Philosophy

The core philosophy is based on **Transparency and Reproducibility**. Instead of relying on the implicit state of a Git submodule (which can be hard to track, patch, or verify), we use an explicit "recette" (recipe) in the `Justfile`.

### Why not Git Submodules?

While submodules are useful for including external code, they introduce several challenges when local patches are required:

- **Implicit State**: Submodules can easily get out of sync with the main project.
- **Maintenance Burden**: If a dependency needs a patch (e.g., to fix a compiler error in a specific version), maintaining a personal fork of a large project like Tracy is a heavy burden.
- **Workflow Friction**: They require specific commands (`git submodule update --init`) that are often forgotten by collaborators.

## The "Source + Patch" Approach

This approach, inspired by modern package management systems like **Nix** or **Gentoo**, consists of:

1. **Upstream Source**: We target a specific tag or commit of the official upstream repository.
2. **Explicit Recipe**: The `Justfile` automates the cloning and checkout of this source.
3. **Local Patches**: We maintain a `patches/` directory containing formal `.patch` files versioned within this repository.
4. **Atomic Application**: The `Justfile` automates the application of these patches during the tool's build process.

### Practical Example: Tracy Profiler

The Tracy Profiler server setup in this project is managed this way:

- **Recipe**: `just tracy-server`
- **Action**:
    1. Clones the official `wolfpld/tracy` repository into `tools/tracy`.
    2. Checkouts the stable version `v0.10`.
    3. Applies the local `patches/tracy-v0.10-compiler-fix.patch` to ensure compatibility with modern GCC/Clang compilers.
    4. Builds the server.

This ensures that any developer cloning this project has an immediate, working, and reproducible setup by simply running a one-liner command.

## Summary of Benefits

- **Reproducibility**: The exact build process is documented and executable.
- **Transparency**: Every modification to a third-party tool is visible in the `patches/` folder.
- **Lightweight**: We don't carry the weight of a fork or complex submodule state.
- **Portability**: The fix is "embedded" in the project and works on any development workstation.
