#include "fps.h"

#include "log.h"
#include <stdbool.h>

static const float MILLISECONDS = 1000.0F;

void fps_init(FpsCounter* fps, float decay, double log_interval)
{
	fps->average_frame_time = 0.0;
	fps->decay_factor = decay;
	fps->last_log_time = 0.0;
	fps->log_interval = log_interval;
}

void fps_update(FpsCounter* fps, double delta_time, double current_time)
{
	static const bool log_fps = false;

	/* Calcul de la moyenne mobile exponentielle (identique au script
	 * Python) */
	if (fps->average_frame_time == 0.0) {
		fps->average_frame_time = delta_time;
	} else {
		fps->average_frame_time =
		    (fps->average_frame_time * fps->decay_factor) +
		    (delta_time * (1.0F - fps->decay_factor));
	}

	/* Affichage dans les logs toutes les X secondes */
	if (log_fps && current_time - fps->last_log_time >= fps->log_interval) {
		double fps_value = (fps->average_frame_time > 0)
		                       ? (1.0 / fps->average_frame_time)
		                       : 0;
		log_message(LOG_LEVEL_INFO, "FPS",
		            "Avg Frame Time: %.3f ms | FPS: %.1f",
		            fps->average_frame_time * MILLISECONDS, fps_value);
		fps->last_log_time = current_time;
	}
}