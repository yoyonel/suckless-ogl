#include "dvec3.h"
#include "nbody.h"
#include "nbody_internal.h"
#include "nbody_types.h"
#include "utils.h"
#include <cglm/vec3.h>
#include <math.h>

static const double CENTRAL_STAR_MASS = 100.0;
static const float CENTRAL_STAR_RADIUS = 1.5F;
static const float CENTRAL_STAR_ALBEDO[3] = {1.0F, 0.76F, 0.34F};
static const float CENTRAL_STAR_METALLIC = 1.0F;
static const float CENTRAL_STAR_ROUGHNESS = 0.2F;

struct OrbiterDef {
	float pos[3];
	float vel_dir[3];
	float orbit_r;
	float body_r;
	float mass;
	float albedo[3];
	float metallic;
	float roughness;
	float ecc;
};

/* clang-format off */
static const struct OrbiterDef ORBITERS[] = {
	/* 1  */ {{ 3, 0, 0},      { 0, 1, 0},     3,   0.30F, 0.3F, {1.0F,0.84F,0.0F},  1.0F, 0.10F, 1.00F},
	/* 2  */ {{ 0, 5, 0.5F},   {-1, 0, 0},     5,   0.50F, 1.5F, {0.77F,0.78F,0.78F},1.0F, 0.05F, 1.00F},
	/* 3  */ {{ 0,-1, 7},      { 1, 0, 0},     7,   0.60F, 2.0F, {0.85F,0.85F,0.90F},1.0F, 0.15F, 1.00F},
	/* 4  */ {{-10, 0, 1.5F},  { 0,-1, 0},    10,   0.80F, 4.0F, {0.72F,0.45F,0.20F},1.0F, 0.30F, 1.00F},
	/* 5  */ {{ 8, 2, 2},      { 0, 1,-0.3F},  8,   0.40F, 0.5F, {0.15F,0.85F,0.95F},0.1F, 0.05F, 0.85F},
	/* 6  */ {{-3,11,-2},      { 0.5F, 0, 0.6F},11,  0.35F, 0.5F, {0.90F,0.20F,0.75F},0.1F, 0.05F, 0.85F},
	/* 7  */ {{ 4, 0.5F, 0},   { 0, 0, 1},     4,   0.35F, 0.6F, {0.15F,0.85F,0.30F},0.8F, 0.15F, 1.00F},
	/* 8  */ {{ 0,-10, 2},     { 1, 0, 0},    10,   0.55F, 1.8F, {0.90F,0.55F,0.65F},0.3F, 0.25F, 0.95F},
	/* 9  */ {{ 0, 0,-3.5F},   { 0, 1, 0},     3.5F,0.30F, 0.4F, {0.10F,0.25F,0.95F},0.9F, 0.10F, 1.00F},
	/* 10 */ {{ 9,-2, 0},      { 0, 1, 0},     9,   0.65F, 3.0F, {1.0F,0.60F,0.10F}, 0.7F, 0.30F, 1.00F},
	/* 11 */ {{11, 2,-3},      {-0.4F, 0, 0.7F},11,  0.30F, 0.4F, {0.55F,0.15F,0.95F},0.2F, 0.08F, 0.92F},
	/* 12 */ {{-6, 0,-1},      { 0, 1, 0},     6,   0.45F, 1.2F, {0.20F,0.80F,0.75F},0.6F, 0.12F, 1.00F},
	/* 13 */ {{-12,-2, 1},     { 0.3F, 0.7F, 0},12,  0.50F, 2.5F, {0.90F,0.12F,0.15F},0.85F,0.35F, 0.95F},
};
/* clang-format on */

static const int ORBITER_COUNT = (int)(sizeof(ORBITERS) / sizeof(ORBITERS[0]));

static void add_body(NBodySim* sim, const double pos[3], const double vel[3],
                     double mass, float radius, const float albedo[3],
                     float metallic, float roughness)
{
	if (sim->body_count >= NBODY_MAX_BODIES) {
		return;
	}

	NBodyParticle* body = &sim->bodies[sim->body_count];
	dvec3_copy(pos, body->position);
	dvec3_copy(vel, body->velocity);
	body->mass = mass;
	body->radius = radius;
	glm_vec3_copy((float*)albedo, body->albedo);
	body->metallic = metallic;
	body->roughness = roughness;
	sim->body_count++;
}

static double softened_orbital_vel(double grav, double central_mass,
                                   double orbit_r, double star_r, double body_r)
{
	double eps_sq = pair_softening_sq(star_r, body_r);
	double r_sq = orbit_r * orbit_r;
	double d_sq = r_sq + eps_sq;
	double d_32 = d_sq * sqrt(d_sq);
	return orbit_r * sqrt(grav * central_mass / d_32);
}

void nbody_init_preset(NBodySim* sim)
{
	(void)safe_memset(sim, sizeof(*sim), 0, sizeof(*sim));
	sim->gravity = NBODY_DEFAULT_G;
	sim->time_scale = 1.0F;
	sim->target_time_scale = 1.0F;
	sim->paused = false;

	add_body(sim, (double[]){0, 0, 0}, (double[]){0, 0, 0},
	         CENTRAL_STAR_MASS, CENTRAL_STAR_RADIUS, CENTRAL_STAR_ALBEDO,
	         CENTRAL_STAR_METALLIC, CENTRAL_STAR_ROUGHNESS);

	for (int idx = 0; idx < ORBITER_COUNT; idx++) {
		const struct OrbiterDef* orb = &ORBITERS[idx];
		double spd = softened_orbital_vel((double)sim->gravity,
		                                  CENTRAL_STAR_MASS,
		                                  (double)orb->orbit_r,
		                                  (double)CENTRAL_STAR_RADIUS,
		                                  (double)orb->body_r) *
		             (double)orb->ecc;

		double normalized_vel_dir[3] = {(double)orb->vel_dir[0],
		                                (double)orb->vel_dir[1],
		                                (double)orb->vel_dir[2]};
		dvec3_normalize(normalized_vel_dir);

		dvec3 vel;
		dvec3_scale(normalized_vel_dir, spd, vel);
		add_body(sim,
		         (double[]){(double)orb->pos[0], (double)orb->pos[1],
		                    (double)orb->pos[2]},
		         vel, (double)orb->mass, orb->body_r, orb->albedo,
		         orb->metallic, orb->roughness);
	}

	dvec3 total_momentum = {0.0, 0.0, 0.0};
	double total_mass = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		dvec3 mom;
		dvec3_scale(sim->bodies[i].velocity, sim->bodies[i].mass, mom);
		dvec3_addto(total_momentum, mom);
		total_mass += sim->bodies[i].mass;
	}

	dvec3 vel_cm;
	dvec3_scale(total_momentum, 1.0 / total_mass, vel_cm);
	for (int i = 0; i < sim->body_count; i++) {
		dvec3_subfrom(sim->bodies[i].velocity, vel_cm);
	}

	sim->initial_energy = nbody_total_energy(sim);

	for (int i = 0; i < sim->body_count; i++) {
		dvec3_copy(sim->bodies[i].position,
		           sim->bodies[i].prev_position);
	}
}
