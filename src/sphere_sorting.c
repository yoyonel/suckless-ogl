#include "sphere_sorting.h"

#include "gl_common.h"           /* For SIMD_ALIGNMENT */
#include "instanced_rendering.h" /* For SphereInstance */
#include "log.h"
#include "shader.h"
#include <stdlib.h>

enum { WORKGROUP_SIZE = 256 };
enum { DEFAULT_MIN_CAPACITY = 64 };
static const unsigned int BIT_SHIFT_1 = 1U;
static const unsigned int BIT_SHIFT_2 = 2U;
static const unsigned int BIT_SHIFT_4 = 4U;
static const unsigned int BIT_SHIFT_8 = 8U;
static const unsigned int BIT_SHIFT_16 = 16U;
static const unsigned int INITIAL_SORT_STEP = 2U;

static int next_pow2(int v)
{
	if (v <= 0) {
		return 1;
	}
	unsigned int x = (unsigned int)v;
	x--;
	x |= x >> BIT_SHIFT_1;
	x |= x >> BIT_SHIFT_2;
	x |= x >> BIT_SHIFT_4;
	x |= x >> BIT_SHIFT_8;
	x |= x >> BIT_SHIFT_16;
	x++;
	return (int)x;
}

void sphere_sorter_init(SphereSorter* sorter, int initial_capacity)
{
	sorter->compute_program =
	    shader_load_compute("shaders/sphere_sort.glsl");
	if (sorter->compute_program == 0) {
		LOG_ERROR("SphereSorter", "Failed to load compute shader");
	}

	glGenBuffers(1, &sorter->instance_ssbo);
	sorter->capacity = 0;
	sorter->min_capacity = initial_capacity > 0 ? initial_capacity
	                                            : DEFAULT_MIN_CAPACITY;
}

void sphere_sorter_cleanup(SphereSorter* sorter)
{
	GL_SAFE_DELETE_BUFFER(sorter->instance_ssbo);
	GL_SAFE_DELETE_PROGRAM(sorter->compute_program);
	sorter->capacity = 0;
	sorter->min_capacity = 0;
}

GLuint sphere_sorter_sort_gpu(SphereSorter* sorter,
                              const SphereInstance* instances, int count,
                              const vec3 camera_pos)
{
	if (count <= 0 || sorter->compute_program == 0) {
		return sorter->instance_ssbo;
	}

	/* 1. Ensure SSBO Capacity (Power of Two) */
	int count_pot = next_pow2(count);
	if (count_pot < sorter->min_capacity) {
		count_pot = sorter->min_capacity;
		/* Ensure min_capacity is POT too if we enforce it */
		count_pot = next_pow2(count_pot);
	}

	/* 2. Resize buffer if needed */
	if (count_pot > sorter->capacity) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * sizeof(SphereInstance)),
		             NULL, GL_DYNAMIC_DRAW);
		sorter->capacity = count_pot;
	}

	/* 3. Upload Data */
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
	                (GLsizeiptr)(count * sizeof(SphereInstance)),
	                instances);

	/* 4. Dispatch Compute */
	glUseProgram(sorter->compute_program);

	GLint loc_count =
	    glGetUniformLocation(sorter->compute_program, "u_count");
	GLint loc_count_pot =
	    glGetUniformLocation(sorter->compute_program, "u_count_pot");
	GLint loc_cam =
	    glGetUniformLocation(sorter->compute_program, "u_cam_pos");
	GLint loc_j = glGetUniformLocation(sorter->compute_program, "u_j");
	GLint loc_k = glGetUniformLocation(sorter->compute_program, "u_k");

	if (loc_count >= 0)
		glUniform1i(loc_count, count);
	if (loc_count_pot >= 0)
		glUniform1i(loc_count_pot, count_pot);
	if (loc_cam >= 0)
		glUniform3fv(loc_cam, 1, (const float*)camera_pos);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sorter->instance_ssbo);

	/* Bitonic Sort Loop */
	unsigned int u_count_pot = (unsigned int)count_pot;
	for (unsigned int k = INITIAL_SORT_STEP; k <= u_count_pot;
	     k <<= BIT_SHIFT_1) {
		for (unsigned int j = k >> BIT_SHIFT_1; j > 0U;
		     j >>= BIT_SHIFT_1) {
			if (loc_j >= 0)
				glUniform1i(loc_j, (int)j);
			if (loc_k >= 0)
				glUniform1i(loc_k, (int)k);

			/* Dispatch */
			unsigned int num_groups =
			    (u_count_pot + WORKGROUP_SIZE - 1U) /
			    WORKGROUP_SIZE;
			glDispatchCompute(num_groups, 1, 1);

			/* Barrier */
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	glUseProgram(0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	return sorter->instance_ssbo;
}
