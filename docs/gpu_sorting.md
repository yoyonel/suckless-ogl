# GPU Sphere Sorting Documentation

This document provides a deep dive into the sorting algorithms used in this project to enable correct transparency for thousands of spheres.

## 1. Why Sorting?

In computer graphics, transparency requires rendering fragments in **Back-to-Front** order (the "Painter's Algorithm"). If a near sphere is rendered before a far sphere, the depth buffer will prevent the far sphere from being drawn, even though it should be visible through the near one's semi-transparent pixels.

## 2. Bitonic Sort (GPU Implementation)

The **Bitonic Sort** is a "Sorting Network" algorithm. Unlike `qsort` or `std::sort`, it performs a fixed sequence of comparisons regardless of the data values.

### How it Works

1. **Bitonic Sequence**: A sequence $a_0, a_1, \dots, a_n$ is bitonic if it monotonically increases and then monotonically decreases (or can be circularly shifted to satisfy this).
1. **Bitonic Split**: If you have a bitonic sequence and compare $a_i$ with $a_{i+n/2}$, swapping them to put the smaller first, you get two smaller bitonic sequences where every element in the first is smaller than every element in the second.
1. **Recursion**: By recursively applying this split, you eventually reach individual sorted elements.

### Why it's Perfect for the GPU

- **No Branching**: A GPU shader is like a factory line. If one worker (thread) has to do an "if" and another doesn't, the whole line slows down (Divergence). Bitonic sort has _zero_ divergence because every thread performs exactly the same comparison pattern.

- **Massive Parallelism**: With a complexity of $O(N \log^2 N)$, it does more total work than $O(N \log N)$ algorithms, but it does it **simultaneously** across thousands of GPU cores.

### Our Optimized "Proxy" Approach

Initially, we swapped 128-byte `SphereInstance` structs. This was slow due to VRAM bandwidth.

- **Solution**: We extract 8-byte **Proxy Entries** (4-byte float depth, 4-byte int index).

- **Prepare Pass**: Compute depths once.

- **Sort Pass**: Bitonic sort on 8-byte proxies (highly cache-efficient).

- **Permute Pass**: Use sorted indices to copy the 128-byte structs to the final buffer in one single jump.

## 3. Radix Sort (CPU Implementation)

For the CPU, we implemented a **LSD (Least Significant Digit) Radix Sort**. This is a non-comparative algorithm that doesn't compare elements with each other.

### How it Works

1. **Bucketing**: Instead of comparing A with B, we look at the "digits" of the value. In our case, we treat a 32-bit float as four 8-bit "digits" (base 256).
1. **Passes**:

- Pass 1: Sort by bits 0-7.

- Pass 2: Sort by bits 8-15.

- Pass 3: Sort by bits 16-23.

- Pass 4: Sort by bits 24-31.

1. **Counting**: In each pass, we count how many items fall into each of the 256 "buckets", compute a prefix sum (offsets), and move items to a temporary buffer.

### The "IEEE-754 Float Trick"

Sorting floats with Radix is tricky because negative floats' bit representations don't naturally sort in numerical order.

- **The Solution**: We apply a transformation to the raw bits:
  - If the float is positive, we flip the sign bit.

  - If the float is negative, we flip _all_ bits.

- **Result**: This maps the floats into a 32-bit unsigned integer space where binary order matches numerical order. After sorting, we flip the bits back.

### Performance

- **Complexity**: $O(N \times K)$ where $K$ is the number of bits. Since $K$ is constant (32), this is effectively **$O(N)$**.

- **Speed**: For 100,000 spheres, it is significantly faster than `qsort` because it avoids the overhead of function callbacks and pointer dereferencing for every comparison.

## 4. Comparison Table

| Metric | CPU (qsort) | CPU (Radix) | GPU (Bitonic) |
| :----- | :---------- | :---------- | :------------ |

| **Logic** | Comparison-based | Distribution-based | Network-based |

| **Theoretical Complexity** | $O(N \log N)$ | $O(N)$ | $O(N \log^2 N)$ |

| **Data Dependencies** | High | None | None |

| **Implementation** | `qsort()` | Custom 4-pass | GLSL Compute |

| **Best For** | < 1,000 items | 1,000 - 100,000 | > 10,000 or GPU-native data |

## 5. Summary

We use **three** distinct paths:

1. **CPU Qsort**: Solid baseline, used for tests and reference.
1. **CPU Radix**: The "Golden Path" for high-performance sorting when data is already on the CPU.
1. **GPU Bitonic**: The "Scalability Path" for extreme counts or when we want to keep the CPU completely free for other tasks.
