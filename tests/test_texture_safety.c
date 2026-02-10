#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mock Logging Implementation */
#include "log.h"
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
}
void log_set_callback(LogCallback callback)
{
	(void)callback;
}
void log_set_level(LogLevel level)
{
	(void)level;
}
LogLevel log_get_level(void)
{
	return LOG_LEVEL_INFO;
}

/* Mock OpenGL Implementation */
#include "glad/glad.h"
/* (Only implementing what is needed for texture.c and stubs) */

void glDeleteTextures(GLsizei n, const GLuint* textures)
{
	(void)n;
	(void)textures;
}
void glGenTextures(GLsizei n, GLuint* textures)
{
	(void)n;
	*textures = 123;
}
void glBindTexture(GLenum target, GLuint texture)
{
	(void)target;
	(void)texture;
}
void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	(void)target;
	(void)pname;
	(void)param;
}
void glPixelStorei(GLenum pname, GLint param)
{
	(void)pname;
	(void)param;
}
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const void* pixels)
{
	(void)target;
	(void)level;
	(void)xoffset;
	(void)yoffset;
	(void)width;
	(void)height;
	(void)format;
	(void)type;
	(void)pixels;
}
GLenum glGetError(void)
{
	return GL_NO_ERROR;
}
void glGenerateMipmap(GLenum target)
{
	(void)target;
}
void glObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                   const GLchar* label)
{
	(void)identifier;
	(void)name;
	(void)length;
	(void)label;
}

/* Mock render_utils */
#include "render_utils.h"

/* State to capture calls to create_texture_2d */
static struct {
	bool called;
	int width;
	int height;
} mock_create_tex_state = {0};

void reset_mock_state(void)
{
	mock_create_tex_state.called = false;
	mock_create_tex_state.width = 0;
	mock_create_tex_state.height = 0;
}

GLuint render_utils_create_texture_2d(int width, int height,
                                      GLenum internal_format, GLint levels,
                                      const char* label)
{
	mock_create_tex_state.called = true;
	mock_create_tex_state.width = width;
	mock_create_tex_state.height = height;
	(void)internal_format;
	(void)levels;
	(void)label;
	return 1; /* Return dummy texture ID */
}

/*
 * Mock STB Image
 * We need to define stbi_info_from_file and stbi_load_from_file.
 * texture.c includes <stb_image.h>. Since we don't have it in include path
 * (except via system/deps which we want to avoid linking against),
 * we rely on the fact that we compile this file standalone.
 * BUT, texture.c includes <stb_image.h>. If the compiler can't find it, it fails.
 * So we MUST provide a fake stb_image.h or include the real one but override symbols.
 *
 * Strategy: Provide a fake stb_image.h in a local directory included with -I.
 * But for now, let's assume we can just implement the functions.
 * The issue is ensuring the signatures match what texture.c expects.
 */

/* State to control stbi mocks */
static struct {
	int info_width;
	int info_height;
	int info_channels;
	int info_ret;

	int load_width;
	int load_height;
	int load_channels;
	unsigned char* load_ret;
} mock_stbi_state = {0};

/* We need to define the functions with correct linkage.
 * Since texture.c includes stb_image.h, it expects them to be declared there.
 * We can create a mocks/stb_image.h that declares them.
 */

#include <stdio.h> /* FILE */

int stbi_info_from_file(FILE const* f, int* x, int* y, int* comp)
{
	(void)f;
	*x = mock_stbi_state.info_width;
	*y = mock_stbi_state.info_height;
	*comp = mock_stbi_state.info_channels;
	return mock_stbi_state.info_ret;
}

unsigned char* stbi_load_from_file(FILE const* f, int* x, int* y, int* comp,
                                   int req_comp)
{
	(void)f;
	(void)req_comp;
	*x = mock_stbi_state.load_width;
	*y = mock_stbi_state.load_height;
	*comp = mock_stbi_state.load_channels;
	return mock_stbi_state.load_ret;
}

void stbi_image_free(void* retval_from_stbi_load)
{
	(void)retval_from_stbi_load;
	/* No-op for mock */
}

