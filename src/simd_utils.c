#include "simd_utils.h"

#include "log.h"
#include <immintrin.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Check for F16C support. Usually implied by AVX2/Broadwell+, but explicit
 * check is safer. If __F16C__ is not defined, we fall back to a software
 * implementation.
 */
#if defined(__F16C__) && defined(__AVX__)

/* Mask for rounding mode: Round to nearest even */
#define F16C_ROUND_MODE (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)

#define SIMD_BATCH_SIZE 8

static inline uint16_t float_to_half_intrinsic(float value)
{
	__m128 vec = _mm_set_ss(value);
	__m128i result = _mm_cvtps_ph(vec, F16C_ROUND_MODE);
	/* Extract the lower 16 bits */
	return (uint16_t)_mm_cvtsi128_si32(result);
}

void convert_float_to_half_simd(const float* src, uint16_t* dst, size_t count)
{
	static bool s_logged = false;
	size_t idx = 0;

/* AVX2 path processing 8 floats at a time */
#ifdef __AVX2__
	if (!s_logged) {
		LOG_INFO("simd_utils", "SIMD Optimization: AVX2/F16C Enabled");
		s_logged = true;
	}
	for (; idx + SIMD_BATCH_SIZE <= count; idx += SIMD_BATCH_SIZE) {
		/* Load 8 floats (32-bit * 8 = 256 bits) */
		/* Use loadu for unaligned access (safer) */
		__m256 v_fp32 = _mm256_loadu_ps(&src[idx]);

		/* Convert to 8 half-floats (16-bit * 8 = 128 bits) */
		/* NOLINTNEXTLINE(readability-magic-numbers) */
		__m128i v_fp16 = _mm256_cvtps_ph(v_fp32, F16C_ROUND_MODE);

		/* Store 128 bits (8 shorts) to destination */
		_mm_storeu_si128((__m128i*)&dst[idx], v_fp16);
	}
#else
	if (!s_logged) {
		LOG_INFO("simd_utils",
		         "SIMD Optimization: F16C Enabled (No AVX2)");
		s_logged = true;
	}
#endif /* __AVX2__ */

	/* Handle remaining elements (scalar fallback using F16C intrinsic) */
	for (; idx < count; ++idx) {
		dst[idx] = float_to_half_intrinsic(src[idx]);
	}
}

#else /* Software Fallback (No F16C) */

/* Magic numbers for IEEE 754 float/half conversion */
static const uint32_t SIGN_MASK = 0x80000000U;
static const uint32_t EXP_MASK = 0x7F800000U;
static const uint32_t MANT_MASK = 0x007FFFFFU;
static const int EXP_SHIFT = 23;
static const int EXP_BIAS_F32 = 127;
static const int EXP_BIAS_F16 = 15;
static const int EXP_MAX_F16 = 31;
static const int EXP_MAX_F32 = 255;
static const int MANT_SHIFT_DIFF = 13; /* 23 - 10 */
static const int HALF_MANT_SHIFT = 10;
static const uint16_t HALF_QNAN_BIT = 0x0200U;
/* 31 - 15 */
static const int HALF_SIGN_SHIFT = 16;

static uint16_t float_to_half_soft(float value)
{
	union {
		float f;
		uint32_t u;
	} val_union;
	val_union.f = value;

	/* Align sign bit for half (bit 15) */
	uint32_t sign = (val_union.u & SIGN_MASK) >> HALF_SIGN_SHIFT;
	uint32_t exponent = (val_union.u & EXP_MASK) >> EXP_SHIFT;
	uint32_t mantissa = val_union.u & MANT_MASK;

	uint16_t h_sign = (uint16_t)sign;
	uint16_t h_exponent = 0;
	uint16_t h_mantissa = 0;

	if (exponent == 0) {
		/* Zero / Denormal (flush to zero) */
		h_exponent = 0;
		h_mantissa = 0;
	} else if (exponent == EXP_MAX_F32) { /* Inf / NaN (255) */
		h_exponent = (uint16_t)EXP_MAX_F16;
		h_mantissa = mantissa ? HALF_QNAN_BIT : 0;
	} else {
		/* Normalized */
		int new_exponent = (int)exponent - EXP_BIAS_F32 + EXP_BIAS_F16;
		if (new_exponent <= 0) {
			/* Underflow */
			h_exponent = 0;
			h_mantissa = 0;
		} else if (new_exponent >= EXP_MAX_F16) {
			/* Overflow */
			h_exponent = (uint16_t)EXP_MAX_F16;
			h_mantissa = 0;
		} else {
			h_exponent = (uint16_t)new_exponent;
			h_mantissa = (uint16_t)(mantissa >> MANT_SHIFT_DIFF);
		}
	}

	/* Force unsigned arithmetic to avoid lint errors about signed bitwise
	 * ops (integer promotion) */
	return (uint16_t)((uint32_t)h_sign |
	                  ((uint32_t)h_exponent << HALF_MANT_SHIFT) |
	                  (uint32_t)h_mantissa);
}

void convert_float_to_half_simd(const float* src, uint16_t* dst, size_t count)
{
	static bool s_logged = false;
	if (!s_logged) {
		LOG_WARN("simd_utils",
		         "SIMD Optimization: Software Fallback (No F16C/AVX) - "
		         "VERY SLOW");
		s_logged = true;
	}
	for (size_t i = 0; i < count; ++i) {
		dst[i] = float_to_half_soft(src[i]);
	}
}

#endif
