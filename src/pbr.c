#include "pbr.h"

#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <math.h>
#include <stddef.h>  // Fournit NULL (proprement)
#include <stdint.h>  // Fournit uint32_t

// Constantes pour éviter les "Magic Numbers" et faciliter le linting
static const uint32_t COMPUTE_GROUP_SIZE_PBR = 32;
static const uint32_t COMPUTE_GROUP_SIZE_LUM = 16;

GLuint build_prefiltered_specular_map(GLuint env_hdr_tex, int width, int height,
                                      float threshold)
{
	int levels = (int)floor(log2(fmax((double)width, (double)height))) + 1;

	GLuint spec_tex = 0;
	glGenTextures(1, &spec_tex);
	glBindTexture(GL_TEXTURE_2D, spec_tex);

	glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA16F, width, height);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLuint shader = shader_load_compute("shaders/IBL/spmap.glsl");
	glUseProgram(shader);

	/* Explicitly set envMap sampler to unit 0 (fix for Intel/Mesa) */
	GLint u_env_map = glGetUniformLocation(shader, "envMap");
	if (u_env_map >= 0) {
		glUniform1i(u_env_map, 0);
	}

	for (int level = 0; level < levels; level++) {
		// Utilisation de uint32_t pour éviter les warnings sur les
		// bitwise
		uint32_t mip_w = (uint32_t)width >> (uint32_t)level;
		uint32_t mip_h = (uint32_t)height >> (uint32_t)level;

		if (mip_w < 1) {
			mip_w = 1;
		}
		if (mip_h < 1) {
			mip_h = 1;
		}

		float roughness = (float)level / (float)(levels - 1);
		glUniform1f(0, roughness);
		/* Location 2 correspond à clampThreshold ajouté dans le shader
		 */
		glUniform1f(2, threshold);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, env_hdr_tex);

		glBindImageTexture(1, spec_tex, level, GL_FALSE, 0,
		                   GL_WRITE_ONLY, GL_RGBA16F);

		// Correction mw/mh -> mip_w/mip_h
		uint32_t groups_x = (mip_w + (COMPUTE_GROUP_SIZE_PBR - 1)) /
		                    COMPUTE_GROUP_SIZE_PBR;
		uint32_t groups_y = (mip_h + (COMPUTE_GROUP_SIZE_PBR - 1)) /
		                    COMPUTE_GROUP_SIZE_PBR;

		glDispatchCompute(groups_x, groups_y, 1);
		glMemoryBarrier(GL_ALL_BARRIER_BITS);

		LOG_INFO("IBL", "Prefiltered level %d (%ux%u) roughness %.2f",
		         level, mip_w, mip_h, roughness);
	}

	glUseProgram(0);
	glDeleteProgram(shader);
	return spec_tex;
}

GLuint build_irradiance_map(GLuint env_hdr_tex, int size, float threshold)
{
	GLuint irr_tex = 0;
	glGenTextures(1, &irr_tex);
	glBindTexture(GL_TEXTURE_2D, irr_tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, size, size);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLuint shader = shader_load_compute("shaders/IBL/irmap.glsl");
	if (shader == 0) {
		return 0;
	}

	glUseProgram(shader);
	glUniform1f(glGetUniformLocation(shader, "clamp_threshold"), threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, env_hdr_tex);
	glBindImageTexture(1, irr_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY,
	                   GL_RGBA16F);

	uint32_t groups = ((uint32_t)size + (COMPUTE_GROUP_SIZE_PBR - 1)) /
	                  COMPUTE_GROUP_SIZE_PBR;
	glDispatchCompute(groups, groups, 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	LOG_INFO("suckless-ogl.ibl", "Irradiance Map finished (%dx%d)", size,
	         size);

	glUseProgram(0);
	glDeleteProgram(shader);
	return irr_tex;
}

float compute_mean_luminance_gpu(GLuint hdr_tex, int width, int height,
                                 float clamp_multiplier)
{
	uint32_t group_x = ((uint32_t)width + (COMPUTE_GROUP_SIZE_LUM - 1)) /
	                   COMPUTE_GROUP_SIZE_LUM;
	uint32_t group_y = ((uint32_t)height + (COMPUTE_GROUP_SIZE_LUM - 1)) /
	                   COMPUTE_GROUP_SIZE_LUM;
	uint32_t num_groups = group_x * group_y;
	uint32_t num_pixels = (uint32_t)width * (uint32_t)height;

	GLuint ssbos[2] = {0, 0};
	glGenBuffers(2, ssbos);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[0]);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
	             (GLsizeiptr)(num_groups * sizeof(float)), NULL,
	             GL_STREAM_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[1]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)sizeof(float), NULL,
	             GL_STREAM_READ);

	GLuint prog1 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass1.glsl");
	if (prog1 == 0) {
		/* possibilité de logger une erreur */
		LOG_ERROR("suckless-ogl.ibl", "Failed to load compute shader");
		glDeleteBuffers(2, ssbos);
		return 0.0F;
	}

	glUseProgram(prog1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdr_tex);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbos[0]);

	glDispatchCompute(group_x, group_y, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	GLuint prog2 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass2.glsl");
	if (prog2 == 0) {
		/* possibilité de logger une erreur */
		LOG_ERROR("suckless-ogl.ibl", "Failed to load compute shader");
		glDeleteBuffers(2, ssbos);
		glDeleteProgram(prog1);
		return 0.0F;
	}

	glUseProgram(prog2);
	glUniform1ui(glGetUniformLocation(prog2, "numGroups"), num_groups);
	glUniform1ui(glGetUniformLocation(prog2, "numPixels"), num_pixels);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbos[0]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbos[1]);

	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	float mean = 0.0F;
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[1]);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float), &mean);

	glDeleteBuffers(2, ssbos);
	glDeleteProgram(prog1);  // Nettoyage propre
	glDeleteProgram(prog2);  // Nettoyage propre

	if (isinf(mean) || isnan(mean)) {
		mean = 0.0F;
	}

	return mean * clamp_multiplier;
}

GLuint build_brdf_lut_map(int size)
{
	GLuint brdf_tex = 0;  // Initialisé
	glGenTextures(1, &brdf_tex);
	glBindTexture(GL_TEXTURE_2D, brdf_tex);

	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16F, size, size);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLuint shader = shader_load_compute("shaders/IBL/spbrdf.glsl");
	if (shader == 0) {
		return 0;
	}

	glUseProgram(shader);
	glBindImageTexture(0, brdf_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY,
	                   GL_RG16F);

	uint32_t groups = ((uint32_t)size + (COMPUTE_GROUP_SIZE_PBR - 1)) /
	                  COMPUTE_GROUP_SIZE_PBR;
	glDispatchCompute(groups, groups, 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	LOG_INFO("suckless-ogl.ibl", "BRDF LUT generated: %dx%d", size, size);

	glUseProgram(0);
	glDeleteProgram(shader);

	return brdf_tex;
}
