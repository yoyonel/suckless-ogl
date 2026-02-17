/**
 * @file perf_mode.h
 * @brief Runtime performance optimization module.
 *
 * Provides a unified interface for activating system-level performance
 * optimizations similar to Windows "Game Mode". Uses libgamemode when
 * available, with native Linux scheduler fallback.
 *
 * @section Usage
 * @code
 * perf_mode_init();
 * if (perf_mode_request_start() == 0) {
 *     // Performance mode active
 * }
 * // ... run application ...
 * perf_mode_request_end();
 * perf_mode_cleanup();
 * @endcode
 */

#ifndef PERF_MODE_H
#define PERF_MODE_H

#include <sched.h>

/**
 * @enum PerfModeState
 * @brief Current state of the performance mode subsystem.
 */
typedef enum {
	PERF_MODE_OFF = 0,      /**< No optimizations active. */
	PERF_MODE_GAMEMODE,     /**< Using libgamemode/gamemoded. */
	PERF_MODE_NATIVE_SCHED, /**< Using native sched_setscheduler. */
	PERF_MODE_NATIVE_NICE,  /**< Using native nice priority. */
	PERF_MODE_ERROR         /**< Activation failed. */
} PerfModeState;

/**
 * @enum PerfModeBackend
 * @brief Available performance optimization backends.
 */
typedef enum {
	PERF_BACKEND_NONE = 0, /**< No backend available. */
	PERF_BACKEND_GAMEMODE, /**< libgamemode detected. */
	PERF_BACKEND_NATIVE    /**< Native Linux syscalls. */
} PerfModeBackend;

/**
 * @struct PerfModeContext
 * @brief State container for performance mode.
 */
typedef struct {
	PerfModeState state;               /**< Current activity state. */
	PerfModeBackend backend;           /**< Selected backend. */
	int original_policy;               /**< Saved scheduler policy. */
	struct sched_param original_param; /**< Saved scheduler params. */
	int original_nice;                 /**< Saved process nice value. */
	int initialized;                   /**< Initialization flag. */
} PerfModeContext;

/**
 * @brief Initialize the performance mode subsystem.
 *
 * Detects available backends (GameMode daemon, native capabilities).
 * Should be called once at application startup.
 *
 * @param ctx Pointer to the context to initialize.
 * @return 0 on success, -1 on failure.
 */
int perf_mode_init(PerfModeContext* ctx);

/**
 * @brief Clean up performance mode resources.
 *
 * Ensures any active optimizations are properly reverted.
 * Should be called at application shutdown.
 *
 * @param ctx Pointer to the context.
 */
void perf_mode_cleanup(PerfModeContext* ctx);

/**
 * @brief Request activation of performance mode.
 *
 * Attempts to activate performance optimizations using the best
 * available backend. Effects may include:
 * - CPU governor set to "performance"
 * - GPU performance mode enabled
 * - Process scheduling priority elevated
 * - Background services deprioritized
 *
 * @param ctx Pointer to the context.
 * @return 0 on success, -1 on failure.
 *
 * @note For native SCHED_FIFO, requires CAP_SYS_NICE or root.
 *       GameMode handles permissions via polkit.
 */
int perf_mode_request_start(PerfModeContext* ctx);

/**
 * @brief Request deactivation of performance mode.
 *
 * Reverts any system changes made by perf_mode_request_start().
 *
 * @param ctx Pointer to the context.
 * @return 0 on success, -1 on failure.
 */
int perf_mode_request_end(PerfModeContext* ctx);

/**
 * @brief Get a human-readable string for the current state.
 *
 * @param ctx Pointer to the context.
 * @return Static string describing the current mode (e.g., "GameMode",
 *         "Native SCHED_FIFO", "Off").
 */
const char* perf_mode_get_state_string(const PerfModeContext* ctx);

#endif /* PERF_MODE_H */
