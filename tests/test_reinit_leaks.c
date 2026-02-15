#include "unity.h"
#include "mock_gl_standalone.h"
#include "log.h"
#include "shader.h"

// Define mocks for functions needed by the source files

// Mock log_message
void log_message(LogLevel level, const char* tag, const char* fmt, ...) { (void)level; (void)tag; (void)fmt; }

// Mock shader functions
// We can return a pointer to a static dummy shader
static Shader dummy_shader = {0};

Shader* shader_load(const char* v, const char* f) { (void)v; (void)f; return &dummy_shader; }
void shader_destroy(Shader* s) { (void)s; }
void shader_use(Shader* s) { (void)s; }
void shader_set_mat4(Shader* s, const char* n, const float* v) { (void)s; (void)n; (void)v; }
void shader_set_vec3(Shader* s, const char* n, const float* v) { (void)s; (void)n; (void)v; }
void shader_set_float(Shader* s, const char* n, float v) { (void)s; (void)n; (void)v; }
void shader_set_int(Shader* s, const char* n, int v) { (void)s; (void)n; (void)v; }
void shader_set_vec2(Shader* s, const char* n, const float* v) { (void)s; (void)n; (void)v; }

// Helper to mock shader_get_uniform_location
GLint shader_get_uniform_location(Shader* shader, const char* name) { (void)shader; (void)name; return 0; }

// Include source files directly
// Dependencies need to be included first or handled via headers

#include "adaptive_sampler.c"
#include "metric_stack.c"
#include "gpu_profiler.c"

// ui.c includes ui.h -> shader.h
#include "ui.c"

// render_utils.c
#include "render_utils.c"

// skybox.c
#include "skybox.c"

// stb implementation
#include "stb_image_impl.c"

void setUp(void) {
    mock_gl_reset_calls();
}

void tearDown(void) {
}

void test_gpu_profiler_init_leak(void) {
    GPUProfiler profiler = {0};

    // First init
    gpu_profiler_init(&profiler);

    // Simulate that the profiler was used and has valid queries
    // gpu_profiler_init creates queries and assigns them.
    // Mock glGenQueries assigns 999.

    TEST_ASSERT_EQUAL(999, profiler.buffers[0].queries[0].query_start);

    mock_gl_reset_calls();

    // Re-init WITHOUT cleanup. Should verify that it DOES call glDeleteQueries now.
    gpu_profiler_init(&profiler);

    // Check that cleanup was called
    TEST_ASSERT_GREATER_THAN(0, mock_gl_get_delete_query_call_count());

    // Cleanup to be nice
    gpu_profiler_cleanup(&profiler);
}

void test_ui_init_leak(void) {
    UIContext ui = {0};

    // To properly test leak, we need ui_init to FAIL (so it cleans up?) or SUCCEED?
    // If it succeeds, it overwrites.

    // Pre-populate with a texture ID
    ui.texture = 123;

    // Calling ui_init with existing ID.
    // It will likely fail because we didn't mock file reading for font, returning 0.
    // But BEFORE failing, does it check texture? No.
    // Does it overwrite texture? Yes: ui_context->texture = 0;

    // So if we check mock_gl_get_delete_buffer_call_count (for texture?) No, texture delete.
    // We need mock_gl_get_delete_texture_call_count?
    // mock_gl_standalone currently mocks glDeleteTextures but doesn't expose a counter?
    // It exposes g_delete_buffer_call_count.

    // I need to update mock_gl_standalone.c to expose texture delete count.
    // But wait, ui_init resets it to 0 immediately.
    // So if it returns 0 (fail), ui.texture is 0.
    // And if it didn't call delete, we leaked 123.

    // I will add mock_gl_get_delete_texture_call_count to mock_gl_standalone.
    // For now, I can assert that ui.texture is 0 (it was overwritten).

    int result = ui_init(&ui, "dummy_font.ttf", 16.0f);
    TEST_ASSERT_EQUAL(0, result); // Expect fail due to font load

    // Verify delete called (leak prevention)
    TEST_ASSERT_EQUAL(1, mock_gl_get_delete_texture_call_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gpu_profiler_init_leak);
    RUN_TEST(test_ui_init_leak);
    return UNITY_END();
}
