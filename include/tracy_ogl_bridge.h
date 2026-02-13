#ifndef TRACY_OGL_BRIDGE_H
#define TRACY_OGL_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TracySourceLocationData {
	const char* name;
	const char* function;
	const char* file;
	uint32_t line;
	uint32_t color;
} TracySourceLocationData;

#ifdef TRACY_ENABLE
void TracyOGL_Init(void);
void TracyOGL_Destroy(void);
void* TracyOGL_ZoneBegin(const TracySourceLocationData* srcloc);
void TracyOGL_ZoneEnd(void* ctx);
void TracyOGL_Collect(void);
void TracyOGL_FrameImage(const void* data, uint16_t width, uint16_t height,
                         uint8_t offset, int flip);
#endif

#ifdef __cplusplus
}
#endif

#endif  // TRACY_OGL_BRIDGE_H
