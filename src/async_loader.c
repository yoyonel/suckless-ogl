#include "async_loader.h"

#include "async_backend.h"
#include "log.h"
#include "perf_timer.h"
#include "profiler.h"
#include "tracy_manager.h"
#include "utils.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct AsyncLoader {
	AsyncRequest current_request;
	pthread_mutex_t request_mutex;
	pthread_cond_t request_cond;
	pthread_t worker_thread;
	volatile bool running;
	volatile bool has_pending_work;
	PerfTimer sys_timer;
	TracyManager* tracy_mgr;
};

#define transition_tracy_state(s) \
	tracy_manager_async_transition(loader->tracy_mgr, s)
#define cleanup_tracy_states() tracy_manager_async_end(loader->tracy_mgr)

/**
 * @brief Performs the actual format conversion (e.g., float -> half float) into
 * the PBO.
 *
 * Runs the format-specific backend conversion. Since writing to mapped PBO
 * memory can be slow, the request mutex is released during the conversion
 * process to keep the main thread from stalling.
 *
 * @param loader Pointer to the AsyncLoader instance.
 *
 * Concurrency constraints:
 * - PRECONDITION: request_mutex MUST be HELD on entry.
 * - POSTCONDITION: request_mutex is HELD on exit.
 * - Mutex is temporarily released during backend->convert().
 * - Expected state on entry: ASYNC_CONVERTING.
 * - State on exit: ASYNC_READY.
 */
static void async_perform_conversion(AsyncLoader* loader)
{
	/* Extract request info while holding lock (lock is held on entry) */
	AssetType rtype = loader->current_request.resource_type;
	void* dst_ptr = loader->current_request.pbo_mapped_ptr;
	const AsyncBackendInterface* backend = async_backend_get(rtype);

	if (!backend || !backend->convert) {
		pthread_mutex_unlock(&loader->request_mutex);
		return;
	}

	/* Relâchement du verrou pour le transfert lourd de données (I/O ou
	 * conversion) afin que le thread principal continue à rendre les frames
	 * sans saccade. */
	pthread_mutex_unlock(&loader->request_mutex);

	backend->convert(dst_ptr, &loader->current_request);

	pthread_mutex_lock(&loader->request_mutex);

	/* Common End State: La conversion est terminée, la ressource est prête
	 * à être uploadée */
	loader->current_request.state = ASYNC_READY;
	transition_tracy_state(ASYNC_READY);
	LOG_INFO("suckless-ogl.async", "Finished loading & converting: %s",
	         loader->current_request.path);
}

/**
 * @brief Destroys backend-specific payload allocations.
 *
 * Invokes the active format backend's cleanup function to free CPU allocations
 * loaded during the IO phase.
 *
 * @param loader Pointer to the AsyncLoader instance.
 *
 * Concurrency constraints:
 * - PRECONDITION: request_mutex MUST be HELD on entry.
 * - POSTCONDITION: request_mutex is HELD on exit.
 */
static void async_cleanup_payload(AsyncLoader* loader)
{
	/* Le mutex est supposé être verrouillé ici */
	const AsyncBackendInterface* backend =
	    async_backend_get(loader->current_request.resource_type);
	if (backend && backend->cleanup) {
		backend->cleanup(&loader->current_request);
	}
}

/**
 * @brief Worker thread wait loop for PBO allocation from the main thread.
 *
 * Blocks the worker thread until the main thread allocates a PBO, maps it,
 * and calls async_loader_provide_pbo() (which transitions state to
 * ASYNC_CONVERTING and signals request_cond).
 *
 * @param loader Pointer to the AsyncLoader instance.
 *
 * Concurrency constraints:
 * - PRECONDITION: request_mutex MUST be HELD on entry.
 * - POSTCONDITION: request_mutex is HELD on exit.
 * - Temporarily releases the mutex during pthread_cond_wait().
 * - Expected state on entry: ASYNC_WAITING_FOR_PBO.
 * - State on exit: ASYNC_READY (success) or ASYNC_FAILED (cancel/shutdown).
 */
