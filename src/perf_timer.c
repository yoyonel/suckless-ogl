#include "perf_timer.h"

#include "log.h"
#include <string.h>
#include <time.h>  // Pour clock_gettime et CLOCK_MONOTONIC

// ============================================================================
// Time conversion constants
// ============================================================================

enum TimeConversionFactors {
	NS_PER_MS = 1000000,     // Nanoseconds per millisecond
	NS_PER_US = 1000,        // Nanoseconds per microsecond
	NS_PER_S = 1000000000,   // Nanoseconds per second
	US_PER_S = 1000000,      // Microseconds per second
	MS_PER_S = 1000,         // Milliseconds per second
	LABEL_BUFFER_SIZE = 128  // Buffer size for Tracy labels
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

#ifdef TRACY_ENABLE
#include "profiler.h"
#include "utils.h"
// Source location statique pour les tâches hybrides afin d'éviter la double
// barre dans Tracy (on laisse 'function' à NULL pour n'afficher que le label)
static const struct ___tracy_source_location_data HYBRID_SRCLOC = {
    .name = "Hybrid Perf",
    .function = NULL,
    .file = __FILE__,
    .line = __LINE__,
    .color = 0};

static const struct ___tracy_source_location_data HOST_SRCLOC = {
    .name = "Host (CPU)",
    .function = NULL,
    .file = __FILE__,
    .line = __LINE__,
    .color = 0xAA6666};  // Rougeâtre pour le travail CPU

static const struct ___tracy_source_location_data SYNC_SRCLOC = {
    .name = "Sync (GPU Wait)",
    .function = NULL,
    .file = __FILE__,
    .line = __LINE__,
    .color = 0x66AA66};  // Verdâtre pour l'attente GPU
#endif

HybridTimer perf_hybrid_start(void)
{
	HybridTimer timer_struct;
	perf_timer_start(&timer_struct.cpu);
	gpu_timer_start(&timer_struct.gpu);

#ifdef TRACY_ENABLE
	PROFILE_FIBER_ENTER("Hybrid Perf");
	timer_struct.tracy_ctx = ___tracy_emit_zone_begin(&HYBRID_SRCLOC, 1);
	timer_struct.host_ctx = ___tracy_emit_zone_begin(&HOST_SRCLOC, 1);
	PROFILE_FIBER_LEAVE;
#endif

	return timer_struct;
}

/*
 * X-macro: generates the perf_hybrid_stop body parameterized by log level.
 * The log_fn argument is resolved at compile time (LOG_INFO or LOG_DEBUG),
 * so there is no runtime branch for the log level.
 *
 * Expands to a block that declares `double _gpu_ms` in the enclosing scope.
 * The caller is responsible for return semantics (void vs double).
 */
/* clang-format off */
#define HYBRID_STOP_BODY(log_fn)                                               \
	TRACY_HYBRID_STOP_PREAMBLE(timer);                                     \
	double _cpu_ms = perf_timer_elapsed_ms(&timer->cpu);                   \
	double _gpu_ms = gpu_timer_elapsed_ms(&timer->gpu, 1);                 \
	TRACY_HYBRID_STOP_SYNC_END();                                          \
	log_fn("perf.hybrid", "%s: [CPU: %.2f ms] [GPU: %.3f ms]",            \
	       label, _cpu_ms, _gpu_ms);                                       \
	TRACY_HYBRID_STOP_POSTAMBLE(timer, label, _cpu_ms, _gpu_ms);           \
	gpu_timer_cleanup(&timer->gpu)
/* clang-format on */

/*
 * Tracy helper macros — expand to nothing when TRACY_ENABLE is off.
 * Kept as macros (not inline functions) because TracyCZoneCtx has
 * local scope and cannot be passed across function boundaries.
 */
#ifdef TRACY_ENABLE

#define TRACY_HYBRID_STOP_PREAMBLE(timer)    \
	PROFILE_FIBER_ENTER("Hybrid Perf");  \
	PROFILE_ZONE_END((timer)->host_ctx); \
	TracyCZoneCtx _sync_ctx = ___tracy_emit_zone_begin(&SYNC_SRCLOC, 1)

#define TRACY_HYBRID_STOP_SYNC_END() PROFILE_ZONE_END(_sync_ctx)

#define TRACY_HYBRID_STOP_POSTAMBLE(timer, label, cpu_ms, gpu_ms)              \
	do {                                                                   \
		if (label) {                                                   \
			TracyCZoneName((timer)->tracy_ctx, (label),            \
			               strlen(label));                         \
		}                                                              \
		char _buf[LABEL_BUFFER_SIZE];                                  \
		int res = safe_snprintf(_buf, sizeof(_buf),                    \
		                        "CPU: %.2fms | GPU: %.3fms", (cpu_ms), \
		                        (gpu_ms));                             \
		if (res >= 0) {                                                \
			PROFILE_ZONE_TEXT((timer)->tracy_ctx, _buf,            \
			                  (size_t)res);                        \
		}                                                              \
		PROFILE_ZONE_END((timer)->tracy_ctx);                          \
		PROFILE_FIBER_LEAVE;                                           \
	} while (0)

#else /* !TRACY_ENABLE */

#define TRACY_HYBRID_STOP_PREAMBLE(timer) ((void)0)
#define TRACY_HYBRID_STOP_SYNC_END() ((void)0)
#define TRACY_HYBRID_STOP_POSTAMBLE(timer, label, cpu_ms, gpu_ms) ((void)0)

#endif /* TRACY_ENABLE */

/* --- Public API ---------------------------------------------------------- */

void perf_hybrid_stop(HybridTimer* timer, const char* label)
{
	if (timer == NULL) {
		return;
	}

	HYBRID_STOP_BODY(LOG_INFO);
}

double perf_hybrid_stop_debug(HybridTimer* timer, const char* label)
{
	if (timer == NULL) {
		return 0.0;
	}
	HYBRID_STOP_BODY(LOG_DEBUG);
	return _gpu_ms;
}
