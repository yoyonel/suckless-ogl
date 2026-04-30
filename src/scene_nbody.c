#include "app_settings.h"
#include "billboard_rendering.h"
#include "billboard_sorting.h"
#include "instanced_rendering.h"
#include "nbody.h"
#include "platform/platform_utils.h"
#include "profiler.h"
#include "scene.h"
#include "scene_internal.h"
#include "scene_simulation.h"
#include "shockwave.h"
#include "trail_renderer.h"
#include "utils.h"

void scene_toggle_nbody(Scene* scene)
{
	scene->simulation->nbody_mode = !scene->simulation->nbody_mode;

	if (scene->simulation->nbody_mode) {
		/* Initialize simulation and trails */
		nbody_init_preset(&scene->simulation->nbody_sim);

		int count = nbody_get_count(&scene->simulation->nbody_sim);
		if (!trail_renderer_init(&scene->visuals.trail_renderer,
		                         count)) {
			scene->simulation->nbody_mode = 0;
			return;
		}

		if (!shockwave_renderer_init(
		        &scene->visuals.shockwave_renderer)) {
			trail_renderer_cleanup(&scene->visuals.trail_renderer);
			scene->simulation->nbody_mode = 0;
			return;
		}

		/* Set trail colors from body albedos (HDR-scaled) */
		for (int i = 0; i < count; i++) {
			trail_renderer_set_color(
			    &scene->visuals.trail_renderer, i,
			    scene->simulation->nbody_sim.bodies[i].albedo);
		}

		/* Write initial SphereInstance data and upload to GPU */
		SphereInstance instances[NBODY_MAX_BODIES];
		nbody_write_instances(&scene->simulation->nbody_sim, instances);
		instanced_group_update(&scene->instanced_group, instances,
		                       count);

#ifdef USE_TRANSPARENT_BILLBOARDS
		if (scene->billboard_instances) {
			safe_memcpy(scene->billboard_instances,
			            sizeof(SphereInstance) * (size_t)count,
			            instances,
			            sizeof(SphereInstance) * (size_t)count);
			scene->billboard_instance_count = count;
		}
		scene->billboard_group.instance_count = count;
#endif
	} else {
		/* Restore original material grid — clean up before re-init
		 * to avoid leaking GPU buffers and CPU allocations */
		trail_renderer_cleanup(&scene->visuals.trail_renderer);
		shockwave_renderer_cleanup(&scene->visuals.shockwave_renderer);
#ifdef USE_TRANSPARENT_BILLBOARDS
		if (scene->billboard_instances) {
			platform_aligned_free(scene->billboard_instances);
			scene->billboard_instances = NULL;
		}
		billboard_sorter_cleanup(&scene->billboard_sorter);
#endif
		instanced_group_cleanup(&scene->instanced_group);
		billboard_group_cleanup(&scene->billboard_group);
		scene_init_instancing(scene);
	}
}

void scene_nbody_update(Scene* scene, float delta_time)
{
	if (!scene->simulation->nbody_mode) {
		return;
	}

	/* Smooth time-scale transition (decelerate → pause → reverse) */
	nbody_update_time_scale(&scene->simulation->nbody_sim, delta_time);

	/* Advance physics (Velocity Verlet, O(N²) gravity) */
	{
		PROFILE_ZONE(verlet_ctx, "NBody Verlet");
		nbody_step(&scene->simulation->nbody_sim, delta_time);
		PROFILE_ZONE_END(verlet_ctx);
	}

	/* Forward confinement impacts to shockwave VFX */
	for (int i = 1; i < scene->simulation->nbody_sim.body_count; i++) {
		const NBodyImpact* imp =
		    &scene->simulation->nbody_sim.impacts[i];
		if (imp->active) {
			shockwave_emit(&scene->visuals.shockwave_renderer,
			               imp->position, imp->color, imp->velocity,
			               scene->simulation->nbody_sim.sim_time);
		}
	}
	shockwave_update(&scene->visuals.shockwave_renderer,
	                 scene->simulation->nbody_sim.sim_time);

	/* Record trail positions into ring buffers */
	{
		PROFILE_ZONE(trail_ctx, "NBody Trail Sample");
		trail_renderer_record(&scene->visuals.trail_renderer,
		                      &scene->simulation->nbody_sim,
		                      delta_time);
		PROFILE_ZONE_END(trail_ctx);
	}

	/* Build SphereInstance data and upload to GPU */
	int count = nbody_get_count(&scene->simulation->nbody_sim);
	SphereInstance instances[NBODY_MAX_BODIES];
	{
		PROFILE_ZONE(inst_ctx, "NBody Instance Build");
		nbody_write_instances(&scene->simulation->nbody_sim, instances);
		PROFILE_ZONE_END(inst_ctx);
	}
	{
		PROFILE_ZONE(upload_ctx, "NBody VBO Upload");
		instanced_group_update(&scene->instanced_group, instances,
		                       count);
		PROFILE_ZONE_END(upload_ctx);
	}

#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->billboard_instances) {
		safe_memcpy(scene->billboard_instances,
		            sizeof(SphereInstance) * (size_t)count, instances,
		            sizeof(SphereInstance) * (size_t)count);
		scene->billboard_instance_count = count;
	}
	scene->billboard_group.instance_count = count;
#endif
}
