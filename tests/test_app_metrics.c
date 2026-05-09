#include "adaptive_sampler.h"
#include "app_metrics.h"
#include "log.h"
#include "unity.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

/* Mock data for GPUProfiler */
static void setup_mock_profiler(GPUProfiler* profiler)
{
	memset(profiler, 0, sizeof(GPUProfiler));
	profiler->stage_count = 2;

	/* Stage 0: Total Frame (Root) */
	strcpy(profiler->stages[0].name, "Total Frame");
	profiler->stages[0].depth = 0;
	profiler->stages[0].parent_index = -1;
	profiler->stages[0].duration_ms = 16.6F;
	profiler->stages[0].start_offset_ms = 0.0F;
	adaptive_sampler_init(&profiler->stages[0].duration_sampler, 2.0F, 60,
	                      60.0F);

	/* Stage 1: Geometry Pass */
	strcpy(profiler->stages[1].name, "Geometry");
	profiler->stages[1].depth = 1;
	profiler->stages[1].parent_index = 0;
	profiler->stages[1].duration_ms = 8.0F;
	profiler->stages[1].start_offset_ms = 1.0F;
	adaptive_sampler_init(&profiler->stages[1].duration_sampler, 2.0F, 60,
	                      60.0F);
}

void setUp(void)
{
}

void tearDown(void)
{
}

/**
 * @brief Handle cases with no telemetry data.
 *
 * Motivation: Ensure the system doesn't crash or report phantom data when
 * no profiling stages are defined, returning gracefully.
 */
void test_app_metrics_log_gpu_stats_empty(void)
{
	GPUProfiler profiler;
	memset(&profiler, 0, sizeof(GPUProfiler));
	profiler.stage_count = 0;

	/* Should return false if no stages */
	TEST_ASSERT_FALSE(app_metrics_log_gpu_stats(&profiler, 1.0, true));
}

/**
 * @brief Verify window-based logging frequency.
 *
 * Motivation: Validate that metrics are only logged when the defined
 * time window (e.g., 0.5s) has elapsed, preventing console spam.
 */
void test_app_metrics_log_gpu_stats_window_not_elapsed(void)
{
	GPUProfiler profiler;
	setup_mock_profiler(&profiler);

	/* Add a sample to root */
	adaptive_sampler_add(&profiler.stages[0].duration_sampler, 16.6F, 1);

	/* Current time 0.1, window start 0.0, duration 0.5 (from settings) ->
	 * NOT elapsed */
	TEST_ASSERT_FALSE(app_metrics_log_gpu_stats(&profiler, 0.1, true));

	gpu_profiler_cleanup(&profiler);
}

/**
 * @brief Handle valid timing windows with missing samples.
 *
 * Motivation: Ensure the system correctly identifies when a time window
 * has passed but no actual GPU samples were recorded (e.g., during a stall).
 */
void test_app_metrics_log_gpu_stats_no_samples(void)
{
	GPUProfiler profiler;
	setup_mock_profiler(&profiler);

	/* Elapsed time (3.0 > 0.5) but no samples */
	TEST_ASSERT_FALSE(app_metrics_log_gpu_stats(&profiler, 3.0, true));

	gpu_profiler_cleanup(&profiler);
}

/* Global buffer for log interception */
static char g_last_log[2048];

static void mock_log_callback(LogLevel level, const char* tag,
                              const char* message)
{
	(void)level;
	(void)tag;
	safe_strncat(g_last_log, sizeof(g_last_log), message);
}

/**
 * @brief Validate full end-to-end logging workflow with content verification.
 *
 * Motivation: Confirm that when samples are present and the window has
 * elapsed, the metrics are correctly aggregated and reported to the log.
 * This test uses a LogCallback to intercept and verify the actual strings.
 */
void test_app_metrics_log_gpu_stats_success(void)
{
	GPUProfiler profiler;
	setup_mock_profiler(&profiler);

	/* Add samples */
	adaptive_sampler_add(&profiler.stages[0].duration_sampler, 16.6F, 1);
	adaptive_sampler_add(&profiler.stages[1].duration_sampler, 8.3F, 1);

	/* Force window start to 0.0 for predictability */
	profiler.stages[0].duration_sampler.window_start_time = 0.0;

	/* Prepare log interception */
	g_last_log[0] = '\0';
	log_set_callback(mock_log_callback);

	/* Elapsed time (3.0 > 0.5) and has samples */
	TEST_ASSERT_TRUE(app_metrics_log_gpu_stats(&profiler, 3.0, true));

	/* Verify log contents */
	TEST_ASSERT_NOT_NULL(strstr(g_last_log, "Total Frame"));
	TEST_ASSERT_NOT_NULL(strstr(g_last_log, "Geometry"));
	/* 8.3ms is ~50% of 16.6ms */
	TEST_ASSERT_NOT_NULL(strstr(g_last_log, "50% Total Frame"));

	/* Cleanup */
	log_set_callback(NULL);
	gpu_profiler_cleanup(&profiler);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_app_metrics_log_gpu_stats_empty);
	RUN_TEST(test_app_metrics_log_gpu_stats_window_not_elapsed);
	RUN_TEST(test_app_metrics_log_gpu_stats_no_samples);
	RUN_TEST(test_app_metrics_log_gpu_stats_success);
	return UNITY_END();
}
