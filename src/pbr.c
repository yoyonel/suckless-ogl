#include "pbr.h"

#include "gl_common.h"
#include "perf_timer.h"
#include "shader.h"
#include <math.h>
#include <stddef.h>  // Fournit NULL (proprement)
#include <stdint.h>  // Fournit uint32_t

// Constantes pour éviter les "Magic Numbers" et faciliter le linting
static const uint32_t COMPUTE_GROUP_SIZE_PBR = 32;
static const uint32_t COMPUTE_GROUP_SIZE_LUM = 16;

GLuint pbr_prefilter_init(int width, int height)
{
	GLuint spec_tex = 0;
	int levels = (int)floor(log2(fmax((double)width, (double)height))) + 1;

	glGenTextures(1, &spec_tex);
	glBindTexture(GL_TEXTURE_2D, spec_tex);
	glObjectLabel(GL_TEXTURE, spec_tex, -1, "Prefiltered Specular Map");

	glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA16F, width, height);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	return spec_tex;
}

void pbr_prefilter_mip(GLuint shader, GLuint env_hdr_tex, GLuint dest_tex,
                       int width, int height, int level, int total_levels,
                       int slice_index, int total_slices, float threshold)
{
	if (shader == 0 || dest_tex == 0) {
		return;
	}

	GL_SCOPE_USE_PROGRAM(shader);

	/* Set uniforms */
	GLint u_env_map = glGetUniformLocation(shader, "envMap");
	if (u_env_map >= 0) {
		glUniform1i(u_env_map, 0);
	}

	GLint u_roughness = glGetUniformLocation(shader, "roughnessValue");
	GLint u_mip = glGetUniformLocation(shader, "currentMipLevel");
	GLint u_threshold = glGetUniformLocation(shader, "clampThreshold");
	GLint u_offset_y = glGetUniformLocation(shader, "u_offset_y");
	GLint u_max_y = glGetUniformLocation(shader, "u_max_y");

	uint32_t mip_w = (uint32_t)width >> (uint32_t)level;
	uint32_t mip_h = (uint32_t)height >> (uint32_t)level;

	if (mip_w < 1) {
		mip_w = 1;
	}
	if (mip_h < 1) {
		mip_h = 1;
	}

	float roughness = (float)level / (float)(total_levels - 1);
	if (u_roughness >= 0) {
		glUniform1f(u_roughness, roughness);
	}
	if (u_mip >= 0) {
		glUniform1i(u_mip, level);
	}
	if (u_threshold >= 0) {
		glUniform1f(u_threshold, threshold);
	}

	int lines_per_slice = ((int)mip_h + total_slices - 1) / total_slices;
	int y_start = slice_index * lines_per_slice;
	int y_end = y_start + lines_per_slice;
	if (y_end > (int)mip_h) {
		y_end = (int)mip_h;
	}
	int actual_lines = y_end - y_start;

	if (actual_lines <= 0) {
		return;
	}

	if (u_offset_y >= 0) {
		glUniform1i(u_offset_y, y_start);
	}
	if (u_max_y >= 0) {
		glUniform1i(u_max_y, y_end);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, env_hdr_tex);

	glBindImageTexture(1, dest_tex, level, GL_FALSE, 0, GL_WRITE_ONLY,
	                   GL_RGBA16F);

	uint32_t groups_x =
	    (mip_w + (COMPUTE_GROUP_SIZE_PBR - 1)) / COMPUTE_GROUP_SIZE_PBR;
	uint32_t groups_y =
	    ((uint32_t)actual_lines + (COMPUTE_GROUP_SIZE_PBR - 1)) /
	    COMPUTE_GROUP_SIZE_PBR;

	glDispatchCompute(groups_x, groups_y, 1);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint build_prefiltered_specular_map(GLuint shader, GLuint env_hdr_tex,
                                      int width, int height, float threshold)
{
	if (shader == 0) {
		return 0;
	}

	HYBRID_FUNC_TIMER("IBL: Prefiltered Specular Map");
	GL_SCOPE_DEBUG_GROUP("IBL: Prefiltered Specular Map");

	GLuint spec_tex = pbr_prefilter_init(width, height);
	int levels = (int)floor(log2(fmax((double)width, (double)height))) + 1;

	for (int level = 0; level < levels; level++) {
		pbr_prefilter_mip(shader, env_hdr_tex, spec_tex, width, height,
		                  level, levels, 0, 1, threshold);
	}

	return spec_tex;
}

GLuint pbr_irradiance_init(int size)
{
	GLuint irr_tex = 0;
	glGenTextures(1, &irr_tex);
	glBindTexture(GL_TEXTURE_2D, irr_tex);
	glObjectLabel(GL_TEXTURE, irr_tex, -1, "Irradiance Map");
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, size, size);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	return irr_tex;
}

