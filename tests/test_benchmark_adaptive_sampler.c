#include "adaptive_sampler.h"
#include "unity.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

void test_benchmark_ascii_plot(void) {
    AdaptiveSampler sampler;
    // Initialize with dummy values
    adaptive_sampler_init(&sampler, 1.0f, 100, 60.0f);

    // Manually populate samples to ensure we have data to plot
    // Ensure capacity
    if (sampler.capacity < 100) {
        sampler.capacity = 100;
        sampler.samples = (AdaptiveSampleItem*)realloc(sampler.samples, sizeof(AdaptiveSampleItem) * sampler.capacity);
    }
    sampler.count = 100;

    for (size_t i = 0; i < sampler.count; ++i) {
        sampler.samples[i].timestamp = (float)i / 100.0f; // 0 to 1s
        sampler.samples[i].value = 60.0f + ((float)(i % 10) - 5.0f); // Fluctuating FPS
    }

    char buffer[256];
    size_t width = 40;
    float avg = 60.0f;

    // Warmup
    for (int i = 0; i < 100; ++i) {
        adaptive_sampler_ascii_plot(&sampler, buffer, sizeof(buffer), width, avg);
    }

    clock_t start = clock();
    int iterations = 200000;
    for (int i = 0; i < iterations; ++i) {
        adaptive_sampler_ascii_plot(&sampler, buffer, sizeof(buffer), width, avg);
    }
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("Benchmark: adaptive_sampler_ascii_plot %d iterations took %f seconds\n", iterations, cpu_time_used);

    adaptive_sampler_cleanup(&sampler);
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_benchmark_ascii_plot);
    return UNITY_END();
}
