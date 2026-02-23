/**
 * @file perf_mode.c
 * @brief Runtime performance optimization implementation.
 *
 * Implements performance mode with two backends:
 * 1. libgamemode - Preferred when gamemoded is running
 * 2. Native Linux syscalls - Fallback using sched_setscheduler/nice
 */

#include "perf_mode.h"

#include "log.h"
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef TRACY_ENABLE
#include <tracy/TracyC.h>
#endif

/* Optional GameMode integration */
#ifdef HAVE_GAMEMODE
#include <gamemode_client.h>
#endif

/** @brief Default real-time priority for SCHED_FIFO (1-99). */
enum { PERF_RT_PRIORITY = 50 };

/** @brief Nice value for elevated priority (-20 to 19). */
enum { PERF_NICE_VALUE = -10 };

/**
 * @brief Detect if GameMode daemon is available.
 * @return 1 if available, 0 otherwise.
 */
static int detect_gamemode(void)
{
#ifdef HAVE_GAMEMODE
	/* Query GameMode status - returns -1 if daemon not running */
	int status = gamemode_query_status();
	if (status >= 0) {
		LOG_INFO("suckless-ogl.perf",
		         "GameMode daemon detected (status: %d)", status);
		return 1;
	}
	const char* err = gamemode_error_string();
	LOG_DEBUG("suckless-ogl.perf", "GameMode not available: %s",
	          (err && *err) ? err : "Daemon not running or unreachable");
#endif
	return 0;
}

/**
 * @brief Detect if native scheduling capabilities are available.
 * @return 1 if SCHED_FIFO possible, 2 if only nice, 0 otherwise.
 */
static int detect_native_capabilities(void)
{
	/* Check if we have CAP_SYS_NICE or are root */
	if (geteuid() == 0) {
		LOG_INFO("suckless-ogl.perf",
		         "Running as root - native SCHED_FIFO available");
		return 1;
	}

	/* Try to query max schedulable priority */
	int max_prio = sched_get_priority_max(SCHED_FIFO);
	if (max_prio > 0) {
		/* We can at least query RT priorities, might have CAP_SYS_NICE
		 */
		LOG_INFO("suckless-ogl.perf",
		         "Native scheduling available (max RT priority: %d)",
		         max_prio);
		return 1;
	}

	/* Fallback: nice should always work for lowering priority,
	 * but raising requires privileges */
	LOG_INFO("suckless-ogl.perf",
	         "Native nice available (may need privileges for effect)");
	return 2;
}

int perf_mode_init(PerfModeContext* ctx)
{
	if (!ctx) {
		return -1;
	}
	if (ctx->initialized) {
		return 0;
	}

	/* Initialize default state */
	ctx->state = PERF_MODE_OFF;
	ctx->backend = PERF_BACKEND_NONE;
	ctx->original_policy = SCHED_OTHER;
	ctx->original_param.sched_priority = 0;
	ctx->original_nice = 0;

	/* Save original scheduling state */
	ctx->original_policy = sched_getscheduler(0);
	if (sched_getparam(0, &ctx->original_param) != 0) {
		ctx->original_param.sched_priority = 0;
	}
	errno = 0;
	ctx->original_nice = getpriority(PRIO_PROCESS, 0);
	if (errno != 0) {
		ctx->original_nice = 0;
	}

	/* Detect available backends */
	if (detect_gamemode()) {
		ctx->backend = PERF_BACKEND_GAMEMODE;
	} else if (detect_native_capabilities() > 0) {
		ctx->backend = PERF_BACKEND_NATIVE;
	} else {
		ctx->backend = PERF_BACKEND_NONE;
		LOG_WARN("suckless-ogl.perf",
		         "No performance optimization backend available");
	}

	ctx->initialized = 1;
	LOG_INFO("suckless-ogl.perf",
	         "Performance mode initialized (backend: %s)",
	         ctx->backend == PERF_BACKEND_GAMEMODE ? "GameMode"
	         : ctx->backend == PERF_BACKEND_NATIVE ? "Native"
	                                               : "None");
	return 0;
}

