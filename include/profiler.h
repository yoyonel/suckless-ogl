#ifndef PROFILER_H
#define PROFILER_H

/* --- Tracy Toggle --- */
// #define TRACY_ENABLE

#ifdef TRACY_ENABLE
#include "../deps/tracy/public/tracy/TracyC.h"

/* Frame Mark */
#define TRACE_FRAME_MARK TracyCFrameMark

/* GPU Stages (Legacy mapping if needed, or Tracy GPU marks) */
/* For now, we keep manual GPUProfiler for UI, but Tracy for CPU zones */
#define TRACE_ZONE_BEGIN(name) TracyCZoneN(ctx, name, 1)
#define TRACE_ZONE_END(ctx) TracyCZoneEnd(ctx)

/* Scoped Zones (C doesn't have RAII, so we use BEGIN/END) */
// Note: TracyC.h defines TracyCZone, TracyCZoneEnd etc.

#else
/* Legacy / Dummy macros */
#define TRACE_FRAME_MARK
#define TRACE_ZONE_BEGIN(name)
#define TRACE_ZONE_END(ctx)
#endif

/* Native GPU Stages (Always on for the in-app timeline) */
#include "gpu_profiler.h"
#define TRACE_GPU_STAGE(profiler_ptr, name, color) \
	GPU_STAGE_PROFILER(profiler_ptr, name, color)

/* Helper for scoped GPU+CPU tracing without a profiler instance */
#define TRACE_GPU_SCOPE(name, color) TRACE_GPU_STAGE(NULL, name, color)

#endif  // PROFILER_H
