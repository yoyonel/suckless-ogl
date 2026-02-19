#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Converts an array of 32-bit floats to 16-bit half-floats using SIMD
 * (AVX2/F16C).
 *
 * This function utilizes hardware acceleration to perform the conversion
 * significantly faster than a scalar implementation. It falls back to a scalar
 * loop for the remaining elements (tail) or if SIMD is completely unavailable
 * at compile time.
 *
 * @param src Pointer to the source array of floats.
 * @param dst Pointer to the destination array of half-floats (uint16_t
 * storage).
 * @param count Number of elements to convert.
 */
void convert_float_to_half_simd(const float* src, uint16_t* dst, size_t count);

#endif /* SIMD_UTILS_H */
