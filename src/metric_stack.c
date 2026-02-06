#include "metric_stack.h"

void metric_stack_init(MetricStack* stack)
{
	if (stack) {
		stack->size = 0;
		for (int i = 0; i < METRIC_STACK_MAX_DEPTH; ++i) {
			stack->stack[i] = 0;
		}
	}
}

bool metric_stack_push(MetricStack* stack, int stage_id)
{
	if (!stack || stack->size >= METRIC_STACK_MAX_DEPTH) {
		return false;
	}
	stack->stack[stack->size++] = stage_id;
	return true;
}

int metric_stack_pop(MetricStack* stack)
{
	if (!stack || stack->size <= 0) {
		return -1;
	}
	return stack->stack[--stack->size];
}

int metric_stack_peek(MetricStack* stack)
{
	if (!stack || stack->size <= 0) {
		return -1;
	}
	return stack->stack[stack->size - 1];
}

int metric_stack_get_depth(MetricStack* stack)
{
	if (!stack) {
		return 0;
	}
	return stack->size;
}
