#include "effects/fx_lut3d.h"

#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum LUT3DConstants {
	MAX_LUT_LINE_LENGTH = 256,
	MIN_LUT_SIZE = 2,
	MAX_LUT_SIZE = 256,
	LUT3D_TEXTURE_UNIT = 8,
	LUT_SIZE_TOKEN_LEN = 11,
	LUT_SIZE_VALUE_OFFSET = 12
};

int fx_lut3d_init(PostProcess* post_processing)
{
	LUT3DFX* lut3d_sys = &post_processing->lut3d_fx;
	lut3d_sys->lut_tex = 0;
	lut3d_sys->current_size = 0;
	lut3d_sys->current_lut_idx = 0;
	return 0;
}

void fx_lut3d_cleanup(PostProcess* post_processing)
{
	LUT3DFX* lut3d_sys = &post_processing->lut3d_fx;
	if (lut3d_sys->lut_tex) {
		glDeleteTextures(1, &lut3d_sys->lut_tex);
		lut3d_sys->lut_tex = 0;
	}
}

static int parse_lut_line(const char* line, float* lut_data, int* entry_count,
                          int lut_size)
{
	/* Skip comments and empty lines */
	if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
		return 0;
	}

	if (strncmp(line, "LUT_3D_SIZE", LUT_SIZE_TOKEN_LEN) == 0) {
		return 1; /* Already handled or needs special handling */
	}

	/* Parse RGB data */
	if (lut_data && *entry_count < lut_size * lut_size * lut_size) {
		char* next = NULL;
		float red = strtof(line, &next);
		if (next == line) {
			return 0;
		}
		float green = strtof(next, &next);
		float blue = strtof(next, NULL);

		lut_data[(*entry_count * 3) + 0] = red;
		lut_data[(*entry_count * 3) + 1] = green;
		lut_data[(*entry_count * 3) + 2] = blue;
		(*entry_count)++;
	}
	return 0;
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
		if (strncmp(line, "LUT_3D_SIZE", LUT_SIZE_TOKEN_LEN) == 0) {
			char* endptr = NULL;
			const int base_decimal = 10;
			int new_size = (int)strtol(line + LUT_SIZE_VALUE_OFFSET,
			                           &endptr, base_decimal);
			(void)endptr;

			if (new_size < MIN_LUT_SIZE ||
			    new_size > MAX_LUT_SIZE) {
				LOG_ERROR("suckless-ogl.postprocess.lut3d",
				          "Invalid LUT size: %d", new_size);
				if (lut_data) {
					free(lut_data);
				}
				(void)fclose(cube_file);
				return -2;
			}

			if (lut_data) {
				free(lut_data);
			}
			lut_size = new_size;
			lut_data = (float*)malloc((size_t)lut_size * lut_size *
			                          lut_size * 3 * sizeof(float));
			if (!lut_data) {
				(void)fclose(cube_file);
				return -4;
			}
			continue;
		}

		(void)parse_lut_line(line, lut_data, &entry_count, lut_size);
	}

	(void)fclose(cube_file);

	if (entry_count != lut_size * lut_size * lut_size || !lut_data) {
		LOG_ERROR("suckless-ogl.postprocess.lut3d", "LUT mismatch");
		if (lut_data) {
			free(lut_data);
		}
		return -3;
	}

	LUT3DFX* lut3d_sys = &post_processing->lut3d_fx;
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

	free(lut_data);
	lut3d_sys->current_size = lut_size;
	post_processing->lut3d.texture = lut3d_sys->lut_tex;
	post_processing->lut3d.size = lut_size;

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
