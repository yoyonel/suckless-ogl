#include "tracy_ogl_bridge.h"

#ifdef TRACY_ENABLE

#include <glad/glad.h>

#include <tracy/TracyC.h>
#include <tracy/TracyOpenGL.hpp>

extern "C" {

void TracyOGL_Init(void)
{
	// Initializes the GPU context.
	// TracyGpuContext is a macro that calls Tracy::GetGpuCtx().Context()
	TracyGpuContext;
}

void TracyOGL_Destroy(void)
{
	// No explicit destroy needed for Tracy GPU context singleton usually.
}

void TracyOGL_Collect(void)
{
	TracyGpuCollect;
}

void* TracyOGL_ZoneBegin(const TracySourceLocationData* srcloc)
{
	// Cast to Tracy's internal SourceLocationData.
	const auto* tracy_srcloc =
	    reinterpret_cast<const tracy::SourceLocationData*>(srcloc);

	// Use GpuCtxScope (RAII) allocated on heap to manage the zone
	return new tracy::GpuCtxScope(tracy_srcloc, true);
}

void TracyOGL_ZoneEnd(void* ctx)
{
	if (ctx) {
		delete static_cast<tracy::GpuCtxScope*>(ctx);
	}
}

void TracyOGL_FrameImage(const void* data, uint16_t width, uint16_t height,
                         uint8_t offset, int flip)
{
	TracyCFrameImage(data, width, height, offset, flip);
}

}  // extern "C"

#endif  // TRACY_ENABLE
