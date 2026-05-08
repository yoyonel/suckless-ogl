/**
 * @file dvec3.h
 * @brief Double-precision 3D vector helpers for physics simulation.
 *
 * cglm (v0.9.x) only provides float-precision vec3 operations — there is
 * no dvec3 type and no SIMD acceleration for vec3 (SSE/NEON are only used
 * for vec4 and mat4, where 4-wide lanes map naturally to 128-bit registers).
 *
 * These inline helpers mirror the cglm vec3 API (`glm_vec3_*`) but operate
 * on plain `double[3]` arrays, giving the n-body simulation the numerical
 * precision it needs without sacrificing readability.
 *
 * Performance note: on 14 bodies the overhead of double vs float is
 * negligible.  Should the body count grow to hundreds, consider migrating
 * to AVX double SIMD (2-wide `__m128d` or 4-wide `__m256d`).
 *
 * @see docs/nbody_precision_fix.md for the full rationale.
 */
#ifndef DVEC3_H
#define DVEC3_H

#include <math.h>

/** A double-precision 3D vector stored as a plain array. */
typedef double dvec3[3];

/* ── Element access ─────────────────────────────────────────────────── */

/** Set all components to zero: dest = {0, 0, 0}. */
static inline void dvec3_zero(dvec3 dest)
{
	dest[0] = 0.0;
	dest[1] = 0.0;
	dest[2] = 0.0;
}

/** Copy src into dest. */
static inline void dvec3_copy(const dvec3 src, dvec3 dest)
{
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
}

/** Set dest from three scalars. */
static inline void dvec3_set(dvec3 dest, double x_val, double y_val,
                             double z_val)
{
	dest[0] = x_val;
	dest[1] = y_val;
	dest[2] = z_val;
}

/* ── Arithmetic ─────────────────────────────────────────────────────── */

/** dest = a + b */
static inline void dvec3_add(const dvec3 a_vec, const dvec3 b_vec, dvec3 dest)
{
	dest[0] = a_vec[0] + b_vec[0];
	dest[1] = a_vec[1] + b_vec[1];
	dest[2] = a_vec[2] + b_vec[2];
}

/** dest += b (in-place accumulate). */
static inline void dvec3_addto(dvec3 dest, const dvec3 b_vec)
{
	dest[0] += b_vec[0];
	dest[1] += b_vec[1];
	dest[2] += b_vec[2];
}

/** dest = a - b */
static inline void dvec3_sub(const dvec3 a_vec, const dvec3 b_vec, dvec3 dest)
{
	dest[0] = a_vec[0] - b_vec[0];
	dest[1] = a_vec[1] - b_vec[1];
	dest[2] = a_vec[2] - b_vec[2];
}

/** dest -= b (in-place subtract). */
static inline void dvec3_subfrom(dvec3 dest, const dvec3 b_vec)
{
	dest[0] -= b_vec[0];
	dest[1] -= b_vec[1];
	dest[2] -= b_vec[2];
}

/** dest = v * s (uniform scale). */
static inline void dvec3_scale(const dvec3 v_vec, double scalar, dvec3 dest)
{
	dest[0] = v_vec[0] * scalar;
	dest[1] = v_vec[1] * scalar;
	dest[2] = v_vec[2] * scalar;
}

/** dest += v * s (multiply-add, used in Verlet integration). */
static inline void dvec3_muladds(dvec3 dest, const dvec3 v_vec, double scalar)
{
	dest[0] += v_vec[0] * scalar;
	dest[1] += v_vec[1] * scalar;
	dest[2] += v_vec[2] * scalar;
}

/* ── Products ───────────────────────────────────────────────────────── */

/** Dot product: a · b */
static inline double dvec3_dot(const dvec3 a_vec, const dvec3 b_vec)
{
	return (a_vec[0] * b_vec[0]) + (a_vec[1] * b_vec[1]) +
	       (a_vec[2] * b_vec[2]);
}

/* ── Length / Normalize ──────────────────────────────────────────────── */

/** Squared length: |v|² */
static inline double dvec3_norm2(const dvec3 v_vec)
{
	return dvec3_dot(v_vec, v_vec);
}

/** Length: |v| */
static inline double dvec3_norm(const dvec3 v_vec)
{
	return sqrt(dvec3_norm2(v_vec));
}

/** Normalize v in-place.  No-op if length is zero. */
static inline void dvec3_normalize(dvec3 v_vec)
{
	double len = dvec3_norm(v_vec);
	if (len > 0.0) {
		double inv = 1.0 / len;
		v_vec[0] *= inv;
		v_vec[1] *= inv;
		v_vec[2] *= inv;
	}
}

#endif /* DVEC3_H */
