# Profiling Guide: ApiTrace & Trace Analysis

This guide explains how to use **ApiTrace** in combination with our custom analysis tool to measure GPU performance, with a focus on overcoming driver-specific profiling limitations for compute shaders.

## Why use this workflow?

Standard GPU profilers (and even `apitrace replay --pgpu`) often report **0.00ms** for `glDispatchCompute` calls on many drivers/architectures because they rely on driver-internal counters that don't always track compute asynchronously or accurately.

Our engine injects manual `GL_TIMESTAMP` queries and `glPushDebugGroup` markers into the command stream. The `trace_analyze.py` tool correlates these timestamps with the trace dump to provide a "Source of Truth" for compute shaders.

---

## 1. Prerequisites

Ensure you have **ApiTrace** installed on your system.
The `Makefile` expects `apitrace` to be in your PATH, or you can point to a specific binary using `APITRACE_BIN`.

```bash
# Example for a specific location
export APITRACE_BIN="/path/to/apitrace"
```

---

## 2. Generating a Trace

To record the execution of the application:

```bash
make apitrace
```

This will:
1. Build the application in "Profile" mode (without high-level debug overhead).
2. Use `apitrace trace` to record all OpenGL calls into `build-profile/app.trace`.

---

## 3. High-Level Performance Analysis

The easiest way to see the results is to use the integrated target:

```bash
make trace-perf
```

This command runs `scripts/trace_analyze.py` which produces two tables:
1. **Performance by Shader (Cumulative)**: Groups all calls by shader name/label.
2. **Debug Groups (Per Instance)**: Shows the chronological execution of marked blocks (e.g., IBL generation steps).

### Understanding the Metrics

| Column | Description |
| :--- | :--- |
| **GPU [ms]** | Driver-reported duration (Standard `glDraw*` calls). Use this for Vertex/Fragment shaders. |
| **Timer [ms]** | **Manual GL_TIMESTAMP markers**. Use this for **Compute Shaders** and IBL generation blocks. |
| **Avg/Fr[ms]** | Cumulative GPU time divided by the total number of frames in the trace. |

---

## 4. Advanced Tool Usage

You can run the script manually for more control:

```bash
# Usage: python3 scripts/trace_analyze.py <trace_file> [apitrace_bin]
python3 scripts/trace_analyze.py build-profile/app.trace
```

### Script Logic
- **Regex Parsing**: The script parses `apitrace dump` to extract `glObjectLabel` (to name shaders) and `glGetQueryObjectui64v` (to get manual timestamps).
- **Matching**: It looks for query result fetches that happen immediately after a debug group ends to measure that group's duration.
- **Nested Sums**: If a parent debug group doesn't have its own timer, the script sums up the durations of its direct children (marked with `*` in the table).

---

## 5. Coding for Profiling

To make a new feature measurable:

1. **Label your shaders**: Use `shader_set_label(shader, "Description")` in C.
2. **Add debug groups**:
   ```c
   GL_PUSH_GROUP("My Complex Task", 0);
   // ... GPU calls ...
   GL_POP_GROUP();
   ```
3. **Add fine-grained timers (Optional)**:
   The engine automatically attempts to capture timestamps around critical IBL sections. See `src/pbr.c` for examples.

---

## 6. Developing the Tool

The analysis script is fully tested:
- **Linting**: `make lint` (uses Ruff).
- **Formatting**: `make format` (uses Ruff).
- **Tests**: `make test-python` (uses Pytest).
- **Coverage**: `make coverage` (generates HTML report for Python logic).