static void async_wait_and_convert(AsyncLoader* loader)
{
	/* Le mutex DOIT être verrouillé avant d'appeler cette fonction.
	   La requête DOIT être dans l'état ASYNC_WAITING_FOR_PBO. */
	transition_tracy_state(ASYNC_WAITING_FOR_PBO);

	/* 1. Wait for PBO to be mapped and provided by the main thread.
	 * We use a while loop to protect against spurious wakeups. */
	while (loader->running &&
	       loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		pthread_cond_wait(&loader->request_cond,
		                  &loader->request_mutex);
	}

	/* 2. Convert or Clean up */
	if (!loader->running ||
	    loader->current_request.state != ASYNC_CONVERTING) {
		/* Cancelled or failed during wait */
		async_cleanup_payload(loader);
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);
		/* Le unlock final est géré par la boucle du worker */
	} else {
		/* Convert (async_perform_conversion gère son propre
		 * unlock/lock) */
		async_perform_conversion(loader);
	}
}

/**
 * @brief Heavy-lifting disk IO loading phase.
 *
 * Loads the asset payload from disk into temporary CPU memory. This is executed
 * entirely outside the request_mutex to keep disk latency from blocking the
 * main thread.
 *
 * @param loader Pointer to the AsyncLoader instance.
 * @param path_to_load Absolute or relative path to the asset on disk.
 *
 * Concurrency constraints:
 * - PRECONDITION: request_mutex MUST NOT be held on entry (called without
 * lock).
 * - POSTCONDITION: request_mutex is HELD on exit.
 * - State on entry: ASYNC_LOADING.
 * - State on exit: ASYNC_WAITING_FOR_PBO (success) or ASYNC_FAILED (failure).
 */
static void async_handle_io(AsyncLoader* loader, const char* path_to_load)
{
	const AsyncBackendInterface* backend =
	    async_backend_get(loader->current_request.resource_type);

	if (!backend || !backend->load) {
		LOG_ERROR("suckless-ogl.async",
		          "No backend found for asset type: %d",
		          loader->current_request.resource_type);
		pthread_mutex_lock(&loader->request_mutex);
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);
		return;
	}

	/* Phase de chargement lourd HORS du mutex pour éviter de figer le
	 * thread de rendu */
	bool success = backend->load(path_to_load, &loader->current_request);

	pthread_mutex_lock(&loader->request_mutex);
	if (!success) {
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);
		LOG_ERROR("suckless-ogl.async", "Backend failed loading: %s",
		          path_to_load);
		return;
	}

	/* Le chargement I/O CPU a réussi. Nous passons à l'attente du PBO. */
	loader->current_request.state = ASYNC_WAITING_FOR_PBO;
	async_wait_and_convert(loader);
}

/**
 * @brief Thread entry point for the background worker.
 *
 * Handles the main execution loop, waiting passively for submitted requests,
 * coordinating IO and conversion task dispatches.
 *
 * @param arg Opaque pointer to the AsyncLoader instance.
 * @return NULL.
 */
static void* async_worker_func(void* arg)
{
	AsyncLoader* loader = (AsyncLoader*)arg;
	PROFILE_THREAD_NAME("Async Loader");

	pthread_mutex_lock(&loader->request_mutex);
	while (loader->running) {
		/* Wait passively for new work to be submitted */
		while (loader->running && !loader->has_pending_work) {
			pthread_cond_wait(&loader->request_cond,
			                  &loader->request_mutex);
		}

		if (!loader->running) {
			break;
		}

		/* Extract work details */
		char path_to_load[ASYNC_MAX_PATH];
		bool has_work = false;

		if (loader->current_request.state == ASYNC_PENDING) {
			(void)safe_snprintf(path_to_load, sizeof(path_to_load),
			                    "%s", loader->current_request.path);
			loader->current_request.state = ASYNC_LOADING;
			transition_tracy_state(ASYNC_LOADING);

			double now = perf_timer_elapsed_ms(&loader->sys_timer);
			double queue_time =
			    now - loader->current_request.submission_time;
			char msg[MSG_BUF_SIZE];
			int res =
			    safe_snprintf(msg, sizeof(msg),
			                  "Queuing delay: %.2f ms", queue_time);
			if (res >= 0) {
				PROFILE_MESSAGE(msg, (size_t)res);
			}
			has_work = true;
		}

		/* Unlock the mutex while executing heavy work (I/O, decryption,
		 * format decoding) so the main thread remains completely
		 * non-blocking. */
		pthread_mutex_unlock(&loader->request_mutex);

		if (has_work) {
			async_handle_io(loader, path_to_load);
			/* async_handle_io returns with mutex HELD in all
			 * paths (success, failure, cancel). No re-lock needed.
			 */
		} else {
			/* No work was dispatched, re-acquire lock for the next
			 * iteration check */
			pthread_mutex_lock(&loader->request_mutex);
		}
		loader->has_pending_work = false;
	}
	pthread_mutex_unlock(&loader->request_mutex);
	return NULL;
}

