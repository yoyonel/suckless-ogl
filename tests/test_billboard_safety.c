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

static struct {
	GLsizeiptr buffer_size;
	bool buffer_created;
	bool buffer_overflow;
} mock_gl_state = {0};

void reset_mock_state(void)
{
	mock_gl_state.buffer_size = 0;
	mock_gl_state.buffer_created = false;
	mock_gl_state.buffer_overflow = false;
}

void glGenBuffers(GLsizei n, GLuint* buffers)
{
	(void)n;
	*buffers = 1;
}

void glBindBuffer(GLenum target, GLuint buffer)
{
	(void)target;
	(void)buffer;
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
	(void)target;
	(void)data;
	(void)usage;
	mock_gl_state.buffer_size = size;
	mock_gl_state.buffer_created = true;
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
	(void)target;
	(void)data;
	if (!mock_gl_state.buffer_created) {
		printf("FAIL: glBufferSubData called before glBufferData\n");
		exit(1);
	}
	if (offset + size > mock_gl_state.buffer_size) {
		mock_gl_state.buffer_overflow = true;
		printf("VULNERABILITY CONFIRMED: Buffer Overflow! allocated=%ld, write=%ld (offset=%ld + size=%ld)\n",
		       (long)mock_gl_state.buffer_size, (long)(offset + size), (long)offset, (long)size);
	}
}

void glGenVertexArrays(GLsizei n, GLuint* arrays) { (void)n; *arrays = 1; }
void glBindVertexArray(GLuint array) { (void)array; }
void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) { (void)n; (void)arrays; }
void glEnableVertexAttribArray(GLuint index) { (void)index; }
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) { (void)index; (void)size; (void)type; (void)normalized; (void)stride; (void)pointer; }
void glVertexAttribDivisor(GLuint index, GLuint divisor) { (void)index; (void)divisor; }
void glDisableVertexAttribArray(GLuint index) { (void)index; }
void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) { (void)mode; (void)first; (void)count; (void)instancecount; }
GLboolean glIsEnabled(GLenum cap) { (void)cap; return GL_FALSE; }
void glDisable(GLenum cap) { (void)cap; }
void glEnable(GLenum cap) { (void)cap; }
GLenum glGetError(void) { return GL_NO_ERROR; }

/* Mock render_utils */
#include "render_utils.h"
void render_utils_setup_sphere_instance_attributes(GLsizei stride, size_t offset_albedo, size_t offset_metallic)
{
	(void)stride;
	(void)offset_albedo;
	(void)offset_metallic;
}

/* Include unit under test */
/* We need SphereInstance definition */
#include "instanced_rendering.h"
/* But wait, instanced_rendering.h is included by billboard_rendering.c */
/* We need to define the struct or include header. billboard_rendering.c includes "instanced_rendering.h" */
/* We can just include the source file */
#include "billboard_rendering.c"

/* Test Logic */

#define ASSERT_TRUE(cond, msg) \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); } \
	else { printf("PASS: %s\n", msg); }

#define ASSERT_FALSE(cond, msg) \
	if (cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); } \
	else { printf("PASS: %s\n", msg); }

void test_billboard_overflow(void)
{
	printf("Running test_billboard_overflow...\n");
	reset_mock_state();

	BillboardGroup group;
	int initial_count = 10;
	SphereInstance* data = calloc(initial_count, sizeof(SphereInstance));

	/* Initialize with capacity 10 */
	billboard_group_init(&group, data, initial_count);
	ASSERT_TRUE(mock_gl_state.buffer_created, "Buffer should be created");
	/* Check allocated size */
	long expected_size = initial_count * sizeof(SphereInstance);
	if (mock_gl_state.buffer_size != expected_size) {
		printf("FAIL: Buffer size mismatch. Expected %ld, got %ld\n", expected_size, (long)mock_gl_state.buffer_size);
		exit(1);
	}

	/* Update with 20 instances (Overflow!) */
	int overflow_count = 20;
	SphereInstance* overflow_data = calloc(overflow_count, sizeof(SphereInstance));

	billboard_group_update(&group, overflow_data, overflow_count);

	/* Check if overflow occurred */
	if (mock_gl_state.buffer_overflow) {
		printf("FAIL: Buffer overflow detected!\n");
		exit(1);
	} else {
		printf("PASS: No overflow detected\n");
	}

	/* Verify clamping logic */
	if (group.instance_count != initial_count) {
		printf("FAIL: instance_count not clamped. Expected %d, got %d\n", initial_count, group.instance_count);
		exit(1);
	} else {
		printf("PASS: instance_count correctly clamped to capacity\n");
	}

	free(data);
	free(overflow_data);
}

int main(void)
{
	test_billboard_overflow();
	return 0;
}
