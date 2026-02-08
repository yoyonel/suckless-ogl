#include "metric_stack.h"
#include "unity.h"

void setUp(void)
{
}
void tearDown(void)
{
}

void test_metric_stack_init_should_zero_size(void)
{
	MetricStack stack;
	metric_stack_init(&stack);
	TEST_ASSERT_EQUAL_INT(0, stack.size);
	TEST_ASSERT_EQUAL_INT(0, metric_stack_get_depth(&stack));
}

void test_metric_stack_push_should_add_element(void)
{
	MetricStack stack;
	metric_stack_init(&stack);
	TEST_ASSERT_TRUE(metric_stack_push(&stack, 42));
	TEST_ASSERT_EQUAL_INT(1, stack.size);
	TEST_ASSERT_EQUAL_INT(42, metric_stack_peek(&stack));
}

void test_metric_stack_pop_should_return_last_element(void)
{
	MetricStack stack;
	metric_stack_init(&stack);
	metric_stack_push(&stack, 10);
	metric_stack_push(&stack, 20);

	TEST_ASSERT_EQUAL_INT(20, metric_stack_pop(&stack));
	TEST_ASSERT_EQUAL_INT(1, stack.size);
	TEST_ASSERT_EQUAL_INT(10, metric_stack_peek(&stack));
}

void test_metric_stack_overflow_protection(void)
{
	MetricStack stack;
	metric_stack_init(&stack);
	for (int i = 0; i < METRIC_STACK_MAX_DEPTH; i++) {
		TEST_ASSERT_TRUE(metric_stack_push(&stack, i));
	}
	// Try one more
	TEST_ASSERT_FALSE(metric_stack_push(&stack, 999));
	TEST_ASSERT_EQUAL_INT(METRIC_STACK_MAX_DEPTH, stack.size);
}

void test_metric_stack_underflow_protection(void)
{
	MetricStack stack;
	metric_stack_init(&stack);
	TEST_ASSERT_EQUAL_INT(-1, metric_stack_pop(&stack));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_metric_stack_init_should_zero_size);
	RUN_TEST(test_metric_stack_push_should_add_element);
	RUN_TEST(test_metric_stack_pop_should_return_last_element);
	RUN_TEST(test_metric_stack_overflow_protection);
	RUN_TEST(test_metric_stack_underflow_protection);
	return UNITY_END();
}