AsyncLoader* async_loader_create(struct TracyManager* mgr)
{
	/* Allocation dynamique du conteneur opaque du chargeur */
	AsyncLoader* loader = (AsyncLoader*)calloc(1, sizeof(AsyncLoader));
	if (!loader) {
		return NULL;
	}

	/* Initialisation de la requête dans un état inactif */
	loader->current_request.state = ASYNC_IDLE;

	/* Initialisation du mutex gérant tout l'automate d'états de la requête
	 */
	if (pthread_mutex_init(&loader->request_mutex, NULL) != 0) {
		free(loader);
		return NULL;
	}

	/* Initialisation de la variable de condition pour l'attente passive
	 * bidirectionnelle :
	 * 1. Le worker attend qu'une tâche soit soumise ou qu'un PBO soit
	 * fourni.
	 * 2. La destruction ou l'annulation réveille le worker. */
	if (pthread_cond_init(&loader->request_cond, NULL) != 0) {
		pthread_mutex_destroy(&loader->request_mutex);
		free(loader);
		return NULL;
	}

	loader->running = true;
	loader->tracy_mgr = mgr;
	perf_timer_start(&loader->sys_timer);
	transition_tracy_state(ASYNC_IDLE);

	/* Création du thread d'arrière-plan */
	if (pthread_create(&loader->worker_thread, NULL, async_worker_func,
	                   loader) != 0) {
		loader->running = false;
		pthread_cond_destroy(&loader->request_cond);
		pthread_mutex_destroy(&loader->request_mutex);
		free(loader);
		return NULL;
	}

	return loader;
}

void async_loader_destroy(AsyncLoader* loader)
{
	if (!loader) {
		return;
	}

	/* Signalement d'arrêt au thread worker */
	if (loader->running) {
		pthread_mutex_lock(&loader->request_mutex);
		loader->running = false;
		/* Réveille le worker s'il attendait du travail ou un PBO */
		pthread_cond_broadcast(&loader->request_cond);
		pthread_mutex_unlock(&loader->request_mutex);

		/* Attente bloquante de la fin du worker thread.
		 * Cela garantit que le worker n'écrira plus jamais dans la
		 * mémoire et qu'il a quitté sa boucle principale. */
		pthread_join(loader->worker_thread, NULL);
	}

	/* Nettoyage final sécurisé des allocations résiduelles du backend de
	 * format */
	async_cleanup_payload(loader);

	/* Libération des primitives de synchronisation */
	pthread_cond_destroy(&loader->request_cond);
	pthread_mutex_destroy(&loader->request_mutex);
	cleanup_tracy_states();
	free(loader);

	LOG_INFO("suckless-ogl.async", "Async loader destroyed.");
}

