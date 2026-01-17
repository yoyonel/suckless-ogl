// tests/test_fps.c
#include "fps.h"
#include "unity.h"

void setUp(void)
{
}
void tearDown(void)
{
}

void test_fps_module_exists(void)
{
	FpsCounter fps;
	fps_init(&fps, 0.95f, 5.0);
	TEST_PASS();
}

void test_fps_counter_initialization(void)
{
	FpsCounter fps;
	fps_init(&fps, 0.95f, 5.0);
	TEST_ASSERT_EQUAL_FLOAT(0.95f, fps.decay_factor);
	// Ne pas tester log_interval car Unity n'a pas le support double par
	// défaut
}

void test_fps_update(void)
{
	FpsCounter fps;
	fps_init(&fps, 0.95f, 5.0);
	fps_update(&fps, 0.016, 1.0);  // ~60 FPS, temps = 1.0s
	TEST_PASS();
}

void test_fps_average(void)
{
	FpsCounter fps;
	fps_init(&fps, 0.95f, 5.0);
	fps_update(&fps, 0.016, 1.0);
	// Vérifier que average_frame_time est positif (cast en float pour
	// Unity)
	TEST_ASSERT_GREATER_OR_EQUAL(0.0f, (float)fps.average_frame_time);
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