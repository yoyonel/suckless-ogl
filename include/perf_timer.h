#ifndef PERF_TIMER_H
#define PERF_TIMER_H

#include <glad/glad.h>

#include <time.h>

/**
 * @brief High-precision performance timer (équivalent de time.perf_counter() en
 * Python)
 *
 * Utilise clock_gettime(CLOCK_MONOTONIC) pour une mesure précise du temps CPU.
 * Résolution typique : nanosecondes
 */

typedef struct {
	struct timespec start;
	struct timespec end;
} PerfTimer;

/**
 * @brief GPU performance timer using OpenGL Query Objects
 *
 * Mesure le temps réel d'exécution GPU (équivalent GL_TIME_ELAPSED en
 * Python/PyOpenGL)
 */
typedef struct {
	GLuint query_start;
	GLuint query_end;
	int active;
} GPUTimer;

/**
 * @brief Hybrid performance timer combining CPU and GPU measurements
 */
typedef struct {
	PerfTimer cpu;
	GPUTimer gpu;
} HybridTimer;

// ============================================================================
// CPU Timer API
// ============================================================================

/**
 * @brief Démarre le timer CPU
 * @param timer Pointeur vers la structure PerfTimer
 */
void perf_timer_start(PerfTimer* timer);

/**
 * @brief Arrête le timer et retourne le temps écoulé en millisecondes
 * @param timer Pointeur vers la structure PerfTimer
 * @return Temps écoulé en millisecondes (double précision)
 */
double perf_timer_elapsed_ms(PerfTimer* timer);

/**
 * @brief Arrête le timer et retourne le temps écoulé en microsecondes
 * @param timer Pointeur vers la structure PerfTimer
 * @return Temps écoulé en microsecondes (double précision)
 */
double perf_timer_elapsed_us(PerfTimer* timer);

/**
 * @brief Arrête le timer et retourne le temps écoulé en secondes
 * @param timer Pointeur vers la structure PerfTimer
 * @return Temps écoulé en secondes (double précision)
 */
double perf_timer_elapsed_s(PerfTimer* timer);

// ============================================================================
// GPU Timer API
// ============================================================================

/**
 * @brief Initialise et démarre un timer GPU
 * @param timer Pointeur vers la structure GPUTimer
 */
void gpu_timer_start(GPUTimer* timer);

/**
 * @brief Arrête le timer GPU et retourne le temps écoulé en millisecondes
 * @param timer Pointeur vers la structure GPUTimer
 * @param wait_for_result Si vrai, bloque jusqu'à ce que le résultat soit
 * disponible
 * @return Temps écoulé GPU en millisecondes, ou -1.0 si non disponible
 */
double gpu_timer_elapsed_ms(GPUTimer* timer, int wait_for_result);

/**
 * @brief Nettoie les ressources du timer GPU
 * @param timer Pointeur vers la structure GPUTimer
 */
void gpu_timer_cleanup(GPUTimer* timer);

// ============================================================================
// Hybrid Timer API
// ============================================================================

/**
 * @brief Initialise et démarre un timer hybride (CPU + GPU)
 * @return Une structure HybridTimer initialisée et démarrée
 */
HybridTimer perf_hybrid_start(void);

/**
 * @brief Arrête et loggue les résultats d'un timer hybride
 * @param timer Pointeur vers le timer
 * @param label Étiquette pour la ligne de log
 */
void perf_hybrid_stop(HybridTimer* timer, const char* label);

// ============================================================================
// Macros helpers
// ============================================================================

/**
 * @brief Macro helper pour mesurer automatiquement un bloc de code (CPU)
 *
 * Usage:
 *   PERF_MEASURE_MS(load_time) {
 *       // Code à mesurer
 *   }
 *   printf("Took %.2f ms\n", load_time);
 */
#define PERF_MEASURE_MS(var_name)                                              \
	double var_name = 0.0;                                                 \
	for (PerfTimer                                                         \
	         _timer##var_name = {0},                                       \
	         *_run = (perf_timer_start(&_timer##var_name), (PerfTimer*)1); \
	     _run;                                                             \
	     var_name = perf_timer_elapsed_ms(&_timer##var_name), _run = NULL)

/**
 * @brief Macro helper pour mesurer et logger automatiquement (CPU)
 *
 * Usage:
 *   PERF_MEASURE_LOG("Loading HDR") {
 *       texture_load_hdr(...);
 *   }
 */
#define PERF_MEASURE_LOG(label)                                            \
	for (PerfTimer _timer = {0},                                       \
	               *_run = (perf_timer_start(&_timer), (PerfTimer*)1); \
	     _run; LOG_INFO("perf", "%s: %.2f ms", label,                  \
	                    perf_timer_elapsed_ms(&_timer)),               \
	               _run = NULL)

/**
 * @brief Macro helper pour mesurer un bloc de code GPU
 *
 * Usage:
 *   GPU_MEASURE_MS(gpu_time) {
 *       // Commandes OpenGL à mesurer
 *   }
 *   printf("GPU took %.2f ms\n", gpu_time);
 */
#define GPU_MEASURE_MS(var_name)                                           \
	double var_name = 0.0;                                             \
	for (GPUTimer _gpu_timer##var_name = {0},                          \
	              *_gpu_run = (gpu_timer_start(&_gpu_timer##var_name), \
	                           (GPUTimer*)1);                          \
	     _gpu_run;                                                     \
	     var_name = gpu_timer_elapsed_ms(&_gpu_timer##var_name, 1),    \
	              gpu_timer_cleanup(&_gpu_timer##var_name),            \
	              _gpu_run = NULL)

/**
 * @brief Macro helper pour mesurer et logger automatiquement (GPU)
 *
 * Usage:
 *   GPU_MEASURE_LOG("PBR Generation") {
 *       build_pbr_maps(...);
 *   }
 */
#define GPU_MEASURE_LOG(label)                                             \
	for (GPUTimer                                                      \
	         _gpu_timer = {0},                                         \
	         *_gpu_run = (gpu_timer_start(&_gpu_timer), (GPUTimer*)1); \
	     _gpu_run; LOG_INFO("perf.gpu", "%s: %.2f ms", label,          \
	                        gpu_timer_elapsed_ms(&_gpu_timer, 1)),     \
	         gpu_timer_cleanup(&_gpu_timer), _gpu_run = NULL)

/**
 * @brief Macro helper pour mesurer et logger automatiquement (CPU + GPU)
 *
 * Combine les mesures CPU (wall-clock) et GPU pour un aperçu complet.
 * Utile pour identifier si un bottleneck est CPU-bound (ex: le driver)
 * ou GPU-bound (ex: la complexité des shaders).
 *
 * Usage:
 *   HYBRID_MEASURE_LOG("Texture Upload") {
 *       glTexImage2D(...);
 *   }
 */
#define HYBRID_MEASURE_LOG(label)                                             \
	for (HybridTimer _h = perf_hybrid_start(), *_h_run = (HybridTimer*)1; \
	     _h_run; perf_hybrid_stop(&_h, label), _h_run = NULL)

#endif  // PERF_TIMER_H
