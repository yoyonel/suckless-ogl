# `glTexSubImage2D` Bottleneck Analysis: Float32 → Float16 Conversion in Mesa 25.0.7

## Motivation

Profiling via [Tracy](profiling_tracy.md) revealed that `glTexSubImage2D` was a significant bottleneck during the resource loading phase, particularly for large HDR environment maps (4K resolution).

- **Problem**: The OpenGL driver (Mesa) performs the `Float32 → Float16` conversion on the CPU using a **scalar** (single-value) path, leading to high CPU usage and stalling the main thread.
- **Goal**: Understand why Mesa does not implement vectorized conversion (AVX2/F16C) for this path, and evaluate alternatives.

---

## Full Call Chain

```
glTexSubImage2D()                                    [src/mesa/main/teximage.c:4096]
  └─► st_TexSubImage()                               [src/mesa/state_tracker/st_cb_texture.c:2117]
        │
        ├─ 1. Fast path: texture_subdata (memcpy)     [line ~2162]
        │      → SKIPPED if source format ≠ dest format (GL_FLOAT ≠ GL_HALF_FLOAT)
        │      → Condition: _mesa_texstore_can_use_memcpy() must return true
        │
        ├─ 2. Blit-based path (GPU blit)              [line ~2198]
        │      → SKIPPED if _mesa_format_matches_format_and_type() fails
        │
        └─ 3. CPU FALLBACK: _mesa_store_texsubimage() [line ~2398]
              └─► store_texsubimage()                  [src/mesa/main/texstore.c:978]
                    └─► _mesa_texstore()               [src/mesa/main/texstore.c:1091]
                          └─► texstore_rgba()          [src/mesa/main/texstore.c:680]
                                └─► _mesa_format_convert()  [src/mesa/main/format_utils.c:278]
                                      │
                                      ├─ If src and dst are array formats:
                                      │    └─► _mesa_swizzle_and_convert()  [format_utils.c:1417]
                                      │          └─► convert_half_float()   [format_utils.c:913]
                                      │                case MESA_ARRAY_FORMAT_TYPE_FLOAT:
                                      │                  SWIZZLE_CONVERT(uint16_t, float,
                                      │                    _mesa_float_to_half(src))  ← HERE
                                      │
                                      └─ Otherwise (via float intermediate):
                                           └─► _mesa_pack_float_rgba_row() [format_pack.h:38]
```

---

## Files Involved (Detailed)

### 1. GL Entry Point — `src/mesa/main/teximage.c`

**Line 4096** — `_mesa_TexSubImage2D()`:

```c
_mesa_TexSubImage2D(GLenum target, GLint level,
                    GLint xoffset, GLint yoffset,
                    GLsizei width, GLsizei height,
                    GLenum format, GLenum type, const GLvoid *pixels)
```

Dispatches to `ctx->Driver.TexSubImage` which points to `st_TexSubImage`.

### 2. State Tracker — `src/mesa/state_tracker/st_cb_texture.c`

**Line 2117** — `st_TexSubImage()`:

The state tracker attempts 3 paths, in order:

#### Fast path: `texture_subdata` (lines 2157-2196)

```c
if (pixels &&
    !unpack->BufferObj &&
    !is_ms &&
    _mesa_texstore_can_use_memcpy(ctx, texImage->_BaseFormat,
                                  texImage->TexFormat, format, type,
                                  unpack)) {
    // ... direct memcpy via pipe->texture_subdata()
    return;
}
```

**Failure condition**: `_mesa_texstore_can_use_memcpy()` checks that the source format (GL_FLOAT) matches the texture format (HALF_FLOAT) **exactly**. It does not → **skipped**.

#### Blit-based path (lines 2198-2390)

Creates an intermediate GPU texture, copies data via memcpy, then blits. But if `_mesa_format_matches_format_and_type()` already matches (meaning the memcpy fallback would be just as fast), the code jumps to the fallback.

#### CPU Fallback (lines 2393-2400)

```c
fallback:
   _mesa_store_texsubimage(ctx, dims, texImage, xoffset, yoffset, zoffset,
                           width, height, depth, format, type, pixels, unpack);
```

### 3. Fallback Texture Store — `src/mesa/main/texstore.c`

**Line 978** — `store_texsubimage()`:
Maps the texture into CPU memory, then calls `_mesa_texstore()` → `texstore_rgba()`.

**Line 818** — `texstore_rgba()` calls `_mesa_format_convert()`:

```c
_mesa_format_convert(dstSlices[img], dstFormat, dstRowStride,
                     src, srcMesaFormat, srcRowStride,
                     srcWidth, srcHeight,
                     needRebase ? rebaseSwizzle : NULL);
```

### 4. Format Conversion — `src/mesa/main/format_utils.c`

**Line 278** — `_mesa_format_convert()`:
When src and dst are both array formats (FLOAT RGBA and HALF RGBA), the code goes directly through:

```c
_mesa_swizzle_and_convert(dst, dst_type, dst_num_channels,
                          src, src_type, src_num_channels,
                          src2dst, normalized, width);
```

