#include "tracy_ogl_bridge.h"

#ifdef TRACY_ENABLE

#include <glad/glad.h>
#include <tracy/TracyOpenGL.hpp>

extern "C" {

void TracyOGL_Init(void) {
    // Initializes the GPU context.
    // TracyGpuContext is a macro that calls Tracy::GetGpuCtx().Context()
    TracyGpuContext;
}

void TracyOGL_Destroy(void) {
    // No explicit destroy needed for Tracy GPU context singleton usually.
}

void TracyOGL_Collect(void) {
    TracyGpuCollect;
}

void TracyOGL_ZoneBegin(const TracySourceLocationData* srcloc) {
    // Cast to Tracy's internal SourceLocationData.
    // We assume binary compatibility or layout compatibility.
    // Tracy's SourceLocationData:
    // struct SourceLocationData { const char* name; const char* function; const char* file; uint32_t line; uint32_t color; };
    const auto* tracy_srcloc = reinterpret_cast<const tracy::SourceLocationData*>(srcloc);

    // Call QueryBegin
    tracy::GetGpuCtx().QueryBegin(*tracy_srcloc);
}

void TracyOGL_ZoneEnd(void) {
    tracy::GetGpuCtx().QueryEnd();
}

} // extern "C"

#endif // TRACY_ENABLE
