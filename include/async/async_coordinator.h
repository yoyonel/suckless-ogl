#ifndef ASYNC_COORDINATOR_H
#define ASYNC_COORDINATOR_H

#include "async_loader.h"

typedef struct AsyncCoordinator {
	GLuint upload_pbo[2];
	int upload_pbo_idx;
	GLsizeiptr upload_pbo_size[2];
	int pending_prealloc_w;
	int pending_prealloc_h;
} AsyncCoordinator;

void async_coordinator_init(AsyncCoordinator* coord);
void async_coordinator_cleanup(AsyncCoordinator* coord);

/**
 * Handles communication between the AsyncLoader (background thread) and the
 * main thread for tasks like mapping PBOs. Returns true if an async request
 * is fully ready to be consumed by the application.
 */
bool async_coordinator_update(AsyncCoordinator* coord, AsyncLoader* loader,
                              AsyncRequest* out_req);

/* --- Subsystem descriptor (alloc-only Phase 1) --- */
#include "app_subsystem.h"

int async_coord_subsys_init(struct App* app);
void async_coord_subsys_cleanup(struct App* app);

#define APP_ASYNC_COORD_DESCRIPTOR \
	{"async_coord", async_coord_subsys_init, async_coord_subsys_cleanup}

#endif /* ASYNC_COORDINATOR_H */