**Line 1417** — `_mesa_swizzle_and_convert()`:
The switch on `dst_type` dispatches to `convert_half_float()`:

```c
case MESA_ARRAY_FORMAT_TYPE_HALF:
    convert_half_float(void_dst, num_dst_channels, void_src, src_type,
                       num_src_channels, swizzle, normalized, count);
    break;
```

**Line 913** — `convert_half_float()`:

```c
case MESA_ARRAY_FORMAT_TYPE_FLOAT:
    SWIZZLE_CONVERT(uint16_t, float, _mesa_float_to_half(src));
    break;
```

### 5. The Scalar Loop — `SWIZZLE_CONVERT_LOOP` Macro

**Line 728** — The macro expands to:

```c
for (s = 0; s < count; ++s) {           // for each pixel
    for (j = 0; j < SRC_CHANS; ++j) {   // for each channel (R, G, B, A)
        SRC_TYPE src = typed_src[j];
        tmp[j] = _mesa_float_to_half(src);  // ← scalar conversion
    }
    typed_dst[0] = tmp[swizzle_x];
    typed_dst[1] = tmp[swizzle_y];
    typed_dst[2] = tmp[swizzle_z];
    typed_dst[3] = tmp[swizzle_w];
    typed_src += SRC_CHANS;
    typed_dst += DST_CHANS;
}
```

For an RGBA 4K texture (4096×2048), this results in **4096 × 2048 × 4 = ~33 million** calls to `_mesa_float_to_half()`.

### 6. The Scalar Conversion — `src/util/half_float.h`

**Line 58** — `_mesa_float_to_half()`:

```c
static inline uint16_t
_mesa_float_to_half(float val)
{
#if defined(USE_X86_64_ASM)
   if (util_get_cpu_caps()->has_f16c) {
      __m128 in = {val};          // loads ONE float into XMM (128-bit)
      __m128i out;
      __asm volatile("vcvtps2ph $0, %1, %0" : "=v"(out) : "v"(in));
      return out[0];              // extracts ONE half
   }
#endif
   return _mesa_float_to_half_slow(val);  // pure software fallback
}
```

**Even with F16C available**, the conversion is **scalar**: `vcvtps2ph` can convert 4 floats (XMM) or 8 floats (YMM/AVX2) in a single instruction, but here it converts only **one value** per call.

### 7. Software Fallback — `src/util/half_float.c`

**Line 57** — `_mesa_float_to_half_slow()`:
Pure software conversion via bit manipulation (mantissa/exponent extraction, denorm/NaN/Inf handling, rounding). Even slower than the scalar F16C path.

---

## Git History (blame) and Merge Requests

### Key Commits in Chronological Order

| Date | Commit | Author | Description | MR |
|------|--------|--------|-------------|-----|
| 2014-07-11 | `d55f77b503ab` | **Jason Ekstrand** (Intel) | Created the `SWIZZLE_CONVERT` framework | _pre-GitLab_ |
| 2014-09-12 | `418da979053d` | **Brian Paul** (VMware) | Refactored `SWIZZLE_CONVERT_LOOP` macro | _pre-GitLab_ |
| 2014-09-12 | `cfeb394224f2` | **Brian Paul** (VMware) | Extracted `convert_half_float()` (compile time ÷4) | _pre-GitLab_ |
| 2014-11-27 | `ea79ab3e8c37` | **Iago Toral Quiroga** (Igalia) | Replaced GL types → `MESA_ARRAY_FORMAT_TYPE_*` | _pre-GitLab_ |
| **2020-09-18** | **`ffcdf76799b0`** | **Marek Olšák** (AMD) | **Added F16C inline asm (scalar)** | [**!6987**](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/6987) |
| 2021-02-25 | `a9618e7c4214` | Rob Clark | `util_get_cpu_caps()` accessor | [!9266](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/9266) |
| 2023-06-27 | `c9a5cac4ffa4` | Konstantin Seurer | `immintrin.h` → `xmmintrin.h` (compile time -22%) | [!23871](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/23871) |
| 2024-10-03 | `3c0bf4238188` | David Heidelberg | aarch64 `fcvt` half→float (not float→half!) | [!31564](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/31564) |

### MR !6987 — The F16C Commit (Details)

- **Title**: _"Implement F16C using inline assembly on x86_64"_
- **Author**: Marek Olšák (`@mareko`, AMD)
- **Branch**: `f16c-v2`
- **Reviewer**: Matt Turner (Intel)
- **Merged**: October 7, 2020
- **Primary motivation**: Fix `bptc-float-modes` test on llvmpipe
- **Approach**: Optimize the existing scalar `_mesa_float_to_half()` function, not create a batch variant

---

## Why Mesa Has No Vectorized Conversion

### 1. Layered Architecture Incompatible with Vectorization

The `SWIZZLE_CONVERT_LOOP` framework (Jason Ekstrand, 2014) was designed as a **generic** format converter:

