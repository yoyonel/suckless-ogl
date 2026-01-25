#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "gl_common.h"

typedef struct {
	/* Array of filenames */
	char** hdr_files;
	/* Total number of HDR files */
	int hdr_count;
	/* Index of currently loaded HDR */
	int current_hdr_index;

	/* Environment textures */
	GLuint hdr_texture;
	GLuint spec_prefiltered_tex;
	GLuint irradiance_tex;
	GLuint brdf_lut_tex;

	/* Auto-computed threshold for IBL */
	float auto_threshold;
} Environment;

/* Initialize environment struct */
void environment_init(Environment* env);

/* Scan directory for HDR files */
void environment_scan_files(Environment* env, const char* directory);

/* Load a specific HDR environment map */
int environment_load(Environment* env, const char* filename);

/* Cleanup environment resources */
void environment_cleanup(Environment* env);

#endif /* ENVIRONMENT_H */
