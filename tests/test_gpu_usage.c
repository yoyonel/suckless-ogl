// tests/test_gpu_usage.c
#include "gpu_usage.h"
#include "unity.h"
#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_gpu_usage_init_cleanup_lifecycle(void)
{
	GPUUsageMonitor mon;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&mon, 0, sizeof(GPUUsageMonitor));

	gpu_usage_init(&mon);
	/* On any platform, init should succeed without crash */
	gpu_usage_cleanup(&mon);
	TEST_ASSERT_FALSE(mon.available);
	TEST_ASSERT_EQUAL_INT(0, mon.stream_count);
}

void test_gpu_usage_init_null_safe(void)
{
	/* Must not crash on NULL */
	gpu_usage_init(NULL);
	gpu_usage_cleanup(NULL);
	gpu_usage_update(NULL);
	TEST_PASS();
}

void test_gpu_usage_get_load_null(void)
{
	float load = gpu_usage_get_load(NULL);
	TEST_ASSERT_EQUAL_FLOAT(-1.0F, load);
}

void test_gpu_usage_is_available_null(void)
{
	TEST_ASSERT_FALSE(gpu_usage_is_available(NULL));
}

void test_gpu_usage_get_load_unavailable(void)
{
	GPUUsageMonitor mon;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&mon, 0, sizeof(GPUUsageMonitor));
	mon.available = false;

	float load = gpu_usage_get_load(&mon);
	TEST_ASSERT_EQUAL_FLOAT(-1.0F, load);
}

void test_gpu_usage_update_unavailable_noop(void)
{
	GPUUsageMonitor mon;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&mon, 0, sizeof(GPUUsageMonitor));
	mon.available = false;
	mon.load_percent = 0.0F;

	/* update on unavailable monitor should be a no-op */
	gpu_usage_update(&mon);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, mon.load_percent);
}

void test_gpu_usage_cleanup_resets_state(void)
{
	GPUUsageMonitor mon;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&mon, 0, sizeof(GPUUsageMonitor));

	gpu_usage_init(&mon);
	gpu_usage_cleanup(&mon);

	TEST_ASSERT_FALSE(mon.available);
	TEST_ASSERT_EQUAL_INT(0, mon.stream_count);
}

void test_gpu_usage_double_cleanup_safe(void)
{
	GPUUsageMonitor mon;
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(&mon, 0, sizeof(GPUUsageMonitor));

	gpu_usage_init(&mon);
	gpu_usage_cleanup(&mon);
	/* Second cleanup should not crash */
	gpu_usage_cleanup(&mon);
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gpu_usage_init_cleanup_lifecycle);
	RUN_TEST(test_gpu_usage_init_null_safe);
	RUN_TEST(test_gpu_usage_get_load_null);
	RUN_TEST(test_gpu_usage_is_available_null);
	RUN_TEST(test_gpu_usage_get_load_unavailable);
	RUN_TEST(test_gpu_usage_update_unavailable_noop);
	RUN_TEST(test_gpu_usage_cleanup_resets_state);
	RUN_TEST(test_gpu_usage_double_cleanup_safe);
	return UNITY_END();
}
