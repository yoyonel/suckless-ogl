#ifndef ASYNC_LOADER_H
#define ASYNC_LOADER_H

#include <stdbool.h>

/* Max path length for requests */
#define ASYNC_MAX_PATH 256

/* States for an async request */
typedef enum {
	ASYNC_IDLE = 0,
	ASYNC_PENDING,
	ASYNC_LOADING,
	ASYNC_READY,
	ASYNC_FAILED
} AsyncState;

/* Result structure holding the loaded data */
typedef struct {
	char path[ASYNC_MAX_PATH];
	float* data;
	int width;
	int height;
	int channels;
	volatile AsyncState state;
} AsyncRequest;

/* Initialize the async loader worker thread */
void async_loader_init(void);

/* Shutdown the async loader worker thread */
void async_loader_shutdown(void);

/* Request an HDR file to be loaded. Returns true if request accepted. */
bool async_loader_request(const char* path);

/* Poll for a completed request. Returns true if a request is ready.
 * The 'out_request' struct is filled with valid data (ensure to free 'data').
 * Should be called from the main thread.
 */
bool async_loader_poll(AsyncRequest* out_request);

#endif /* ASYNC_LOADER_H */