bool async_loader_request(AsyncLoader* loader, const AssetHandle* asset)
{
	if (!loader || !asset) {
		return false;
	}
	bool accepted = false;

	/* Verrouillage pour inspecter et modifier l'état de la requête */
	pthread_mutex_lock(&loader->request_mutex);

	/* Invariant : On n'accepte une nouvelle tâche que si le loader est
	 * inactif ou a fini (READY / FAILED) */
	if (loader->current_request.state == ASYNC_IDLE ||
	    loader->current_request.state == ASYNC_FAILED ||
	    loader->current_request.state == ASYNC_READY) {
		/* Libère les structures de la requête précédente si nécessaire
		 */
		async_cleanup_payload(loader);

		if (safe_snprintf(loader->current_request.path,
		                  sizeof(loader->current_request.path), "%s",
		                  asset->full_path) >= 0) {
			loader->current_request.resource_type = asset->type;
			loader->current_request.state = ASYNC_PENDING;
			loader->current_request.submission_time =
			    perf_timer_elapsed_ms(&loader->sys_timer);
			transition_tracy_state(ASYNC_PENDING);
			loader->has_pending_work = true;

			/* Réveille le worker qui attendait passivement du
			 * travail */
			pthread_cond_signal(&loader->request_cond);
			accepted = true;
		}
	}
	pthread_mutex_unlock(&loader->request_mutex);
	return accepted;
}

bool async_loader_poll(AsyncLoader* loader, AsyncRequest* out_req)
{
	if (!loader || !out_req) {
		return false;
	}
	bool result = false;

	/* Verrouillage requis car l'automate d'états est accédé en
	 * lecture/écriture par les deux threads */
	pthread_mutex_lock(&loader->request_mutex);

	if (loader->current_request.state == ASYNC_READY ||
	    loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		/* Copie de l'état actuel pour le thread principal */
		*out_req = loader->current_request;

		/* Si READY, la requête a été entièrement traitée. On repasse
		 * immédiatement à IDLE pour que le loader puisse accepter une
		 * nouvelle requête. Le cycle de vie des PBO est maintenant sous
		 * la responsabilité du thread de rendu principal. */
		if (loader->current_request.state == ASYNC_READY) {
			loader->current_request.state = ASYNC_IDLE;
			transition_tracy_state(ASYNC_IDLE);
			loader->current_request.backend_data = NULL;
			loader->current_request.pbo_mapped_ptr = NULL;
		}
		result = true;
	} else if (loader->current_request.state == ASYNC_FAILED) {
		/* En cas d'échec, on signale l'erreur au thread principal et on
		 * réinitialise l'état */
		*out_req = loader->current_request;
		loader->current_request.state = ASYNC_IDLE;
		transition_tracy_state(ASYNC_IDLE);
		result = true;
	}
	pthread_mutex_unlock(&loader->request_mutex);
	return result;
}

void async_loader_provide_pbo(AsyncLoader* loader, void* mapped_ptr,
                              GLuint pbo_id)
{
	if (!loader) {
		return;
	}
	pthread_mutex_lock(&loader->request_mutex);

	/* Invariant : On ne fournit le PBO que si le worker l'attend activement
	 */
	if (loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		loader->current_request.pbo_mapped_ptr = mapped_ptr;
		loader->current_request.pbo_id = pbo_id;
		loader->current_request.state = ASYNC_CONVERTING;
		transition_tracy_state(ASYNC_CONVERTING);

		/* Réveille le worker bloqué dans pthread_cond_wait de
		 * async_wait_and_convert */
		pthread_cond_signal(&loader->request_cond);
	}
	pthread_mutex_unlock(&loader->request_mutex);
}

void async_loader_cancel(AsyncLoader* loader)
{
	if (!loader) {
		return;
	}
	pthread_mutex_lock(&loader->request_mutex);

	/* Invariant : L'annulation n'est possible que si le worker attend le
	 * PBO. Si la conversion a commencé (ASYNC_CONVERTING), on ne peut plus
	 * annuler de cette façon afin d'éviter les corruptions de mémoire PBO
	 * en cours d'écriture. */
	if (loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);

		/* Réveille le worker pour qu'il sorte de sa boucle d'attente et
		 * procède au nettoyage */
		pthread_cond_signal(&loader->request_cond);
	}
	pthread_mutex_unlock(&loader->request_mutex);
}

#ifndef GL_COMMON_NO_GLFW
#include "app.h"
#include "app_profiling.h"

int async_loader_subsys_init(App* app)
{
	app->async_loader = async_loader_create(&app->profiling->tracy_mgr);
	return app->async_loader != NULL;
}

void async_loader_subsys_cleanup(App* app)
{
	async_loader_destroy(app->async_loader);
	app->async_loader = NULL;
}
#endif
