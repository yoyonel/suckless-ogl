#include "perf_timer.h"

#include "log.h"
#include <time.h>  // Pour clock_gettime et CLOCK_MONOTONIC

// ============================================================================
// Time conversion constants
// ============================================================================

enum TimeConversionFactors {
	NS_PER_MS = 1000000,    // Nanoseconds per millisecond
	NS_PER_US = 1000,       // Nanoseconds per microsecond
	NS_PER_S = 1000000000,  // Nanoseconds per second
	US_PER_S = 1000000,     // Microseconds per second
	MS_PER_S = 1000         // Milliseconds per second
};

static const double NS_TO_MS = 1.0 / (double)NS_PER_MS;
static const double NS_TO_US = 1.0 / (double)NS_PER_US;
static const double NS_TO_S = 1.0 / (double)NS_PER_S;
static const double S_TO_MS = (double)MS_PER_S;
static const double S_TO_US = (double)US_PER_S;

// ============================================================================
// CPU Timer Implementation
// ============================================================================

void perf_timer_start(PerfTimer* timer)
{
	if (timer == NULL) {
		return;
	}
	// NOLINTNEXTLINE(misc-include-cleaner)
	clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

double perf_timer_elapsed_ms(PerfTimer* timer)
{
	if (timer == NULL) {
		return 0.0;
	}
	// NOLINTNEXTLINE(misc-include-cleaner)
	clock_gettime(CLOCK_MONOTONIC, &timer->end);

	const double seconds =
	    (double)(timer->end.tv_sec - timer->start.tv_sec);
	const double nanoseconds =
	    (double)(timer->end.tv_nsec - timer->start.tv_nsec);

	return (seconds * S_TO_MS) + (nanoseconds * NS_TO_MS);
}

double perf_timer_elapsed_us(PerfTimer* timer)
{
	if (timer == NULL) {
		return 0.0;
	}
	// NOLINTNEXTLINE(misc-include-cleaner)
	clock_gettime(CLOCK_MONOTONIC, &timer->end);

	const double seconds =
	    (double)(timer->end.tv_sec - timer->start.tv_sec);
	const double nanoseconds =
	    (double)(timer->end.tv_nsec - timer->start.tv_nsec);

	return (seconds * S_TO_US) + (nanoseconds * NS_TO_US);
}

double perf_timer_elapsed_s(PerfTimer* timer)
{
	if (timer == NULL) {
		return 0.0;
	}
	// NOLINTNEXTLINE(misc-include-cleaner)
	clock_gettime(CLOCK_MONOTONIC, &timer->end);

	const double seconds =
	    (double)(timer->end.tv_sec - timer->start.tv_sec);
	const double nanoseconds =
	    (double)(timer->end.tv_nsec - timer->start.tv_nsec);

	return seconds + (nanoseconds * NS_TO_S);
}

// ============================================================================
// GPU Timer Implementation (OpenGL Query Objects)
// ============================================================================

void gpu_timer_start(GPUTimer* timer)
{
	if (timer == NULL) {
		return;
	}

	// Générer le query object
	glGenQueries(1, &timer->query);

	// Commencer la mesure de temps GPU
	glBeginQuery(GL_TIME_ELAPSED, timer->query);
	timer->active = 1;
}

double gpu_timer_elapsed_ms(GPUTimer* timer, int wait_for_result)
{
	if (timer == NULL || !timer->active) {
		return -1.0;
	}

	// Terminer la query
	glEndQuery(GL_TIME_ELAPSED);
	timer->active = 0;

	GLuint64 elapsed_ns = 0;

	if (wait_for_result) {
		// Bloquer jusqu'à ce que le résultat soit disponible
		// Équivalent de ctx.finish() en Python avant
		// glGetQueryObjectuiv
		glGetQueryObjectui64v(timer->query, GL_QUERY_RESULT,
		                      &elapsed_ns);
	} else {
		// Vérifier si le résultat est disponible sans bloquer
		GLint available = 0;
		glGetQueryObjectiv(timer->query, GL_QUERY_RESULT_AVAILABLE,
		                   &available);

		if (!available) {
			LOG_WARN("perf.gpu",
			         "GPU timer result not yet available");
			return -1.0;
		}

		glGetQueryObjectui64v(timer->query, GL_QUERY_RESULT,
		                      &elapsed_ns);
	}

	// Convertir nanosecondes en millisecondes
	return (double)elapsed_ns * NS_TO_MS;
}

void gpu_timer_cleanup(GPUTimer* timer)
{
	if (timer == NULL) {
		return;
	}

	if (timer->query != 0) {
		glDeleteQueries(1, &timer->query);
		timer->query = 0;
	}
	timer->active = 0;
}