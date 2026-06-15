#ifndef NBODY_INTERNAL_H
#define NBODY_INTERNAL_H

#include "nbody_types.h"

static const double HALF = 0.5;

/* Per-pair Plummer softening: ε² = max(NBODY_SOFTENING_SQ, (F·(r_i+r_j))²). */
static inline double pair_softening_sq(double radius_i, double radius_j)
{
	double sum_r = radius_i + radius_j;
	double eps = (double)NBODY_SOFTENING_FACTOR * sum_r;
	double eps_sq = eps * eps;
	return eps_sq > (double)NBODY_SOFTENING_SQ ? eps_sq
	                                           : (double)NBODY_SOFTENING_SQ;
}

#endif /* NBODY_INTERNAL_H */
