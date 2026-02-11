#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Include headers from the project */
#include "billboard_rendering.h"
#include "glad/glad.h"

/* -------------------------------------------------------------------------- */
/*                                 MOCK GLAD                                  */
/* -------------------------------------------------------------------------- */

static GLuint g_last_deleted_buffer = 0;
static int g_delete_buffer_call_count = 0;
static GLuint g_generated_buffer_id = 123; /* Arbitrary non-zero ID */

void mock_gl_reset_calls(void)
{
	g_last_deleted_buffer = 0;
	g_delete_buffer_call_count = 0;
}

GLuint mock_gl_get_generated_buffer_id(void)
{
	return g_generated_buffer_id;
}

/* Mock OpenGL Functions */
void glGenBuffers(GLsizei n, GLuint* buffers)
{
	(void)n;
	*buffers = g_generated_buffer_id;
}

void glBindBuffer(GLenum target, GLuint buffer)
{
	(void)target;
	(void)buffer;
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage)
{
	(void)target;
	(void)size;
	(void)data;
	(void)usage;
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                     const void* data)
{
	(void)target;
	(void)offset;
	(void)size;
	(void)data;
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
	(void)n;
	if (buffers) {
		g_last_deleted_buffer = *buffers;
		g_delete_buffer_call_count++;
	}
}

/* Mock VAO functions */
void glGenVertexArrays(GLsizei n, GLuint* arrays)
{
	(void)n;
	*arrays = 456; /* Arbitrary ID */
}

void glBindVertexArray(GLuint array)
{
	(void)array;
}

void glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
{
	(void)n;
	(void)arrays;
}

void glEnableVertexAttribArray(GLuint index)
{
	(void)index;
}

void glDisableVertexAttribArray(GLuint index)
{
	(void)index;
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const void* pointer)
{
	(void)index;
	(void)size;
	(void)type;
	(void)normalized;
	(void)stride;
	(void)pointer;
}

void glVertexAttribDivisor(GLuint index, GLuint divisor)
{
	(void)index;
	(void)divisor;
}

void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei instancecount)
{
	(void)mode;
	(void)first;
	(void)count;
	(void)instancecount;
}

void glEnable(GLenum cap)
{
	(void)cap;
}

void glDisable(GLenum cap)
{
	(void)cap;
}

GLboolean glIsEnabled(GLenum cap)
{
	(void)cap;
	return GL_FALSE;
}

/* -------------------------------------------------------------------------- */
/*                             MOCK RENDER UTILS                              */
/* -------------------------------------------------------------------------- */

/* We need to mock functions called by billboard_rendering.c that are not in
 * GLAD */
/* Since we include billboard_rendering.c directly, we can define them here if
 * they are not static in billboard_rendering.c */
/* But wait, render_utils functions are external. */

void render_utils_setup_sphere_instance_attributes(GLsizei stride,
                                                   size_t albedo_offset,
                                                   size_t metallic_offset)
{
	(void)stride;
	(void)albedo_offset;
	(void)metallic_offset;
}

/* -------------------------------------------------------------------------- */
/*                        INCLUDE IMPLEMENTATION UNDER TEST                   */
/* -------------------------------------------------------------------------- */

/* Define SYNC_ATTR_START and MAX_VERTEX_ATTRIBS_BASELINE as they might be used
 */
#ifndef SYNC_ATTR_START
#define SYNC_ATTR_START 10
#endif
#ifndef MAX_VERTEX_ATTRIBS_BASELINE
#define MAX_VERTEX_ATTRIBS_BASELINE 16
#endif

/* Include the source file directly to test it in isolation */
#include "billboard_rendering.c"

/* -------------------------------------------------------------------------- */
/*                                    TESTS                                   */
/* -------------------------------------------------------------------------- */

#define TEST_ASSERT_TRUE(cond)                                             \
	if (!(cond)) {                                                     \
		fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
		exit(1);                                                   \
	} else {                                                           \
		printf("PASS: %s\n", #cond);                               \
	}

#define TEST_ASSERT_EQUAL(expected, actual)                                 \
	if ((expected) != (actual)) {                                       \
		fprintf(stderr, "FAIL: %s == %s (%d != %d) at line %d\n",   \
		        #expected, #actual, (int)(expected), (int)(actual), \
		        __LINE__);                                          \
		exit(1);                                                    \
	} else {                                                            \
		printf("PASS: %s == %s\n", #expected, #actual);             \
	}

void test_billboard_group_cleanup_deletes_vbo(void)
{
	printf("Running test_billboard_group_cleanup_deletes_vbo...\n");

	BillboardGroup group = {0};
	SphereInstance instances[1] = {{{0}, {0}, 0}};

	mock_gl_reset_calls();

	/* Initialize the group - this should create the VBO */
	billboard_group_init(&group, instances, 1);

	TEST_ASSERT_EQUAL(mock_gl_get_generated_buffer_id(),
	                  group.instance_vbo);

	/* Cleanup */
	billboard_group_cleanup(&group);

	/* Verify VBO was deleted */
	TEST_ASSERT_EQUAL(1, g_delete_buffer_call_count);
	TEST_ASSERT_EQUAL(mock_gl_get_generated_buffer_id(),
	                  g_last_deleted_buffer);
}

void test_billboard_group_cleanup_resets_vbo_to_zero(void)
{
	printf("Running test_billboard_group_cleanup_resets_vbo_to_zero...\n");

	BillboardGroup group = {0};
	SphereInstance instances[1] = {{{0}, {0}, 0}};

	mock_gl_reset_calls();

	billboard_group_init(&group, instances, 1);
	billboard_group_cleanup(&group);

	TEST_ASSERT_EQUAL(0, group.instance_vbo);
}

void test_billboard_group_cleanup_handles_uninitialized_group(void)
{
	printf(
	    "Running "
	    "test_billboard_group_cleanup_handles_uninitialized_group...\n");

	BillboardGroup group = {0};
	/* group.instance_vbo is 0 */

	mock_gl_reset_calls();

	/* Should be safe to call on zeroed group */
	billboard_group_cleanup(&group);

	TEST_ASSERT_EQUAL(0, g_delete_buffer_call_count);
}

int main(void)
{
	printf("Running Billboard Cleanup Tests...\n");

	test_billboard_group_cleanup_deletes_vbo();
	test_billboard_group_cleanup_resets_vbo_to_zero();
	test_billboard_group_cleanup_handles_uninitialized_group();

	printf("All billboard cleanup tests passed!\n");
	return 0;
}
