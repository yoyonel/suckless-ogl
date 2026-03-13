# Build Instructions

## Quick Start

Use the convenient Makefile targets from the project root:

```bash

# Build with dynamic shaders (debug mode)

make debug

# Build with optimized static shaders (release mode)

make release

# Run the application

make run

# Run tests

make test

# Clean build directory

make clean

# Show all available targets

make help-user

```

## Shader Optimization Modes

### Debug Mode (Default)

- **Command**: `make -f Makefile.user debug`

- **Behavior**: Shaders use runtime conditionals (`if` statements)

- **Advantages**:
  - Effects can be toggled at runtime

  - Faster iteration during development

- **Use case**: Development, debugging, testing

### Release Mode

- **Command**: `make -f Makefile.user release`

- **Behavior**: Shaders use compile-time constants (`#define`)

- **Advantages**:
  - GPU driver optimizes away unused branches

  - Better performance (reduced branching overhead)

- **Use case**: Production builds, performance benchmarking

## Manual CMake Configuration

If you prefer direct CMake commands:

```bash

# Debug mode

cd build
cmake -DENABLE_SHADER_OPTIMIZATION=OFF ..
make -j$(nproc)

# Release mode

cd build
cmake -DENABLE_SHADER_OPTIMIZATION=ON ..
make -j$(nproc)

```

## Verification

To verify which mode is active, check the startup logs:

```bash
./build/app 2>&1 | grep "BUILD OPTION"

```

- **Debug mode**: No output

- **Release mode**: `BUILD OPTION: Compiling OPTIMIZED shader...`
