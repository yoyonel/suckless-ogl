# CI/CD Pipeline & Release Process

This document details the automated Continuous Integration and Continuous Deployment (CI/CD) pipeline for **Suckless-OGL**, managed via **GitHub Actions**.

## Overview

The pipeline is defined in `.github/workflows/main.yml`. It ensures code quality, runs tests, generates documentation, and handles automated releases.

### Runner Environment

To minimize setup time, the pipeline uses a custom Docker-based runner environment instead of installing dependencies manually on every run:

- **Custom Container**: `ghcr.io/${{ github.repository }}-ci:latest`
- **Definition**: Managed in `.github/workflows/Dockerfile.ci`.
- **Automation**: The image is automatically rebuilt and pushed to the GitHub Container Registry (GHCR) by `.github/workflows/ci-image.yml` whenever the Dockerfile is modified.
- **Benefits**: Reduces environment setup time from **~8 minutes** to **< 1 minute** by consolidating Mesa, Xvfb, Doxygen, and Python dependencies.

## Triggers

The workflow is triggered automatically events:

- **Push to `master` or `main`**:
  - Triggers the full pipeline (Lint, Test, Docs, Build).
  - Updates the **Nightly Release**.
  - Deploys the latest documentation to GitHub Pages.
- **Pull Requests**:
  - Runs quality checks (Lint, Format).
  - Runs unit tests and visual regression tests.
  - Generates a documentation preview (deployed to Surge.sh).
- **Tags (`v*`)**:
  - Triggers a **Stable Release** build.
  - Creates a GitHub Release with the corresponding version number.
- **Schedule (Cron)**:
  - Runs daily at **01:00 UTC**.
  - Ensures a fresh **Nightly Build** is generated every day from the latest code.

## Workflow Jobs

### 1. `lint-and-format` (Quality)

- **Goal**: Ensure code style and quality.
- **Tools**: `clang-format`, `clang-tidy`, `ruff` (Python).
- **Checks**:
  - Fails if `make format` results in file changes (enforces code style).
  - Runs `make lint` for static analysis.

### 2. `test-and-coverage` (Validation)

- **Goal**: Verify functionality and prevent regressions.
- **Environment**: Runs under **Xvfb** (Virtual Framebuffer) to simulate a display server for OpenGL tests.
- **Steps**:
  - Runs Python tests (`pytest`).
  - Runs C Unit Tests with Code Coverage (`llvm-cov`).
  - **Visual Regression**: Captures 6 rendered views (cube faces) and compares them against PNG reference images.
  - **Dynamic Reporting**: A Python script generates an interactive HTML index and a Markdown PR comment with hover-comparisons for all 6 faces.

### 3. `documentation` (Docs)

- **Goal**: Generate and publish technical documentation.
- **Tools**: Doxygen, Graphviz, MkDocs.
- **Outputs**:
  - Combines manual Markdown docs and auto-generated C API docs.
  - **Pull Requests**: Deploys a preview link to [Surge.sh](https://surge.sh).
  - **Master/Main**: Prepares artifacts for GitHub Pages deployment.

### 4. `build-production` (Compilation)

- **Goal**: Compile optimized binaries for users.
- **Configurations**:
  - **Release**: Standard optimized build (`-O2` / `-O3`).
  - **Profiling**: Includes symbols and profiling markers.
  - **UltraRelease**: Aggressive optimizations (`-DENABLE_UNITY_BUILD`, `-march=native`, `-ffast-math`).
- **Artifacts**: Binaries are saved for the Release step.

### 5. Script Organization

- **Location**: `.github/workflows/scripts/`
- **Core Scripts**:
  - `generate_visual_report.py`: Handles dynamic report generation.
  - `run_test_with_xvfb.sh`: Wrapper for OpenGL tests in headless environments.
  - `test_trace_analyze.py`: Python unit tests for the trace analyzer.

### 6. `deploy-pages` (Web)

- **Goal**: Update the official documentation site.
- **Trigger**: Only on `master` or `main` push.
- **Action**: Deploys the content of the `documentation` job to GitHub Pages.

### 7. `release` (Distribution)

- **Goal**: Publish binaries and source code.
- **Logic**:
  - **Nightly**:
    - Triggered by push to `master/main` OR daily schedule.
    - Updates the `nightly` git tag to the current commit.
    - Overwrites the "Nightly Build" release with new assets.
    - **NOT a Draft**: Immediately available.
  - **Stable (Tagged)**:
    - Triggered by pushing a tag like `v1.0.0`.
    - Creates a new standard release titled with the version name.

## Release Artifacts

Each release (Nightly or Stable) contains:

- `app-Release`: The main executable.
- `app-Profiling`: Version for performance analysis.
- `app-UltraRelease`: Experimental high-performance build.
- Source code archives (`zip`, `tar.gz`).