- Iterates **pixel by pixel**, **channel by channel**
- The conversion is passed as a macro parameter (`CONV = _mesa_float_to_half(src)`)
- Swizzling (RGBA→BGRA reordering, etc.) is **interleaved** with conversion
- The code relies on compiler loop unrolling (`nested switch` on `DST_CHANS` × `SRC_CHANS`) rather than explicit vectorization

Vectorizing this code would require **significant refactoring**: separating conversion from swizzle, or creating a specialized `float[4] → half[4]` path with SIMD intrinsics.

### 2. `_mesa_float_to_half` Was Designed as a Scalar API

When Marek Olšák added F16C support (MR !6987, 2020), he optimized the **existing scalar function**:

```c
// Takes ONE float, returns ONE uint16_t
static inline uint16_t _mesa_float_to_half(float val);
```

His motivation was to **fix a bug** (`bptc-float-modes` on llvmpipe), not to optimize texture upload throughput. He never added a batch variant such as:

```c
// Hypothetical — does NOT exist in Mesa
void _mesa_floats_to_halfs(uint16_t *dst, const float *src, size_t count);
```

### 3. llvmpipe Already Does Vectorized Conversion... via JIT

In `src/gallium/auxiliary/gallivm/lp_bld_conv.c` (line 183), llvmpipe uses LLVM to emit **vectorized** instructions:

```c
if (util_get_cpu_caps()->has_f16c &&
    (length == 4 || length == 8)) {
    intrinsic = "llvm.x86.vcvtps2ph.128";  // 4 floats at once
    // or "llvm.x86.vcvtps2ph.256"          // 8 floats at once (AVX)
}
```

This code is reserved for the **JIT rendering pipeline** (shaders compiled on-the-fly) and is **never used** for CPU-side texture uploads via `glTexSubImage2D`.

### 4. Historical Issues with AVX Intrinsics

Mesa release notes for **18.3.3**, **19.0.0**, and **19.3.0** document recurring build issues with `_mm256_cvtps_ph`:
> _"format_types.h:1220: undefined reference to `_mm256_cvtps_ph`"_

The SWR driver (Intel Software Rasterizer, now deprecated) emitted AVX/F16C intrinsics without verifying compiler and target support. These bugs made Mesa developers **cautious** about using SIMD intrinsics in C "hot path" code.

### 5. The CPU Path Is Not the Primary Use Case

For hardware drivers (radeonsi, iris, nouveau...):

- `st_TexSubImage` first tries `texture_subdata` (direct memcpy if formats match)
- Then GPU blit (conversion done by the GPU)
- The CPU fallback is only reached when **source format ≠ destination format**

Mesa developers assume that applications **should provide data in the texture's native format** (i.e., pass `GL_HALF_FLOAT` if the internal texture format is `GL_RGBA16F`).

---

## Concrete Impact for a 4K RGBA HDR Texture

| Parameter | Value |
|---|---|
| Resolution | 4096 × 2048 |
| Channels | 4 (RGBA) |
| Total pixels | 8,388,608 |
| Calls to `_mesa_float_to_half()` | **33,554,432** |
| Instruction per call | `vcvtps2ph` (if F16C) or software fallback |
| XMM register utilization | 1/4 (4 slots available, 1 used) |
| YMM register utilization | 1/8 (8 slots available, not used) |

### Theoretical Gain from Vectorization

| Method | Conversions/instruction | Theoretical speedup |
|---|---|---|
| Current scalar (XMM, 1 float) | 1 | 1× |
| SSE/XMM vectorized (4 floats) | 4 | ~4× |
| AVX/YMM vectorized (8 floats) | 8 | ~8× |

---

## Alternatives and Workarounds

### Application-side (recommended by Mesa)

1. **Supply data as Float16** directly to `glTexSubImage2D` (`type = GL_HALF_FLOAT`), which activates the fast memcpy path
2. **Perform the F32→F16 conversion application-side** using AVX2/F16C intrinsics before the upload, freeing the driver from an expensive conversion
3. **Use PBOs (Pixel Buffer Objects)** to make the upload asynchronous

### Driver-side Mesa (potential improvements)

1. Add a batch variant `_mesa_floats_to_halfs_avx2()` using `vcvtps2ph` on YMM registers
2. Create a specialized path in `convert_half_float()` when the swizzle is identity (RGBA→RGBA)
3. Integrate vectorized conversion directly into `_mesa_format_convert()` for the `FLOAT→HALF` case without swizzle

---

## Conclusion

The observed bottleneck is **real and confirmed by the source code**: the Float32→Float16 conversion in Mesa is scalar even on CPUs with F16C support. This is the result of accumulated design decisions:

1. A **generic** conversion framework designed in 2014 for flexibility, not performance
2. A 2020 F16C optimization targeting **bug fixing**, not throughput
3. An assumption that applications provide data in the **correct format**
4. The fact that llvmpipe's JIT code **already vectorizes** this path, but only for shaders

For the specific use case (asynchronous HDR resource loading on CPU threads while the GPU renders), the optimal solution is to perform the conversion **application-side** with SIMD instructions before calling `glTexSubImage2D`, allowing the driver to perform a simple `memcpy`.
