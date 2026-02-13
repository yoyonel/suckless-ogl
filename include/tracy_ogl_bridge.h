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
void TracyOGL_ZoneBegin(const TracySourceLocationData* srcloc);
void TracyOGL_ZoneEnd(void);
void TracyOGL_Collect(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // TRACY_OGL_BRIDGE_H
