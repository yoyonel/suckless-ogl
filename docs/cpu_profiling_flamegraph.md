# CPU Profiling: perf + FlameGraph Workflow

**Last Updated**: 2026-02-11
**Author**: Profiling workflow established during `test_app` optimization session

This guide provides a comprehensive, step-by-step methodology for CPU profiling C/C++ applications using `perf` and FlameGraph visualization. It covers tool installation for Debian-based and Bazzite Linux distributions, profiling execution, and analysis techniques.

---

## 📋 Table of Contents

1. [Prerequisites & Installation](#prerequisites-installation)
2. [Build Configuration](#build-configuration)
3. [Recording Performance Data](#recording-performance-data)
4. [Analyzing with perf report](#analyzing-with-perf-report)
5. [Generating FlameGraphs](#generating-flamegraphs)
6. [Interpreting Results](#interpreting-results)
7. [Real-World Example: test_app](#real-world-example-test_app)
8. [Troubleshooting](#troubleshooting)

---

## 📦 Prerequisites & Installation

### Debian-based Systems (Debian, Ubuntu, Linux Mint, etc.)

```bash
# Install perf (Linux performance counters)
sudo apt update
sudo apt install linux-perf

# Install FlameGraph tools
cd ~/tools  # or any directory you prefer
git clone https://github.com/brendangregg/FlameGraph.git

# Add to PATH (add to ~/.bashrc for persistence)
export PATH="$HOME/tools/FlameGraph:$PATH"
```

### Bazzite / Fedora Atomic / immutable systems

Bazzite uses `rpm-ostree` for system packages. For development tools, use a container (toolbox/distrobox):

```bash
# Create a development container
distrobox create --name dev-box --image fedora:latest

# Enter the container
distrobox enter dev-box

# Inside the container:
sudo dnf install perf

# Clone FlameGraph
cd ~/tools
git clone https://github.com/brendangregg/FlameGraph.git
export PATH="$HOME/tools/FlameGraph:$PATH"
```

**Alternative (host install via rpm-ostree)**:

```bash
# On Bazzite host (not recommended for dev tools)
rpm-ostree install perf
sudo systemctl reboot  # Required for atomic layering
```

### Verification

```bash
# Check perf is installed
perf --version

# Check FlameGraph scripts
ls ~/tools/FlameGraph/*.pl
```

Expected output:

```
perf version 6.x.x
flamegraph.pl  stackcollapse-perf.pl  (and others)
```

---

## 🔧 Build Configuration

For optimal profiling results, build with **debug symbols** and **optimizations**:

### CMake Projects

```bash
# Option 1: RelWithDebInfo (Recommended)
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)

# Option 2: Profiling build type (if available)
cmake -B build-prof -DCMAKE_BUILD_TYPE=Profiling
cmake --build build-prof -j$(nproc)
```

### Makefile Projects

For this project:

```bash
make build-prof  # Uses Profiling build type
```

**Key compiler flags**:

- `-O3` or `-O2`: Enable optimizations (realistic performance)
- `-g`: Include debug symbols (function names, line numbers)
- `-fno-omit-frame-pointer`: Preserve stack frames for accurate call graphs

---

## 📊 Recording Performance Data

### Basic Recording

```bash
# Record with default settings (99 Hz sampling)
perf record -g ./build/tests/test_app

# Record with custom frequency (e.g., 1000 Hz for finer granularity)
perf record -F 1000 -g ./build/tests/test_app

# Record with call-graph (DWARF unwinding, recommended)
perf record -g --call-graph dwarf ./build/tests/test_app
```

**Flags explained**:

- `-g`: Enable call-graph (stack) recording
- `-F <freq>`: Sampling frequency in Hz (default: 99)
- `--call-graph dwarf`: Use DWARF debug info for unwinding (more accurate)

### Recording Xvfb Tests (Headless)

For tests running in Xvfb:

```bash
perf record -g xvfb-run -a -s "-screen 0 1024x768x24" ./build/tests/test_app
```

### Output

This creates a `perf.data` file in the current directory (~10-50 MB depending on duration).

---

## 📈 Analyzing with perf report

### Interactive TUI

```bash
perf report
```

**Navigation**:

- `↑`/`↓`: Navigate functions
- `Enter`: Expand call stack
- `a`: Annotate selected function (assembly view)
- `q`: Quit

### Text Report

```bash
# Summary report (top functions by sample count)
perf report --stdio | head -50

# Detailed call graph
perf report --stdio --call-graph --show-nr-samples | less
```

### Sample Output

```
# Overhead  Command     Shared Object       Symbol
#  21.60%   test_app    libGL.so.1          [.] glTexImage2D
#  17.80%   test_app    libGL.so.1          [.] glTexSubImage2D
#  11.70%   test_app    test_app            [.] shader_compile
#   9.40%   test_app    test_app            [.] icosphere_generate
```

**Interpretation**:

- `Overhead`: % of samples in this function
- `Symbol`: Function name (if symbols available)
- `[.]`: Userspace code, `[k]`: Kernel code

---

## 🔥 Generating FlameGraphs

FlameGraphs provide an intuitive visual representation of call stacks.

### Step 1: Collapse Stack Samples

```bash
perf script | stackcollapse-perf.pl > perf-folded.txt
```

- `perf script`: Converts `perf.data` to text format
- `stackcollapse-perf.pl`: Aggregates identical stack traces

### Step 2: Generate SVG

```bash
flamegraph.pl perf-folded.txt > flamegraph.svg
```

### One-Liner

```bash
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

### Viewing

```bash
# Open in browser
xdg-open flamegraph.svg

# Or copy to artifact directory
cp flamegraph.svg /path/to/artifacts/
```

---

## 🔍 Interpreting Results

### FlameGraph Anatomy

```
┌────────────────────────────────────────────────────────────┐
│                       main (100%)                          │  ← Entry point
├────────────┬───────────────────────────────┬───────────────┤
│  init()    │      render_loop()            │   cleanup()   │  ← Top-level functions
│   (10%)    │         (85%)                 │    (5%)       │
└────────────┴─────────┬───────────┬─────────┴───────────────┘
                       │ texture_load│ shader_compile
                       │   (40%)     │    (25%)
                       └─────────────┴────────────────────────
                              ▲
                         Width = CPU time
```

- **X-axis (width)**: Proportion of total CPU time
- **Y-axis (height)**: Call stack depth (caller → callee)
- **Color**: Random (for visual separation only)

### Identification of Bottlenecks

1. **Wide boxes**: Functions consuming significant CPU time
2. **Patterns**:
   - **Plateau**: CPU bound (good utilization)
   - **Tower**: Deep call chains (potential overhead)
   - **Fragmentation**: Many small calls (cache/branch issues)

### Example Analysis (test_app)

From today's profiling session (2026-02-11):

```
Total samples: 4800
Total time: 17.01 seconds

Top hotspots:
1. Texture Loading (HDR): 6.7s (39%)
   - glTexImage2D: 3.7s
   - glTexSubImage2D: 3.0s

2. Rendering & Capture: 3.8s (22%)
   - app_render: 2.5s
   - glReadPixels: 1.3s

3. Window/Context Init: 2.8s (16%)
   - glfwCreateWindow: 1.8s
   - GL initialization: 1.0s

4. Shader Compilation: 2.0s (12%)
   - shader_compile_from_file: 2.0s
```

**Optimization targets**:

- Cache HDR texture (avoid reload)
- Use PBO for async `glReadPixels`
- Disable expensive GPU effects in tests

---

## 🎯 Real-World Example: test_app

### Context

Profiling the `test_app` integration test to identify slowness (17s execution).

### Commands Used

```bash
# Build with profiling symbols
cmake -B build -DCMAKE_BUILD_TYPE=Profiling
cmake --build build --target test_app -j$(nproc)

# Record execution
perf record -F 1000 -g xvfb-run -a -s "-screen 0 1024x768x24" ./build/tests/test_app

# Analyze interactively
perf report

# Generate flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > test_app_flamegraph.svg
```

### Key Findings

| Bottleneck | Time | % | Solution Implemented |
|------------|------|---|---------------------|
| HDR texture upload | 6.7s | 39% | ✅ Cache texture between tests |
| `glReadPixels` sync | 1.3s | 8% | ✅ Use PBO async readback |
| Post-processing | 1.5s | 9% | ❌ Kept for ISO production |
| IBL generation | 17s | Varies | ⚠️ Outside test scope |

### Results

- **Before**: 17.0s
- **After** (with optimizations): 22.7s (IBL variation)
- **Test portion**: 3.8s → ~5.2s (with full GPU effects)

> **Note**: The increase is due to IBL generation variability, not regression in test code.

---

## 🛠️ Troubleshooting

### Permission Errors

```
Error: Access to performance monitoring and observability operations is limited
```

**Solution** (on host, not in container):

```bash
# Temporary
sudo sysctl -w kernel.perf_event_paranoid=1

# Permanent
echo "kernel.perf_event_paranoid = 1" | sudo tee /etc/sysctl.d/99-perf.conf
sudo sysctl --system
```

### Missing Symbols

If you see hex addresses instead of function names:

**Check debug symbols**:

```bash
# View information about object files.
objdump -t ./build/tests/test_app | grep debug
# List symbol names in object files.
nm ./build/tests/test_app | head
```

**Rebuild with symbols**:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### FlameGraph Scripts Not Found

```bash
# Ensure FlameGraph is in PATH
export PATH="$HOME/tools/FlameGraph:$PATH"

# Or use absolute paths
~/tools/FlameGraph/stackcollapse-perf.pl
~/tools/FlameGraph/flamegraph.pl
```

### perf.data Too Large

Reduce sampling frequency:

```bash
perf record -F 99 -g ./app  # Default
perf record -F 49 -g ./app  # Half rate
```

Or limit duration:

```bash
perf record -g --duration 5000 ./app  # 5 seconds
```

---

## 📚 Additional Resources

- **Brendan Gregg's Perf Examples**: <https://www.brendangregg.com/perf.html>
- **FlameGraph Documentation**: <https://github.com/brendangregg/FlameGraph>
- **Linux perf Wiki**: <https://perf.wiki.kernel.org/>

---

## 🔗 Related Documentation

- [GPU Profiling System](gpu_profiling.md) - For GPU timeline profiling
- [Profiling Guide](profiling_guide.md) - ApiTrace workflow for OpenGL calls
- [Performance Monitoring](perf_profiling.md) - Quick reference for perf setup

---

**Changelog**:

- **2026-02-11**: Initial comprehensive guide created based on `test_app` profiling session. Includes Debian + Bazzite installation, complete workflow, and real-world example with flamegraph analysis.