float* stbi_loadf_from_file(FILE const* f, int* x, int* y, int* comp, int req_comp)
{
	/* Not used in texture_load, but used in texture_load_pixels */
	(void)f;
	(void)req_comp;
	return NULL;
}


/* Include the unit under test */
/* We need to ensure texture.c can find its includes.
   It includes "texture.h", "gl_common.h", "log.h", "render_utils.h", "utils.h", <math.h>, <stb_image.h>.
   We have mocks for log.h, render_utils.h, glad/glad.h (via gl_common.h).
   We need utils.h (utils.c not included, so we might need to mock or link it).
   texture.c uses utils.h for CLEANUP_FILE/CLEANUP_TEXTURE macros.
   These are macros, so just header is enough.
   But it might use safe_snprintf? No, likely not.
   Let's check utils.h.
*/
#include "texture.c"

/* Test Logic */

#define ASSERT_TRUE(cond, msg) \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); } \
	else { printf("PASS: %s\n", msg); }

#define ASSERT_FALSE(cond, msg) \
	if (cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); } \
	else { printf("PASS: %s\n", msg); }

#define ASSERT_EQ(a, b, msg) \
	if ((a) != (b)) { fprintf(stderr, "FAIL: %s (%d != %d)\n", msg, (int)a, (int)b); exit(1); } \
	else { printf("PASS: %s\n", msg); }

void test_texture_load_safe(void)
{
	printf("Running test_texture_load_safe...\n");
	reset_mock_state();

	/* Setup safe state */
	mock_stbi_state.info_width = 100;
	mock_stbi_state.info_height = 100;
	mock_stbi_state.info_channels = 4;
	mock_stbi_state.info_ret = 1;

	mock_stbi_state.load_width = 100;
	mock_stbi_state.load_height = 100;
	mock_stbi_state.load_channels = 4;
	mock_stbi_state.load_ret = (unsigned char*)malloc(100*100*4); /* valid ptr */

	/* We need a dummy file that fopen can open. texture.c uses fopen.
	   We can use /dev/null or create a temp file. */
	FILE* f = fopen("dummy_tex.png", "wb");
	if(f) fclose(f);

	GLuint tex = texture_load("dummy_tex.png");

	ASSERT_TRUE(tex != 0, "Texture load should succeed");
	ASSERT_TRUE(mock_create_tex_state.called, "render_utils_create_texture_2d should be called");
	ASSERT_EQ(mock_create_tex_state.width, 100, "Width should be 100");
	ASSERT_EQ(mock_create_tex_state.height, 100, "Height should be 100");

	free(mock_stbi_state.load_ret);
	remove("dummy_tex.png");
}

void test_texture_load_unsafe_dimensions_mismatch(void)
{
	printf("Running test_texture_load_unsafe_dimensions_mismatch...\n");
	reset_mock_state();

	/* Setup TOCTOU scenario:
	   Info says 100x100 (Safe)
	   Load says 10000x10000 (Unsafe, > 8192)
	*/
	mock_stbi_state.info_width = 100;
	mock_stbi_state.info_height = 100;
	mock_stbi_state.info_channels = 4;
	mock_stbi_state.info_ret = 1;

	mock_stbi_state.load_width = 10000;
	mock_stbi_state.load_height = 10000;
	mock_stbi_state.load_channels = 4;
	mock_stbi_state.load_ret = (unsigned char*)malloc(100); /* Dummy buffer, size doesn't matter for mock */

	FILE* f = fopen("dummy_tex_unsafe.png", "wb");
	if(f) fclose(f);

	GLuint tex = texture_load("dummy_tex_unsafe.png");

	/* We expect the load to fail because dimensions > MAX_TEXTURE_DIMENSION */
	ASSERT_EQ(tex, 0, "Texture load should fail for unsafe dimensions");
	ASSERT_FALSE(mock_create_tex_state.called, "Should not call create_texture");

	free(mock_stbi_state.load_ret);
	remove("dummy_tex_unsafe.png");
}

int main(void)
{
	test_texture_load_safe();
	test_texture_load_unsafe_dimensions_mismatch();
	return 0;
}
