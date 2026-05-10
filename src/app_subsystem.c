#include "app_subsystem.h"

#include "app.h"
#include "log.h"

static const char* const LOG_TAG = "suckless-ogl.subsystem";

int app_subsystems_init(struct App* app, const SubsystemDescriptor* table)
{
	for (int i = 0; table[i].name; i++) {
		if (table[i].init && !table[i].init(app)) {
			LOG_ERROR(LOG_TAG, "init failed: %s (index %d)",
			          table[i].name, i);
			for (int j = i - 1; j >= 0; j--) {
				if (table[j].cleanup) {
					table[j].cleanup(app);
				}
			}
			return 0;
		}
	}
	return 1;
}

void app_subsystems_cleanup(struct App* app, const SubsystemDescriptor* table)
{
	int count = 0;
	while (table[count].name) {
		count++;
	}
	for (int i = count - 1; i >= 0; i--) {
		if (table[i].cleanup) {
			table[i].cleanup(app);
		}
	}
}
