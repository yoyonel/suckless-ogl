#ifndef METRIC_STACK_H
#define METRIC_STACK_H

#include <stdbool.h>

enum { METRIC_STACK_MAX_DEPTH = 32 };

typedef struct {
	int stack[METRIC_STACK_MAX_DEPTH];
	int size;
} MetricStack;

void metric_stack_init(MetricStack* stack);
bool metric_stack_push(MetricStack* stack, int stage_id);
int metric_stack_pop(MetricStack* stack);
int metric_stack_peek(MetricStack* stack);
int metric_stack_get_depth(MetricStack* stack);

#endif /* METRIC_STACK_H */
