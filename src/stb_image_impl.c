#ifdef TRACY_ENABLE
/*
 * mem.h redefines malloc/realloc/free to Tracy-tracked variants.
 * STB headers use standard allocators internally, which get intercepted
 * by these macro redirections — no explicit STBI_MALLOC override needed.
 */
#include "mem.h"  // IWYU pragma: keep
#else
/* Standard Allocators (explicit overrides for STB) */
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_REALLOC(p, newsz) realloc(p, newsz)
#define STBI_FREE(p) free(p)

#define STBIW_MALLOC(sz) malloc(sz)
#define STBIW_REALLOC(p, newsz) realloc(p, newsz)
#define STBIW_FREE(p) free(p)

#define STBTT_malloc(x, u) ((void)(u), malloc(x))
#define STBTT_free(x, u) ((void)(u), free(x))
#endif

#define STBI_NO_FAILURE_STRINGS

#define STBI_ONLY_PNG
// #define STBI_ONLY_JPEG
#define STBI_ONLY_HDR
#define STBI_ONLY_PNM

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STB_TRUETYPE_IMPLEMENTATION
