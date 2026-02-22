#include "tracy_manager.h"

void tracy_manager_init(TracyManager* mgr, int width, int height)
{
	(void)mgr;
	(void)width;
	(void)height;
}

void tracy_manager_cleanup(TracyManager* mgr)
{
	(void)mgr;
}

void tracy_manager_async_transition(TracyManager* mgr, AsyncState new_state)
{
	(void)mgr;
	(void)new_state;
}

void tracy_manager_async_end(TracyManager* mgr)
{
	(void)mgr;
}
