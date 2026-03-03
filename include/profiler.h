#ifndef PROFILER_H
#define PROFILER_H

#include "gl_common.h"

/* --- Tracy Toggle --- */
// #define TRACY_ENABLE

#ifdef TRACY_ENABLE
#include "../deps/tracy/public/tracy/TracyC.h"

/* Frame Mark */
#define PROFILE_FRAME_MARK TracyCFrameMark

/* CPU Zones */
#define PROFILE_ZONE(ctx, name) TracyCZoneN(ctx, name, 1)
#define PROFILE_ZONE_I(ctx, name) TracyCZoneN(ctx, name, 0)
#define PROFILE_ZONE_TEXT(ctx, text, len) TracyCZoneText(ctx, text, len)
#define PROFILE_ZONE_END(ctx) TracyCZoneEnd(ctx)

/* Messaging and Threads */
#define PROFILE_MESSAGE(text, len) TracyCMessage(text, len)
#define PROFILE_MESSAGE_L(literal) TracyCMessageL(literal)
#define PROFILE_MESSAGE_C(text, len, color) TracyCMessageC(text, len, color)
#define PROFILE_THREAD_NAME(name) TracyCSetThreadName(name)

/* Fibers (for async state tracking) */
#define PROFILE_FIBER_ENTER(name) TracyCFiberEnter(name)
#define PROFILE_FIBER_LEAVE TracyCFiberLeave

#else
/* Legacy / Dummy macros */
#define PROFILE_FRAME_MARK ((void)0)

#define PROFILE_ZONE(ctx, name) \
	int ctx = 0;            \
	(void)(ctx);            \
	(void)(name)
#define PROFILE_ZONE_I(ctx, name) \
	int ctx = 0;              \
	(void)(ctx);              \
	(void)(name)
#define PROFILE_ZONE_TEXT(ctx, text, len) \
	((void)(ctx), (void)(text), (void)(len))
#define PROFILE_ZONE_END(ctx) ((void)(ctx))
#define PROFILE_MESSAGE(text, len) ((void)(text), (void)(len))
#define PROFILE_MESSAGE_L(literal) ((void)(literal))
#define PROFILE_MESSAGE_C(text, len, color) \
	((void)(text), (void)(len), (void)(color))
#define PROFILE_THREAD_NAME(name) ((void)(name))
#define PROFILE_FIBER_ENTER(name) ((void)(name))
#define PROFILE_FIBER_LEAVE ((void)0)
#endif

/* Native GPU Stages (Always on for the in-app timeline) */
#include "gpu_profiler.h"
#define TRACE_GPU_STAGE(profiler_ptr, name, color) \
	GPU_STAGE_PROFILER(profiler_ptr, name, color)

/* Helper for scoped GPU+CPU tracing without a profiler instance */
#define TRACE_GPU_SCOPE(name, color) TRACE_GPU_STAGE(NULL, name, color)

#endif  // PROFILER_H