/**
 * @brief Activate using GameMode backend.
 * @param ctx Context ptr.
 * @return 0 on success, -1 on failure.
 */
static int activate_gamemode(PerfModeContext* ctx)
{
#ifdef HAVE_GAMEMODE
	int status = gamemode_query_status();

	/* If already active and registered for this process (status 2),
	 * or just generally active (status 1), we consider it a success.
	 * This happens when the app is launched via 'gamemoderun'. */
	if (status > 0) {
#ifdef TRACY_ENABLE
		TracyCZoneN(ctx_zone, "GameMode Activate (Already Active)", 1);
		TracyCZoneEnd(ctx_zone);
#endif
		ctx->state = PERF_MODE_GAMEMODE;
		LOG_INFO("suckless-ogl.perf",
		         "GameMode already active (status: %d)", status);
		return 0;
	}

#ifdef TRACY_ENABLE
	TracyCZoneN(req_zone, "GameMode Request Start (D-Bus Call)", 1);
#endif
	if (gamemode_request_start() == 0) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(req_zone);
#endif
		ctx->state = PERF_MODE_GAMEMODE;
		LOG_INFO("suckless-ogl.perf", "GameMode activated (status: %d)",
		         gamemode_query_status());
		return 0;
	}
#ifdef TRACY_ENABLE
	TracyCZoneEnd(req_zone);
#endif

	const char* err = gamemode_error_string();
	LOG_WARN("suckless-ogl.perf",
	         "GameMode activation failed (current status: %d): [%s]",
	         status,
	         (err && *err) ? err : "Empty error string from libgamemode");
#endif
	(void)ctx;
	return -1;
}

/**
 * @brief Deactivate GameMode backend.
 * @param ctx Context ptr.
 * @return 0 on success, -1 on failure.
 */
static int deactivate_gamemode(PerfModeContext* ctx)
{
#ifdef HAVE_GAMEMODE
	if (gamemode_request_end() == 0) {
		ctx->state = PERF_MODE_OFF;
		LOG_INFO("suckless-ogl.perf", "GameMode deactivated");
		return 0;
	}
	LOG_WARN("suckless-ogl.perf", "GameMode deactivation failed: %s",
	         gamemode_error_string());
#endif
	(void)ctx;
	return -1;
}

/**
 * @brief Activate using native Linux scheduler.
 * @param ctx Context ptr.
 * @return 0 on success, -1 on failure.
 */
static int activate_native(PerfModeContext* ctx)
{
	struct sched_param param = {// NOLINT(misc-include-cleaner)
	                            .sched_priority = PERF_RT_PRIORITY};

	/* Try SCHED_FIFO first (real-time) */
	if (sched_setscheduler(0, SCHED_FIFO, &param) == 0) {
		ctx->state = PERF_MODE_NATIVE_SCHED;
		LOG_INFO("suckless-ogl.perf",
		         "Native SCHED_FIFO activated (priority: %d)",
		         PERF_RT_PRIORITY);
		return 0;
	}

	LOG_DEBUG("suckless-ogl.perf", "SCHED_FIFO failed: %s - trying nice",
	          strerror(errno));

	/* Fallback to nice */
	if (setpriority(PRIO_PROCESS, 0, PERF_NICE_VALUE) == 0) {
		ctx->state = PERF_MODE_NATIVE_NICE;
		LOG_INFO("suckless-ogl.perf",
		         "Native nice activated (priority: %d)",
		         PERF_NICE_VALUE);
		return 0;
	}

	LOG_WARN("suckless-ogl.perf",
	         "Native activation failed: %s (try sudo or CAP_SYS_NICE)",
	         strerror(errno));
	ctx->state = PERF_MODE_ERROR;
	return -1;
}

/**
 * @brief Deactivate native Linux scheduler.
 * @param ctx Context ptr.
 * @return 0 on success, -1 on failure.
 */
