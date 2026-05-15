#include "lum_histogram.h"

#include "app.h"
#include "app_settings.h"
#include <stdlib.h>

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
int lum_histogram_subsys_init(App* app)
{
	app->lum_histogram_buffer =
	    malloc((size_t)(LUM_HISTOGRAM_MAP_SIZE * LUM_HISTOGRAM_MAP_SIZE) *
	           sizeof(float));
	return app->lum_histogram_buffer != NULL;
}

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
void lum_histogram_subsys_cleanup(App* app)
{
	free(app->lum_histogram_buffer);
	app->lum_histogram_buffer = NULL;
}
