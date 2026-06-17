// tests/test_async_loader_cancel.c
#include "asset_manager.h"
#define _POSIX_C_SOURCE 200809L
#include "async_loader.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

static const long MS_MULTIPLIER = 1000L;
static AsyncLoader* loader;
static const char* TEMP_FILENAME = "test_cancel_img.ppm";

static void sleep_ms(long milliseconds)
{
	struct timespec req;
	req.tv_sec = milliseconds / MS_MULTIPLIER;
	req.tv_nsec =
	    (milliseconds % MS_MULTIPLIER) * MS_MULTIPLIER * MS_MULTIPLIER;
	nanosleep(&req, NULL);
}

/* Helper pour forger un handle dans les tests sans dépendre de io/fs */
static AssetHandle make_test_handle(const char* path, AssetType type)
{
	AssetHandle asset_handler = {0};
	strncpy(asset_handler.full_path, path,
	        sizeof(asset_handler.full_path) - 1);
	asset_handler.type = type;
	return asset_handler;
}

void setUp(void)
{
	loader = async_loader_create(NULL);
	// Create a valid dummy PPM file
	FILE* f = fopen(TEMP_FILENAME, "wb");
	if (f) {
		fprintf(f, "P6\n1 1\n255\n");
		unsigned char pixel[] = {255, 0, 0};
		fwrite(pixel, 1, 3, f);
		fclose(f);
	}
}

void tearDown(void)
{
	async_loader_destroy(loader);
	remove(TEMP_FILENAME);
}

void test_cancel_request(void)
{
	// 1. Submit request
	AssetHandle handle =
	    make_test_handle(TEMP_FILENAME, ASSET_TYPE_TEXTURE_STB);
	bool accepted = async_loader_request(loader, &handle);
	TEST_ASSERT_TRUE(accepted);

	// 2. Wait until it reaches ASYNC_WAITING_FOR_PBO
	int attempts = 0;
	bool waiting = false;
	const int MAX_ATTEMPTS = 100;
	const int POLL_INTERVAL_MS = 10;
	AsyncRequest req = {0};

	while (attempts < MAX_ATTEMPTS) {
		if (async_loader_poll(loader, &req)) {
			// Poll returned true means we got a result.
			// But we expect it to be waiting for PBO, which returns
			// true with state ASYNC_WAITING_FOR_PBO.
			if (req.state == ASYNC_WAITING_FOR_PBO) {
				waiting = true;
				break;
			}
		}
		sleep_ms(POLL_INTERVAL_MS);
		attempts++;
	}

	TEST_ASSERT_TRUE_MESSAGE(waiting,
	                         "Loader did not reach WAITING_FOR_PBO state");

	// 3. Cancel the request
	// Note: async_loader_cancel is not yet implemented, but we expect it to
	// exist based on our plan. Compilation will fail until we implement it.
	async_loader_cancel(loader);

	// 4. Verify it transitions to FAILED (and then IDLE upon poll)
	// After cancel, the worker should wake up, clean up, set state to
	// FAILED. The next poll should return the FAILED state or IDLE.
	// Actually, async_loader_poll returns true if state is READY or
	// WAITING_FOR_PBO. If state is FAILED, it returns false but logs error
	// and resets to IDLE. Wait, let's check async_loader_poll
	// implementation again.

	/*
	} else if (loader->current_request.state == ASYNC_FAILED) {
	        // Failed loading, just reset
	        LOG_ERROR(..., "Async load failed for: %s", ...);
	        loader->current_request.state = ASYNC_IDLE;
	        transition_tracy_state(ASYNC_IDLE);
	}
	*/
	// It returns false if FAILED. But it resets state to IDLE.

	// So we wait a bit for worker to process cancellation
	sleep_ms(50);

	// Now polling should return true to notify the caller of the event,
	// and the request state should explicitly be ASYNC_FAILED.
	AsyncRequest dummy_req = {0};
	bool res = async_loader_poll(loader, &dummy_req);
	TEST_ASSERT_TRUE_MESSAGE(
	    res, "Le poll doit signaler l'événement d'annulation/échec");
	TEST_ASSERT_EQUAL_INT(ASYNC_FAILED, dummy_req.state);

	// 5. Verify we can submit a new request
	handle = make_test_handle(TEMP_FILENAME, ASSET_TYPE_TEXTURE_STB);
	accepted = async_loader_request(loader, &handle);
	TEST_ASSERT_TRUE_MESSAGE(
	    accepted, "Should be able to submit new request after cancel");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_cancel_request);
	return UNITY_END();
}
