#include "adaptive_sampler.h"
#include "unity.h"
#include <stdlib.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_adaptive_sampler_tracks_frames(void)
{
	AdaptiveSampler sampler;
	adaptive_sampler_init(&sampler, 2.0f, 10, 60.0f);

	// Initial state
	uint64_t start = 999;
	uint64_t end = 999;
	adaptive_sampler_get_window_range(&sampler, &start, &end);
	TEST_ASSERT_EQUAL_UINT64(0, start);
	TEST_ASSERT_EQUAL_UINT64(0, end);

	// Add first sample at frame 100
	adaptive_sampler_add(&sampler, 16.6f, 100);

	// Check start/end updated
	adaptive_sampler_get_window_range(&sampler, &start, &end);
	TEST_ASSERT_EQUAL_UINT64(100, start);
	TEST_ASSERT_EQUAL_UINT64(100, end);
	TEST_ASSERT_EQUAL_UINT64(100, sampler.samples[0].frame_index);

	// Add second sample at frame 150
	adaptive_sampler_add(&sampler, 16.6f, 150);

	adaptive_sampler_get_window_range(&sampler, &start, &end);
	TEST_ASSERT_EQUAL_UINT64(100, start);  // Start should stay same
	TEST_ASSERT_EQUAL_UINT64(150, end);    // End updated
	TEST_ASSERT_EQUAL_UINT64(150, sampler.samples[1].frame_index);

	// Add third sample at frame 200
	adaptive_sampler_add(&sampler, 16.6f, 200);

	adaptive_sampler_get_window_range(&sampler, &start, &end);
	TEST_ASSERT_EQUAL_UINT64(100, start);
	TEST_ASSERT_EQUAL_UINT64(200, end);

	// Reset
	adaptive_sampler_reset(&sampler, 10.0);

	// Check reset clears frames
	adaptive_sampler_get_window_range(&sampler, &start, &end);
	TEST_ASSERT_EQUAL_UINT64(0, start);
	TEST_ASSERT_EQUAL_UINT64(0, end);

	// Add new after reset
	adaptive_sampler_add(&sampler, 16.6f, 300);
	adaptive_sampler_get_window_range(&sampler, &start, &end);
	TEST_ASSERT_EQUAL_UINT64(300, start);
	TEST_ASSERT_EQUAL_UINT64(300, end);

	// Test get_sample_indices
	uint64_t indices[10];
	size_t count =
	    adaptive_sampler_get_sample_indices(&sampler, indices, 10);
	TEST_ASSERT_EQUAL_INT(1, count);  // Only 1 sample added after reset
	TEST_ASSERT_EQUAL_UINT64(300, indices[0]);

	// Add more samples to test list retrieval
	adaptive_sampler_add(&sampler, 16.6f, 310);
	adaptive_sampler_add(&sampler, 16.6f, 320);

	count = adaptive_sampler_get_sample_indices(&sampler, indices, 2);
	TEST_ASSERT_EQUAL_INT(2, count);  // Capped by max_count
	TEST_ASSERT_EQUAL_UINT64(300, indices[0]);
	TEST_ASSERT_EQUAL_UINT64(310, indices[1]);

	count = adaptive_sampler_get_sample_indices(&sampler, indices, 10);
	TEST_ASSERT_EQUAL_INT(3, count);  // All 3 samples
	TEST_ASSERT_EQUAL_UINT64(300, indices[0]);
	TEST_ASSERT_EQUAL_UINT64(310, indices[1]);
	TEST_ASSERT_EQUAL_UINT64(320, indices[2]);

	adaptive_sampler_cleanup(&sampler);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_adaptive_sampler_tracks_frames);
	return UNITY_END();
}
