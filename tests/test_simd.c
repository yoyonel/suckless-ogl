#include "simd_utils.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_float_to_half_basic_values(void)
{
	float input[] = {0.0f, 1.0f, -1.0f, 0.5f, 2.0f, 65504.0f, /* Max half */
	                 -0.0f};
	uint16_t expected[] = {0x0000, 0x3C00, 0xBC00, 0x3800,
	                       0x4000, 0x7BFF, 0x8000};
	uint16_t output[7];

	convert_float_to_half_simd(input, output, 7);

	for (int i = 0; i < 7; ++i) {
		TEST_ASSERT_EQUAL_HEX16_MESSAGE(expected[i], output[i],
		                                "Mismatch at index");
	}
}

void test_float_to_half_arrays(void)
{
	/* Test with more than 8 elements to trigger SIMD loop + tail */
	float input[20];
	uint16_t output[20];

	for (int i = 0; i < 20; ++i) {
		input[i] = 1.0f;
	}

	convert_float_to_half_simd(input, output, 20);

	for (int i = 0; i < 20; ++i) {
		TEST_ASSERT_EQUAL_HEX16(0x3C00, output[i]);
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_float_to_half_basic_values);
	RUN_TEST(test_float_to_half_arrays);
	return UNITY_END();
}
