#include "effects/fx_lut3d.h"

#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LUT_LINE_LENGTH 256
#define MIN_LUT_SIZE 2
#define MAX_LUT_SIZE 256
#define LUT3D_TEXTURE_UNIT 8
#define LUT_SIZE_TOKEN_LEN 11
#define LUT_SIZE_VALUE_OFFSET 12

int fx_lut3d_init(PostProcess* post_processing)
{
	LUT3DFX* lut3d_sys = &post_processing->lut3d_fx;
	lut3d_sys->lut_tex = 0;
	lut3d_sys->current_size = 0;
	return 1;
}

void fx_lut3d_cleanup(PostProcess* post_processing)
{
	LUT3DFX* lut3d_sys = &post_processing->lut3d_fx;
	if (lut3d_sys->lut_tex) {
		glDeleteTextures(1, &lut3d_sys->lut_tex);
		lut3d_sys->lut_tex = 0;
	}
}

int fx_lut3d_load_cube(PostProcess* post_processing, const char* path)
{
	FILE* cube_file = fopen(path, "r");
	if (!cube_file) {
		LOG_ERROR("suckless-ogl.postprocess.lut3d",
		          "Failed to open LUT file: %s", path);
		return -1;
	}

	char line[MAX_LUT_LINE_LENGTH];
	int lut_size = 0;
	float* lut_data = NULL;
	int entry_count = 0;

	while (fgets(line, sizeof(line), cube_file)) {
		/* Skip comments and empty lines */
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}

		if (strncmp(line, "LUT_3D_SIZE", LUT_SIZE_TOKEN_LEN) == 0) {
			char* endptr = NULL;
			const int base_decimal = 10;
			lut_size = (int)strtol(line + LUT_SIZE_VALUE_OFFSET,
			                       &endptr, base_decimal);
			(void)endptr;
			if (lut_size < MIN_LUT_SIZE ||
			    lut_size > MAX_LUT_SIZE) {
				LOG_ERROR("suckless-ogl.postprocess.lut3d",
				          "Invalid LUT size: %d", lut_size);
				(void)fclose(cube_file);
				return -2;
			}
			lut_data = (float*)malloc(
			    (size_t)lut_size * (size_t)lut_size *
			    (size_t)lut_size * 3 * sizeof(float));
			continue;
		}

		/* Parse RGB data */
		if (lut_data && entry_count < lut_size * lut_size * lut_size) {
			char* next = NULL;
			float red = strtof(line, &next);
			float green = strtof(next, &next);
			float blue = strtof(next, NULL);
			(void)next;

			lut_data[(entry_count * 3) + 0] = red;
			lut_data[(entry_count * 3) + 1] = green;
			lut_data[(entry_count * 3) + 2] = blue;
			entry_count++;
		}
	}

	if (entry_count != lut_size * lut_size * lut_size) {
		LOG_ERROR("suckless-ogl.postprocess.lut3d",
		          "LUT data mismatch: expected %d, got %d",
		          lut_size * lut_size * lut_size, entry_count);
		if (lut_data) {
			free(lut_data);
		}
		(void)fclose(cube_file);
		return -3;
	}

	(void)fclose(cube_file);

	LUT3DFX* lut3d_sys = &post_processing->lut3d_fx;

	/* Create GPU texture */
	if (lut3d_sys->lut_tex) {
		glDeleteTextures(1, &lut3d_sys->lut_tex);
	}

	glGenTextures(1, &lut3d_sys->lut_tex);
	glBindTexture(GL_TEXTURE_3D, lut3d_sys->lut_tex);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, lut_size, lut_size, lut_size,
	             0, GL_RGB, GL_FLOAT, lut_data);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_3D, 0);

	free(lut_data);

	lut3d_sys->current_size = lut_size;
	post_processing->lut3d.texture = lut3d_sys->lut_tex;
	post_processing->lut3d.size = lut_size;

	LOG_INFO("suckless-ogl.postprocess.lut3d",
	         "Loaded 3D LUT from %s (Size: %d^3)", path, lut_size);

	return 0;
}

void fx_lut3d_upload_params(Shader* shader, const LUT3DParams* params)
{
	(void)shader;
	/* lut3d_intensity is handled via UBO */
	if (params->texture) {
		glActiveTexture(GL_TEXTURE0 + LUT3D_TEXTURE_UNIT);
		glBindTexture(GL_TEXTURE_3D, params->texture);
	}
}
