#ifdef TRACY_ENABLE
#include "tracy_memory.h"
#define STBI_MALLOC(sz) tracy_malloc(sz)
#define STBI_REALLOC(p, newsz) tracy_realloc(p, newsz)
#define STBI_FREE(p) tracy_free(p)

#define STBIW_MALLOC(sz) tracy_malloc(sz)
#define STBIW_REALLOC(p, newsz) tracy_realloc(p, newsz)
#define STBIW_FREE(p) tracy_free(p)

#define STBTT_malloc(x, u) ((void)(u), tracy_malloc(x))
#define STBTT_free(x, u) ((void)(u), tracy_free(x))
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
