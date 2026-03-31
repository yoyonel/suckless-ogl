#ifdef TRACY_ENABLE
#include "mem.h"
/* STB Image (Read) */
#define STBI_MALLOC(sz) tracy_malloc(sz)
#define STBI_REALLOC(p, newsz) tracy_realloc(p, newsz)
#define STBI_FREE(p) tracy_free(p)

/* STB Image Write */
#define STBIW_MALLOC(sz) tracy_malloc(sz)
#define STBIW_REALLOC(p, newsz) tracy_realloc(p, newsz)
#define STBIW_FREE(p) tracy_free(p)

/* STB TrueType */
#define STBTT_malloc(x, u) ((void)(u), tracy_malloc(x))
#define STBTT_free(x, u) ((void)(u), tracy_free(x))
#else
/* Standard Allocators */
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_REALLOC(p, newsz) realloc(p, newsz)
#define STBI_FREE(p) free(p)

#define STBIW_MALLOC(sz) malloc(sz)
#define STBIW_REALLOC(p, newsz) realloc(p, newsz)
#define STBIW_FREE(p) free(p)

#define STBTT_malloc(x, u) ((void)(u), malloc(x))
#define STBTT_free(x, u) ((void)(u), free(x))
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STB_TRUETYPE_IMPLEMENTATION
