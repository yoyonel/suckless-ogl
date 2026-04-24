#ifndef GPU_USAGE_H
#define GPU_USAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum {
	GPU_USAGE_MAX_FDS = 64,
	GPU_USAGE_MAX_KEY_LEN = 48,
};

/**
 * @struct GPUUsageMonitor
 * @brief Reads GPU utilization % via Linux DRM fdinfo (same method as
 * MangoHud).
 *
 * Scans /proc/self/fdinfo/ for DRM render node file descriptors,
 * reads cumulative engine time (drm-engine-render for i915, drm-engine-gfx
 * for amdgpu, etc.), and computes utilization as delta_gpu_time /
 * delta_wall_time.
 *
 * Supported drivers: i915, xe, amdgpu, nouveau.
 */
typedef struct {
	FILE* streams[GPU_USAGE_MAX_FDS];
	int stream_count;

	uint64_t prev_gpu_time_ns;
	uint64_t prev_wall_time_ns;

	float load_percent;
	char driver[GPU_USAGE_MAX_KEY_LEN];
	char engine_key[GPU_USAGE_MAX_KEY_LEN];

	bool available;
} GPUUsageMonitor;

/**
 * @brief Scans /proc/self/fdinfo/ and opens streams for GPU engine time
 * reading.
 */
void gpu_usage_init(GPUUsageMonitor* mon);

/**
 * @brief Closes all open fdinfo streams.
 */
void gpu_usage_cleanup(GPUUsageMonitor* mon);

/**
 * @brief Re-reads fdinfo streams and computes GPU load %.
 * Should be called periodically (e.g., every 500ms).
 */
void gpu_usage_update(GPUUsageMonitor* mon);

/**
 * @brief Returns the last computed GPU load (0.0–100.0).
 */
float gpu_usage_get_load(const GPUUsageMonitor* mon);

/**
 * @brief Returns true if a supported DRM driver was detected.
 */
bool gpu_usage_is_available(const GPUUsageMonitor* mon);

#endif /* GPU_USAGE_H */
