#include "gpu_usage.h"

#include "log.h"
#include "utils.h"
#include <string.h>

#ifdef __linux__
#include "platform/platform_time.h"
#include <dirent.h>
#include <errno.h>

/* --- Constants --- */
enum {
	FDINFO_LINE_MAX = 256,
	FDINFO_PATH_MAX = 128,
	CLIENT_ID_MAX = 64,
	STRTOUL_BASE_DEC = 10,
};

static const float GPU_LOAD_CLAMP_MAX = 100.0F;

/* MangoHud-ISO: METRICS_UPDATE_PERIOD_MS = 500 */
static const uint64_t UPDATE_PERIOD_NS = 500000000ULL;

/**
 * @brief Determine the DRM engine key for a given driver.
 * @return Engine key string or NULL if unsupported.
 */
static const char* engine_key_for_driver(const char* driver)
{
	if (strcmp(driver, "i915") == 0) {
		return "drm-engine-render";
	}
	if (strcmp(driver, "xe") == 0) {
		return "drm-engine-render";
	}
	if (strcmp(driver, "amdgpu") == 0) {
		return "drm-engine-gfx";
	}
	if (strcmp(driver, "nouveau") == 0) {
		return "drm-engine-gr";
	}
	return NULL;
}

/**
 * @brief Parse a single fdinfo file to extract driver, client-id, and engine
 * time.
 */
static bool parse_fdinfo(FILE* file_ptr, char* out_driver, size_t driver_sz,
                         char* out_client_id, size_t client_sz,
                         uint64_t* out_engine_ns, const char* engine_key)
{
	char line[FDINFO_LINE_MAX];
	bool found_engine = false;

	out_driver[0] = '\0';
	out_client_id[0] = '\0';
	*out_engine_ns = 0;

	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		return false;
	}
	errno = 0;

	while (fgets(line, sizeof(line), file_ptr) != NULL) {
		/* Skip lines starting with whitespace */
		if (line[0] == ' ' || line[0] == '\t') {
			continue;
		}

		char* colon = strchr(line, ':');
		if (!colon || colon[1] == '\0') {
			continue;
		}

		/* Split key:value */
		*colon = '\0';
		const char* key = line;
		const char* val = colon + 1;

		/* Skip leading whitespace in value */
		while (*val == ' ' || *val == '\t') {
			val++;
		}

		/* Strip trailing newline */
		size_t vlen = strlen(val);
		char val_buf[FDINFO_LINE_MAX];
		safe_strncpy(val_buf, sizeof(val_buf), val, vlen);
		vlen = strlen(val_buf);
		while (vlen > 0 && (val_buf[vlen - 1] == '\n' ||
		                    val_buf[vlen - 1] == '\r')) {
			val_buf[--vlen] = '\0';
		}

		if (strcmp(key, "drm-driver") == 0) {
			safe_strncpy(out_driver, driver_sz, val_buf,
			             driver_sz - 1);
		} else if (strcmp(key, "drm-client-id") == 0) {
			safe_strncpy(out_client_id, client_sz, val_buf,
			             client_sz - 1);
		} else if (engine_key && strcmp(key, engine_key) == 0) {
			/* Value format: "12345 ns" or "12345" */
			*out_engine_ns =
			    strtoull(val_buf, NULL, STRTOUL_BASE_DEC);
			found_engine = true;
		}
	}

	return found_engine;
}

/**
 * @brief Check whether a client-id has already been seen.
 */
