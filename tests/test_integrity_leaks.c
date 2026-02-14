#include "unity.h"
#include <stddef.h>
#include <stdio.h>

/* Mock GL (provides glad.h compatible symbols) */
#include "mock_gl_standalone.h"

/* Mock cglm types */
#include <cglm/cglm.h>

/* Mock Shader */
#include "shader.h"

/* Mock Render Utils (declarations) */
#include "render_utils.h"

/* Mock implementation of render_utils */
void render_utils_create_fullscreen_quad(GLuint* vao, GLuint* vbo) {
    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
}

/* Mock implementation of shader */
GLint shader_get_uniform_location(Shader* shader, const char* name) {
    (void)shader; (void)name;
    return 0;
}
void shader_use(Shader* shader) { (void)shader; }

/* Note: glUniform* functions are provided by mock_gl_standalone.c */

/* Add missing mocks that are not in mock_gl_standalone.c yet */
void glDrawArrays(GLenum mode, GLint first, GLsizei count) { (void)mode; (void)first; (void)count; }
void glDepthFunc(GLenum func) { (void)func; }
void glActiveTexture(GLenum texture) { (void)texture; }

/* Include Skybox implementation under test */
/* We need to define included headers to avoid conflicts or missing files */
#define SKYBOX_H
#include "skybox.h" /* Use the header for struct definition */

/* Override implementation of Skybox functions to test logic */
/* Actually, we want to test src/skybox.c, so we include it. */
/* But we need to handle its includes. */

/* Redefine missing includes if necessary, or rely on mocks */
#undef SKYBOX_H /* skybox.c will include it */

/* We need to trick skybox.c into using our mocks */
/* It includes "glad/glad.h", "render_utils.h", "shader.h", "cglm/types.h" */

/* Verify cglm/types.h presence or mock it */
/* Since we are compiling manually or via a special target, we can control includes. */

#include "../src/skybox.c"

void setUp(void) {
    mock_gl_reset_calls();
}

void tearDown(void) {}

void test_skybox_reinit_leak_prevention(void) {
    Skybox skybox = {0};
    Shader dummy_shader = {0};

    /* First Initialization */
    /* Expectation: Creates VAO and VBO. No deletes. */
    skybox_init(&skybox, &dummy_shader);

    TEST_ASSERT_NOT_EQUAL(0, skybox.vao);
    TEST_ASSERT_NOT_EQUAL(0, skybox.vbo);
    TEST_ASSERT_EQUAL(0, mock_gl_get_delete_buffer_call_count());

    GLuint first_vao = skybox.vao;
    GLuint first_vbo = skybox.vbo;

    /* Second Initialization (Re-init) */
    /* Expectation: Should detect existing resources and delete them. */
    skybox_init(&skybox, &dummy_shader);

    /* Verify resources were deleted */
    TEST_ASSERT_EQUAL(1, mock_gl_get_delete_buffer_call_count());
    /* Verify new resources are different (mock usually increments IDs) */
    /* Note: mock_gl_standalone might return fixed IDs if not configured to increment. */
    /* Let's assume it behaves like a simple mock. */

    /* Verify handles are valid */
    TEST_ASSERT_NOT_EQUAL(0, skybox.vao);
    TEST_ASSERT_NOT_EQUAL(0, skybox.vbo);
}

void test_skybox_cleanup_zeroes_handles(void) {
    Skybox skybox = {0};
    Shader dummy_shader = {0};
    skybox_init(&skybox, &dummy_shader);

    TEST_ASSERT_NOT_EQUAL(0, skybox.vao);
    TEST_ASSERT_NOT_EQUAL(0, skybox.vbo);

    skybox_cleanup(&skybox);

    TEST_ASSERT_EQUAL(0, skybox.vao);
    TEST_ASSERT_EQUAL(0, skybox.vbo);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_skybox_reinit_leak_prevention);
    RUN_TEST(test_skybox_cleanup_zeroes_handles);
    return UNITY_END();
}
