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
	(void)clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

double perf_timer_elapsed_ms(PerfTimer* timer)
{
	if (timer == NULL) {
		return 0.0;
	}
	// NOLINTNEXTLINE(misc-include-cleaner)
	(void)clock_gettime(CLOCK_MONOTONIC, &timer->end);

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
	(void)clock_gettime(CLOCK_MONOTONIC, &timer->end);

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
	(void)clock_gettime(CLOCK_MONOTONIC, &timer->end);

	const double seconds =
	    (double)(timer->end.tv_sec - timer->start.tv_sec);
	const double nanoseconds =
	    (double)(timer->end.tv_nsec - timer->start.tv_nsec);

	return seconds + (nanoseconds * NS_TO_S);
}

// ============================================================================
// GPU Timer Implementation (Timestamp queries)
// ============================================================================

void gpu_timer_start(GPUTimer* timer)
{
	if (timer == NULL) {
		return;
	}

	// Générer les query objects
	glGenQueries(1, &timer->query_start);
	glGenQueries(1, &timer->query_end);

	// Enregistrer le timestamp actuel sur le GPU
	glQueryCounter(timer->query_start, GL_TIMESTAMP);

	// Forcer l'envoi de la commande de début pour éviter le batching
	glFlush();

	timer->active = 1;
}

double gpu_timer_elapsed_ms(GPUTimer* timer, int wait_for_result)
{
	if (timer == NULL || !timer->active) {
		return -1.0;
	}

	// Forcer la fin des opérations GPU avant de prendre le timestamp de fin
	// C'est nécessaire pour mesurer le temps réel d'exécution incluant les
	// compute shaders qui pourraient être asynchrones.
	glFinish();

	// Enregistrer le timestamp final sur le GPU
	glQueryCounter(timer->query_end, GL_TIMESTAMP);
	timer->active = 0;

	GLuint64 start_time = 0;
	GLuint64 end_time = 0;

	if (wait_for_result) {
		// Bloquer jusqu'à ce que les deux timestamps soient disponibles
		glGetQueryObjectui64v(timer->query_start, GL_QUERY_RESULT,
		                      &start_time);
		glGetQueryObjectui64v(timer->query_end, GL_QUERY_RESULT,
		                      &end_time);
	} else {
		// Vérifier si les résultats sont disponibles sans bloquer
		GLint available = 0;
		glGetQueryObjectiv(timer->query_end, GL_QUERY_RESULT_AVAILABLE,
		                   &available);

		if (!available) {
			return -1.0;
		}

		glGetQueryObjectui64v(timer->query_start, GL_QUERY_RESULT,
		                      &start_time);
		glGetQueryObjectui64v(timer->query_end, GL_QUERY_RESULT,
		                      &end_time);
	}

	// Calculer la durée
	GLuint64 elapsed_ns =
	    (end_time > start_time) ? (end_time - start_time) : 0;

	static const GLuint64 GPU_SHORT_DURATION_THRESHOLD_NS = 100000;
	if (elapsed_ns > 0 && elapsed_ns < GPU_SHORT_DURATION_THRESHOLD_NS) {
		LOG_DEBUG("perf.gpu", "Short GPU duration: %lu ns",
		          (unsigned long)elapsed_ns);
	}

	// Convertir nanosecondes en millisecondes
	return (double)elapsed_ns * NS_TO_MS;
}

void gpu_timer_cleanup(GPUTimer* timer)
{
	if (timer == NULL) {
		return;
	}

	if (timer->query_start != 0) {
		glDeleteQueries(1, &timer->query_start);
		timer->query_start = 0;
	}
	if (timer->query_end != 0) {
		glDeleteQueries(1, &timer->query_end);
		timer->query_end = 0;
	}
	timer->active = 0;
}

// ============================================================================
// Hybrid Timer Implementation
// ============================================================================

HybridTimer perf_hybrid_start(void)
{
	HybridTimer timer_struct;
	perf_timer_start(&timer_struct.cpu);
	gpu_timer_start(&timer_struct.gpu);
	return timer_struct;
}

void perf_hybrid_stop(HybridTimer* timer, const char* label)
{
	if (timer == NULL) {
		return;
	}

	double cpu_ms = perf_timer_elapsed_ms(&timer->cpu);
	double gpu_ms = gpu_timer_elapsed_ms(&timer->gpu, 1);

	// Utilisation de %g ou plus de précision pour les petites valeurs
	LOG_INFO("perf.hybrid", "%s: [CPU: %.2f ms] [GPU: %.3f ms]", label,
	         cpu_ms, gpu_ms);

	gpu_timer_cleanup(&timer->gpu);
}