static bool is_duplicate_client(char seen_clients[][CLIENT_ID_MAX],
                                int seen_count, const char* client_id)
{
	for (int idx = 0; idx < seen_count; idx++) {
		if (strcmp(seen_clients[idx], client_id) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * @brief Try to register one fdinfo entry as a GPU monitor stream.
 * @return true if the stream was consumed (kept open or closed), false on
 * error.
 */
static bool try_register_fdinfo(GPUUsageMonitor* mon, const char* path,
                                char seen_clients[][CLIENT_ID_MAX],
                                int* seen_count, char* detected_driver,
                                size_t driver_sz, const char** out_engine_key)
{
	FILE* file_ptr = fopen(path, "r");
	if (!file_ptr) {
		return false;
	}

	char driver[GPU_USAGE_MAX_KEY_LEN] = {0};
	char client_id[CLIENT_ID_MAX] = {0};
	uint64_t engine_ns = 0;

	(void)parse_fdinfo(file_ptr, driver, sizeof(driver), client_id,
	                   sizeof(client_id), &engine_ns, NULL);

	if (driver[0] == '\0') {
		(void)fclose(file_ptr);
		return false;
	}

	const char* eng_key = engine_key_for_driver(driver);
	if (!eng_key) {
		(void)fclose(file_ptr);
		return false;
	}

	if (is_duplicate_client(seen_clients, *seen_count, client_id)) {
		(void)fclose(file_ptr);
		return false;
	}

	if (*seen_count < GPU_USAGE_MAX_FDS) {
		safe_strncpy(seen_clients[*seen_count], sizeof(seen_clients[0]),
		             client_id, sizeof(seen_clients[0]) - 1);
		(*seen_count)++;
	}

	(void)parse_fdinfo(file_ptr, driver, sizeof(driver), client_id,
	                   sizeof(client_id), &engine_ns, eng_key);

	if (mon->stream_count < GPU_USAGE_MAX_FDS) {
		mon->streams[mon->stream_count++] = file_ptr;
		safe_strncpy(detected_driver, driver_sz, driver, driver_sz - 1);
		*out_engine_key = eng_key;
	} else {
		(void)fclose(file_ptr);
	}

	return true;
}

void gpu_usage_init(GPUUsageMonitor* mon)
{
	if (!mon) {
		return;
	}

	*mon = (GPUUsageMonitor){0};
	mon->available = false;

	DIR* dir = opendir("/proc/self/fdinfo");
	if (!dir) {
		LOG_WARN("gpu_usage", "Cannot open /proc/self/fdinfo");
		return;
	}

	char seen_clients[GPU_USAGE_MAX_FDS][CLIENT_ID_MAX];
	int seen_count = 0;
	char detected_driver[GPU_USAGE_MAX_KEY_LEN] = {0};
	const char* engine_key = NULL;

	struct dirent* entry = NULL;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') {
			continue;
		}

		char path[FDINFO_PATH_MAX];
		(void)safe_snprintf(path, sizeof(path), "/proc/self/fdinfo/%s",
		                    entry->d_name);

		(void)try_register_fdinfo(mon, path, seen_clients, &seen_count,
		                          detected_driver,
		                          sizeof(detected_driver), &engine_key);
	}

	(void)closedir(dir);

	if (mon->stream_count > 0 && engine_key != NULL) {
		safe_strncpy(mon->driver, sizeof(mon->driver), detected_driver,
		             sizeof(mon->driver) - 1);
		safe_strncpy(mon->engine_key, sizeof(mon->engine_key),
		             engine_key, strlen(engine_key));
		mon->prev_wall_time_ns = platform_get_time_ns();
		mon->prev_gpu_time_ns = 0;
		mon->available = true;

		LOG_INFO("gpu_usage",
		         "Detected driver \"%s\" with %d unique fd(s), "
		         "engine key: %s",
		         mon->driver, mon->stream_count, mon->engine_key);
	} else {
		LOG_WARN("gpu_usage",
		         "No supported DRM driver found in fdinfo");
	}
}

void gpu_usage_cleanup(GPUUsageMonitor* mon)
{
	if (!mon) {
		return;
	}

	for (int i = 0; i < mon->stream_count; i++) {
		if (mon->streams[i]) {
			(void)fclose(mon->streams[i]);
			mon->streams[i] = NULL;
		}
	}
	mon->stream_count = 0;
	mon->available = false;
}

void gpu_usage_update(GPUUsageMonitor* mon)
{
	if (!mon || !mon->available || mon->stream_count == 0) {
		return;
	}

	/* MangoHud-ISO: only update every METRICS_UPDATE_PERIOD_MS (500ms) */
	uint64_t now = platform_get_time_ns();
	if (mon->prev_wall_time_ns > 0 &&
	    (now - mon->prev_wall_time_ns) < UPDATE_PERIOD_NS) {
		return;
	}

	uint64_t total_engine_ns = 0;

	for (int i = 0; i < mon->stream_count; i++) {
		FILE* file_ptr = mon->streams[i];
		if (!file_ptr) {
			continue;
		}

		char driver[GPU_USAGE_MAX_KEY_LEN];
		char client_id[CLIENT_ID_MAX];
		uint64_t engine_ns = 0;

		(void)parse_fdinfo(file_ptr, driver, sizeof(driver), client_id,
		                   sizeof(client_id), &engine_ns,
		                   mon->engine_key);

		total_engine_ns += engine_ns;
	}

	/* First update: just record baseline, no computation */
	if (mon->prev_gpu_time_ns == 0) {
		mon->prev_gpu_time_ns = total_engine_ns;
		mon->prev_wall_time_ns = now;
		return;
	}

	uint64_t delta_wall = now - mon->prev_wall_time_ns;
	uint64_t delta_gpu = total_engine_ns - mon->prev_gpu_time_ns;

	if (delta_wall > 0) {
		float load =
		    (float)delta_gpu / (float)delta_wall * GPU_LOAD_CLAMP_MAX;
		if (load > GPU_LOAD_CLAMP_MAX) {
			load = GPU_LOAD_CLAMP_MAX;
		}
		if (load < 0.0F) {
			load = 0.0F;
		}
		mon->load_percent = load;
	}

	mon->prev_gpu_time_ns = total_engine_ns;
	mon->prev_wall_time_ns = now;
}

#else /* !__linux__ */

void gpu_usage_init(GPUUsageMonitor* mon)
{
	if (!mon) {
		return;
	}
	*mon = (GPUUsageMonitor){0};
	mon->available = false;
	LOG_WARN("gpu_usage", "DRM fdinfo not available on this platform");
}

void gpu_usage_cleanup(GPUUsageMonitor* mon)
{
	if (!mon) {
		return;
	}
	mon->stream_count = 0;
	mon->available = false;
}

void gpu_usage_update(GPUUsageMonitor* mon)
{
	(void)mon;
}

#endif /* __linux__ */

float gpu_usage_get_load(const GPUUsageMonitor* mon)
{
	if (!mon || !mon->available) {
		return -1.0F;
	}
	return mon->load_percent;
}

bool gpu_usage_is_available(const GPUUsageMonitor* mon)
{
	if (mon == NULL) {
		return false;
	}
	return mon->available;
}
