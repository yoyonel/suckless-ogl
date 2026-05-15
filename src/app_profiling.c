#include <glad/glad.h>

#include "app_profiling.h"

#include "app.h"
#include "app_settings.h"
#include "platform/platform_utils.h"

void app_profiling_init(AppProfiling* prof, int width, int height)
{
	tracy_manager_init(&prof->tracy_mgr, width, height);
	fps_init(&prof->fps_counter, DEFAULT_FPS_SMOOTHING, DEFAULT_FPS_WINDOW);
	perf_mode_init(&prof->perf_context);
	gpu_profiler_init(&prof->gpu_profiler);
	gpu_profiler_ui_init(&prof->timeline_ui);
	gpu_usage_init(&prof->gpu_usage);
	prof->perf_mode_active = false;
	prof->log_gpu_metrics = false;
}

void app_profiling_cleanup(AppProfiling* prof)
{
	perf_mode_cleanup(&prof->perf_context);
	gpu_profiler_cleanup(&prof->gpu_profiler);
	gpu_profiler_ui_cleanup(&prof->timeline_ui);
	gpu_usage_cleanup(&prof->gpu_usage);
	tracy_manager_cleanup(&prof->tracy_mgr);
}

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
int app_profiling_subsys_init(App* app)
{
	app->profiling =
	    platform_aligned_alloc(sizeof(*app->profiling), SIMD_ALIGNMENT);
	if (!app->profiling) {
		return 0;
	}
	*app->profiling = (AppProfiling){0};
	app_profiling_init(app->profiling, app->width, app->height);
	return 1;
}

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
void app_profiling_subsys_cleanup(App* app)
{
	if (app->profiling) {
		app_profiling_cleanup(app->profiling);
		platform_aligned_free(app->profiling);
		app->profiling = NULL;
	}
}