void pbr_irradiance_slice_compute(GLuint shader, GLuint env_hdr_tex,
                                  GLuint dest_tex, int size, int slice_index,
                                  int total_slices, float threshold)
{
	if (shader == 0 || dest_tex == 0 || total_slices <= 0) {
		return;
	}

	GL_SCOPE_USE_PROGRAM(shader);
	GLint u_threshold = glGetUniformLocation(shader, "clamp_threshold");
	if (u_threshold >= 0) {
		glUniform1f(u_threshold, threshold);
	}

	GLint u_offset_y = glGetUniformLocation(shader, "u_offset_y");
	int lines_per_slice = (size + total_slices - 1) / total_slices;
	int y_start = slice_index * lines_per_slice;
	int y_end = y_start + lines_per_slice;
	if (y_end > size) {
		y_end = size;
	}
	int actual_lines = y_end - y_start;

	if (actual_lines <= 0) {
		return;
	}

	GLint u_max_y = glGetUniformLocation(shader, "u_max_y");
	if (u_max_y >= 0) {
		glUniform1i(u_max_y, y_end);
	}

	if (u_offset_y >= 0) {
		glUniform1i(u_offset_y, y_start);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, env_hdr_tex);
	glBindImageTexture(1, dest_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY,
	                   GL_RGBA16F);

	int groups_x = (size + (int)COMPUTE_GROUP_SIZE_PBR - 1) /
	               (int)COMPUTE_GROUP_SIZE_PBR;
	int groups_y = (actual_lines + (int)COMPUTE_GROUP_SIZE_PBR - 1) /
	               (int)COMPUTE_GROUP_SIZE_PBR;

	glDispatchCompute(groups_x, groups_y, 1);
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

GLuint build_irradiance_map(GLuint shader, GLuint env_hdr_tex, int size,
                            float threshold)
{
	if (shader == 0) {
		return 0;
	}

	GLuint irr_tex = 0;
	HYBRID_FUNC_TIMER("IBL: Irradiance Map");
	GL_SCOPE_DEBUG_GROUP("IBL: Irradiance Map");

	glGenTextures(1, &irr_tex);
	glBindTexture(GL_TEXTURE_2D, irr_tex);
	glObjectLabel(GL_TEXTURE, irr_tex, -1, "Irradiance Map");
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, size, size);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	{
		GL_SCOPE_USE_PROGRAM(shader);
		GLint u_threshold =
		    glGetUniformLocation(shader, "clamp_threshold");
		if (u_threshold >= 0) {
			glUniform1f(u_threshold, threshold);
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, env_hdr_tex);
		glBindImageTexture(1, irr_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY,
		                   GL_RGBA16F);

		uint32_t groups =
		    ((uint32_t)size + (COMPUTE_GROUP_SIZE_PBR - 1)) /
		    COMPUTE_GROUP_SIZE_PBR;
		glDispatchCompute(groups, groups, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	return irr_tex;
}

float compute_mean_luminance_gpu(GLuint shader_pass1, GLuint shader_pass2,
                                 GLuint hdr_tex, int width, int height,
                                 float clamp_multiplier)
{
	if (shader_pass1 == 0 || shader_pass2 == 0) {
		return 0.0F;
	}

	float mean = 0.0F;
	HYBRID_FUNC_TIMER("IBL: Luminance Reduction");
	GL_SCOPE_DEBUG_GROUP("IBL: Luminance Reduction");

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
	             GL_STREAM_READ);
	glObjectLabel(GL_BUFFER, ssbos[0], -1, "Luminance Reduct. (Step 1)");

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[1]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float), NULL,
	             GL_STREAM_READ);
	glObjectLabel(GL_BUFFER, ssbos[1], -1, "Luminance Reduct. (Step 2)");

	/* Pass 1: Initial reduction (no uniforms needed by pass1 shader) */
	{
		GL_SCOPE_USE_PROGRAM(shader_pass1);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, hdr_tex);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbos[0]);

		glDispatchCompute(group_x, group_y, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	/* Pass 2: Final reduction to single float */
	{
		GL_SCOPE_USE_PROGRAM(shader_pass2);
		GLint u_numGroups =
		    glGetUniformLocation(shader_pass2, "numGroups");
		if (u_numGroups >= 0) {
			glUniform1ui(u_numGroups, num_groups);
		}
		GLint u_numPixels =
		    glGetUniformLocation(shader_pass2, "numPixels");
		if (u_numPixels >= 0) {
			glUniform1ui(u_numPixels, num_pixels);
		}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbos[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbos[1]);

		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

		/* Fetch result */
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[1]);
		float* ptr =
		    (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
		                             sizeof(float), GL_MAP_READ_BIT);
		if (ptr != NULL) {
			mean = *ptr;
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		}
	}

	glDeleteBuffers(2, ssbos);

	return mean * clamp_multiplier;
}

GLuint build_brdf_lut_map(int size)
{
	GLuint shader = shader_load_compute("shaders/IBL/spbrdf.glsl");
	if (shader == 0) {
		return 0;
	}

	GLuint lut_tex = 0;
	HYBRID_FUNC_TIMER("IBL: BRDF LUT");
	GL_SCOPE_DEBUG_GROUP("IBL: BRDF LUT");

	glGenTextures(1, &lut_tex);
	glBindTexture(GL_TEXTURE_2D, lut_tex);
	glObjectLabel(GL_TEXTURE, lut_tex, -1, "BRDF LUT Texture");
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16F, size, size);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	{
		GL_SCOPE_USE_PROGRAM(shader);
		glBindImageTexture(0, lut_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY,
		                   GL_RG16F);

		uint32_t groups =
		    ((uint32_t)size + (COMPUTE_GROUP_SIZE_PBR - 1)) /
		    COMPUTE_GROUP_SIZE_PBR;
		glDispatchCompute(groups, groups, 1);
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
	}

	glDeleteProgram(shader);

	return lut_tex;
}
