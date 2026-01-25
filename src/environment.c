#include "environment.h"

#include "app_settings.h"
#include "gl_common.h"
#include "glad/glad.h"
#include "log.h"
#include "pbr.h"
#include "texture.h"
#include "utils.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_strings(const void* string_a, const void* string_b)
{
	return strcmp(*(const char**)string_a, *(const char**)string_b);
}

void environment_init(Environment* env)
{
	env->hdr_files = NULL;
	env->hdr_count = 0;
	env->current_hdr_index = -1;
	env->hdr_texture = 0;
	env->spec_prefiltered_tex = 0;
	env->irradiance_tex = 0;
	env->brdf_lut_tex = 0;
	env->auto_threshold = DEFAULT_AUTO_THRESHOLD;
}

void environment_scan_files(Environment* env, const char* directory)
{
	env->hdr_count = 0;
	env->hdr_files = NULL;
	env->current_hdr_index = -1;

	DIR* dir_handle = opendir(directory);
	if (dir_handle) {
		struct dirent* entry = NULL;
		while ((entry = readdir(dir_handle)) != NULL) {
			char* dot = strrchr(entry->d_name, '.');
			if (dot && strcmp(dot, ".hdr") == 0) {
				env->hdr_count++;
				env->hdr_files = realloc(
				    env->hdr_files,
				    (size_t)env->hdr_count * sizeof(char*));
				env->hdr_files[env->hdr_count - 1] =
				    strdup(entry->d_name);
			}
		}
		closedir(dir_handle);

		if (env->hdr_count > 1) {
			qsort(env->hdr_files, (size_t)env->hdr_count,
			      sizeof(char*), compare_strings);
		}
	} else {
		LOG_ERROR("suckless-ogl.env", "Failed to open %s directory!",
		          directory);
	}
	LOG_INFO("suckless-ogl.env", "Found %d HDR files.", env->hdr_count);
}

int environment_load(Environment* env, const char* filename)
{
	char path[MAX_PATH_LENGTH];
	(void)safe_snprintf(path, sizeof(path), "assets/textures/hdr/%s",
	                    filename);

	int hdr_w = 0;
	int hdr_h = 0;

	/* Cleanup old textures if simple reload */
	if (env->hdr_texture) {
		glDeleteTextures(1, &env->hdr_texture);
	}
	if (env->spec_prefiltered_tex) {
		glDeleteTextures(1, &env->spec_prefiltered_tex);
	}
	if (env->irradiance_tex) {
		glDeleteTextures(1, &env->irradiance_tex);
	}

	env->hdr_texture = texture_load_hdr(path, &hdr_w, &hdr_h);
	if (!env->hdr_texture) {
		LOG_ERROR("suckless-ogl.env", "Failed to load HDR texture: %s",
		          path);
		return 0;
	}

	float auto_threshold = compute_mean_luminance_gpu(
	    env->hdr_texture, hdr_w, hdr_h, DEFAULT_CLAMP_MULTIPLIER);

	if (auto_threshold < 1.0F || isnan(auto_threshold) ||
	    isinf(auto_threshold)) {
		auto_threshold = DEFAULT_AUTO_THRESHOLD;
	}

	env->auto_threshold = auto_threshold;

	env->spec_prefiltered_tex = build_prefiltered_specular_map(
	    env->hdr_texture, PREFILTERED_SPECULAR_MAP_SIZE,
	    PREFILTERED_SPECULAR_MAP_SIZE, auto_threshold);

	env->irradiance_tex = build_irradiance_map(
	    env->hdr_texture, IRIDIANCE_MAP_SIZE, auto_threshold);

	LOG_INFO("suckless-ogl.env", "Loaded Environment: %s (Thresh: %.2f)",
	         filename, auto_threshold);

	return 1;
}

void environment_cleanup(Environment* env)
{
	if (env->hdr_texture) {
		glDeleteTextures(1, &env->hdr_texture);
	}
	if (env->spec_prefiltered_tex) {
		glDeleteTextures(1, &env->spec_prefiltered_tex);
	}
	if (env->irradiance_tex) {
		glDeleteTextures(1, &env->irradiance_tex);
	}
	if (env->brdf_lut_tex) {
		glDeleteTextures(1, &env->brdf_lut_tex);
	}

	for (int i = 0; i < env->hdr_count; i++) {
		free(env->hdr_files[i]);
	}
	free(env->hdr_files);
	env->hdr_files = NULL;
	env->hdr_count = 0;
}
