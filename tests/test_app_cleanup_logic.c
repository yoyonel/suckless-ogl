#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "mock_gl_standalone.h"
#include "unity.h"

/* Include implementation under test */
#include "app_cleanup.c"

/* -------------------------------------------------------------------------- */
/*                                    MOCKS                                   */
/* -------------------------------------------------------------------------- */

static int g_window_destroy_count = 0;

void icosphere_free(IcosphereGeometry* g)
{
	(void)g;
}
void skybox_cleanup(Skybox* s)
{
	(void)s;
}
void sphere_sorter_cleanup(SphereSorter* s)
{
	(void)s;
}
void instanced_group_cleanup(InstancedGroup* g)
{
	(void)g;
}
void billboard_group_cleanup(BillboardGroup* g)
{
	(void)g;
}
void material_free_lib(MaterialLib* l)
{
	(void)l;
}
void shader_destroy(Shader* s)
{
	(void)s;
}
void ui_destroy(UIContext* u)
{
	(void)u;
}
void postprocess_cleanup(PostProcess* p)
{
	(void)p;
}
void adaptive_sampler_cleanup(AdaptiveSampler* s)
{
	(void)s;
}
void async_loader_shutdown(void)
{
}
void perf_mode_cleanup(PerfModeContext* c)
{
	(void)c;
}
void gpu_profiler_cleanup(GPUProfiler* p)
{
	(void)p;
}
void gpu_profiler_ui_cleanup(GPUProfilerUI* u)
{
	(void)u;
}
void window_destroy(GLFWwindow* w)
{
	(void)w;
	g_window_destroy_count++;
}
/* SSBO Mock if needed (macro might handle it) */
#ifdef USE_SSBO_RENDERING
void ssbo_group_cleanup(SSBOGroup* g) { (void)g; }
#endif

/* -------------------------------------------------------------------------- */
/*                                   TESTS                                    */
/* -------------------------------------------------------------------------- */

void setUp(void)
{
	mock_gl_reset_calls();
	g_window_destroy_count = 0;
}

void tearDown(void)
{
}

void test_cleanup_deletes_pending_ibl_textures(void)
{
	App app = {0};
	app.ibl_ctx.pending_hdr_tex = 101;
	app.ibl_ctx.pending_spec_tex = 102;
	app.ibl_ctx.pending_irr_tex = 103;

	app_cleanup(&app);

	TEST_ASSERT_EQUAL(0, app.ibl_ctx.pending_hdr_tex);
	TEST_ASSERT_EQUAL(0, app.ibl_ctx.pending_spec_tex);
	TEST_ASSERT_EQUAL(0, app.ibl_ctx.pending_irr_tex);

	/* Assert that glDeleteTextures was called at least 3 times. */
	TEST_ASSERT_GREATER_OR_EQUAL(3, mock_gl_get_delete_texture_call_count());
}

void test_cleanup_is_idempotent(void)
{
	App app = {0};
	app.window = (GLFWwindow*)0x1234; /* Fake window */
	app.hdr_texture = 55;

	/* First Cleanup */
	app_cleanup(&app);

	TEST_ASSERT_NULL(app.window);
	TEST_ASSERT_EQUAL(0, app.hdr_texture);
	TEST_ASSERT_EQUAL(1, g_window_destroy_count);

	/* Second Cleanup */
	app_cleanup(&app);

	/* Assert window_destroy not called again */
	TEST_ASSERT_EQUAL(1, g_window_destroy_count);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_cleanup_deletes_pending_ibl_textures);
	RUN_TEST(test_cleanup_is_idempotent);
	return UNITY_END();
}
