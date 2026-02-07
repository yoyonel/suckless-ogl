#ifndef APP_METRICS_H
#define APP_METRICS_H

#include "gpu_profiler.h"

/**
 * @brief Logs GPU performance statistics if the reporting window for any stage
 * has elapsed.
 *
 * This function handles the logic of checking sampling windows, calculating
 * missed frames, formatting the output string, and logging the results via
 * the logging system. It also resets samplers after reporting.
 *
 * @param profiler Pointer to the GPUProfiler containing the stages and
 * samplers.
 * @param current_time The current application time (e.g. from glfwGetTime()).
 * @return true if logs were printed (2s interval elapsed), false otherwise.
 */
bool app_metrics_log_gpu_stats(GPUProfiler* profiler, double current_time,
                               bool should_log);

#endif /* APP_METRICS_H */