static int deactivate_native(PerfModeContext* ctx)
{
	int result = 0;

	if (ctx->state == PERF_MODE_NATIVE_SCHED) {
		/* Restore original scheduler */
		if (sched_setscheduler(0, ctx->original_policy,
		                       &ctx->original_param) != 0) {
			LOG_WARN("suckless-ogl.perf",
			         "Failed to restore scheduler: %s",
			         strerror(errno));
			result = -1;
		}
	} else if (ctx->state == PERF_MODE_NATIVE_NICE) {
		/* Restore original nice value */
		if (setpriority(PRIO_PROCESS, 0, ctx->original_nice) != 0) {
			LOG_WARN("suckless-ogl.perf",
			         "Failed to restore nice: %s", strerror(errno));
			result = -1;
		}
	}

	ctx->state = PERF_MODE_OFF;
	LOG_INFO("suckless-ogl.perf", "Native mode deactivated");
	return result;
}

int perf_mode_request_start(PerfModeContext* ctx)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(perf_zone, "Perf Mode Request Start", 1);
#endif
	if (!ctx || !ctx->initialized) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(perf_zone);
#endif
		LOG_WARN("suckless-ogl.perf",
		         "Performance mode not initialized");
		return -1;
	}

	if (ctx->state != PERF_MODE_OFF && ctx->state != PERF_MODE_ERROR) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(perf_zone);
#endif
		LOG_DEBUG("suckless-ogl.perf",
		          "Performance mode already active");
		return 0;
	}

	int result = -1;
	switch (ctx->backend) {
		case PERF_BACKEND_GAMEMODE:
			if (activate_gamemode(ctx) == 0) {
				result = 0;
				break;
			}
			/* Fall through to try native */
			/* fallthrough */
		case PERF_BACKEND_NATIVE:
			result = activate_native(ctx);
			break;
		case PERF_BACKEND_NONE:
		default:
			LOG_WARN("suckless-ogl.perf",
			         "No backend available for activation");
			result = -1;
			break;
	}

#ifdef TRACY_ENABLE
	TracyCZoneEnd(perf_zone);
#endif
	return result;
}

int perf_mode_request_end(PerfModeContext* ctx)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(perf_zone, "Perf Mode Request End", 1);
#endif
	if (!ctx || !ctx->initialized) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(perf_zone);
#endif
		return -1;
	}

	if (ctx->state == PERF_MODE_OFF) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(perf_zone);
#endif
		return 0;
	}

	int result = -1;
	switch (ctx->state) {
		case PERF_MODE_GAMEMODE:
			result = deactivate_gamemode(ctx);
			break;
		case PERF_MODE_NATIVE_SCHED:
		case PERF_MODE_NATIVE_NICE:
			result = deactivate_native(ctx);
			break;
		default:
			ctx->state = PERF_MODE_OFF;
			result = 0;
			break;
	}

#ifdef TRACY_ENABLE
	TracyCZoneEnd(perf_zone);
#endif
	return result;
}

void perf_mode_cleanup(PerfModeContext* ctx)
{
	if (!ctx || !ctx->initialized) {
		return;
	}

	/* Ensure we revert any active optimizations */
	if (ctx->state != PERF_MODE_OFF) {
		perf_mode_request_end(ctx);
	}

	ctx->initialized = 0;
	LOG_INFO("suckless-ogl.perf", "Performance mode cleaned up");
}

PerfModeState perf_mode_get_state(const PerfModeContext* ctx)
{
	return ctx ? ctx->state : PERF_MODE_OFF;
}

PerfModeBackend perf_mode_get_backend(const PerfModeContext* ctx)
{
	return ctx ? ctx->backend : PERF_BACKEND_NONE;
}

const char* perf_mode_get_state_string(const PerfModeContext* ctx)
{
	if (!ctx) {
		return "Unknown";
	}
	switch (ctx->state) {
		case PERF_MODE_OFF:
			return "Off";
		case PERF_MODE_GAMEMODE:
			return "GameMode";
		case PERF_MODE_NATIVE_SCHED:
			return "SCHED_FIFO";
		case PERF_MODE_NATIVE_NICE:
			return "Nice";
		case PERF_MODE_ERROR:
			return "Error";
		default:
			return "Unknown";
	}
}

int perf_mode_is_active(const PerfModeContext* ctx)
{
	return ctx && ctx->state != PERF_MODE_OFF &&
	       ctx->state != PERF_MODE_ERROR;
}
