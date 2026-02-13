#include "mock_stb_image_standalone.h"

#include <malloc.h>
#include <stdio.h>

#define DEFAULT_INFO_WIDTH 10
#define DEFAULT_INFO_HEIGHT 10
#define DEFAULT_INFO_CHANNELS 4
#define HUGE_DIMENSION 9000

static int g_simulate_toctou = 0;
static int g_info_width = DEFAULT_INFO_WIDTH;
static int g_info_height = DEFAULT_INFO_HEIGHT;
static int g_info_channels = DEFAULT_INFO_CHANNELS;

void mock_stbi_set_toctou_simulation(int enable)
{
	g_simulate_toctou = enable;
}

void mock_stbi_set_info_dimensions(int width, int height, int channels)
{
	g_info_width = width;
	g_info_height = height;
	g_info_channels = channels;
}

/* -------------------------------------------------------------------------- */
/*                            MOCK IMPLEMENTATIONS                            */
/* -------------------------------------------------------------------------- */

int stbi_info_from_file(FILE* f, int* x, int* y, int* comp)
{
	(void)f;
	if (x) {
		*x = g_info_width;
	}
	if (y) {
		*y = g_info_height;
	}
	if (comp) {
		*comp = g_info_channels;
	}
	return 1; /* Success */
}

float* stbi_loadf_from_file(FILE* f, int* x, int* y, int* channels_in_file,
                            int desired_channels)
{
	(void)f;
	(void)channels_in_file;
	(void)desired_channels;

	if (g_simulate_toctou) {
		/* Return HUGE dimensions (TOCTOU simulation) */
		/* MAX_TEXTURE_DIMENSION is 8192 usually */
		if (x) {
			*x = HUGE_DIMENSION;
		}
		if (y) {
			*y = HUGE_DIMENSION;
		}
	} else {
		/* Return consistent dimensions */
		if (x) {
			*x = g_info_width;
		}
		if (y) {
			*y = g_info_height;
		}
	}

	/* Allocate dummy data */
	float* data = (float*)malloc(sizeof(float) * 4);
	return data;
}

void stbi_image_free(void* retval_from_stbi_load)
{
	free(retval_from_stbi_load);
}
