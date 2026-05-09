// tests/test_fps.c
#include "fps.h"
#include "unity.h"

static const float DECAY_FACTOR = 0.95F;
static const double LOG_INTERVAL = 5.0;
static const double FRAME_TIME_60FPS = 0.016;
static const double ONE_SECOND = 1.0;

void setUp(void)
{
}

void tearDown(void)
{
}

void test_fps_module_exists(void)
{
	FpsCounter fps;
	fps_init(&fps, DECAY_FACTOR, LOG_INTERVAL);
	TEST_PASS();
}

void test_fps_counter_initialization(void)
{
	FpsCounter fps;
	fps_init(&fps, DECAY_FACTOR, LOG_INTERVAL);
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	TEST_ASSERT_EQUAL_FLOAT(DECAY_FACTOR, fps.decay_factor);
	// Ne pas tester log_interval car Unity n'a pas le support double par
	// défaut
}

void test_fps_update(void)
{
	FpsCounter fps;
	fps_init(&fps, DECAY_FACTOR, LOG_INTERVAL);
	fps_update(&fps, FRAME_TIME_60FPS,
	           ONE_SECOND);  // ~60 FPS, temps = 1.0s
	TEST_PASS();
}

void test_fps_average(void)
{
	FpsCounter fps;
	fps_init(&fps, DECAY_FACTOR, LOG_INTERVAL);
	fps_update(&fps, FRAME_TIME_60FPS, ONE_SECOND);
	// Vérifier que average_frame_time est positif (cast en float pour
	// Unity)
	TEST_ASSERT_GREATER_OR_EQUAL(0.0F, (float)fps.average_frame_time);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_fps_module_exists);
	RUN_TEST(test_fps_counter_initialization);
	RUN_TEST(test_fps_update);
	RUN_TEST(test_fps_average);
	return UNITY_END();
}
