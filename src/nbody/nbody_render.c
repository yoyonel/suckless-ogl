#include "nbody.h"
#include "nbody_types.h"
#include "sphere_types.h"
#include <cglm/affine-pre.h>
#include <cglm/affine.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/vec3.h>

/* Note : SphereInstance est déjà connu ici via le nbody.h (qui inclut
 * sphere_types.h) */

void nbody_write_instances(const NBodySim* sim, SphereInstance* out)
{
	for (int i = 0; i < sim->body_count; i++) {
		const NBodyParticle* body = &sim->bodies[i];
		glm_mat4_identity(out[i].model);
		vec3 pos_f = {(float)body->position[0],
		              (float)body->position[1],
		              (float)body->position[2]};
		glm_translate(out[i].model, pos_f);

		vec3 scale = {body->radius, body->radius, body->radius};
		glm_scale(out[i].model, scale);

		glm_vec3_copy((float*)body->albedo, out[i].albedo);
		out[i].metallic = body->metallic;
		out[i].roughness = body->roughness;
		out[i].ao = 1.0F;

		out[i].prev_center[0] = (float)body->prev_position[0];
		out[i].prev_center[1] = (float)body->prev_position[1];
		out[i].prev_center[2] = (float)body->prev_position[2];
	}
}
