#ifndef LUM_HISTOGRAM_H
#define LUM_HISTOGRAM_H

#include "app_subsystem.h"

struct App;

int lum_histogram_subsys_init(struct App* app);
void lum_histogram_subsys_cleanup(struct App* app);

#define APP_LUM_HISTOGRAM_DESCRIPTOR                 \
	{"lum_histogram", lum_histogram_subsys_init, \
	 lum_histogram_subsys_cleanup}

#endif /* LUM_HISTOGRAM_H */
