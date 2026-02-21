#include "sphere_sorting.h"

#include "gl_common.h"           /* For SIMD_ALIGNMENT */
#include "instanced_rendering.h" /* For SphereInstance */
#include "log.h"
#include "shader.h"
#include <stdlib.h>
#include <string.h>

enum { WORKGROUP_SIZE = 1024 };
enum { DEFAULT_MIN_CAPACITY = 64 };
enum { MAX_SINGLE_PASS_COUNT = 1024 };
enum { RADIX_PASSES = 4 };
enum { RADIX_BITS_PER_PASS = 8 };
enum { RADIX_BUCKETS = 256 };
enum { RADIX_SHIFT_LIMIT = 32 };
enum { RADIX_MASK = 0xFFU };
static const uint32_t FLOAT_SIGN_MASK = 0x80000000U;
static const uint32_t FLOAT_COMPLEMENT_MASK = 0xFFFFFFFFU;

static const unsigned int BIT_SHIFT_1 = 1U;
static const unsigned int BIT_SHIFT_2 = 2U;
static const unsigned int BIT_SHIFT_4 = 4U;
static const unsigned int BIT_SHIFT_8 = 8U;
static const unsigned int BIT_SHIFT_16 = 16U;
static const unsigned int INITIAL_SORT_STEP = 2U;
static const size_t GPU_ENTRY_SIZE =
    8; /* Size of Entry struct in shader: int (4) + float (4) */

static int next_pow2(int val)
{
	if (val <= 0) {
		return 1;
	}
	unsigned int res = (unsigned int)val;
	res--;
	res |= res >> BIT_SHIFT_1;
	res |= res >> BIT_SHIFT_2;
	res |= res >> BIT_SHIFT_4;
	res |= res >> BIT_SHIFT_8;
	res |= res >> BIT_SHIFT_16;
	res++;
	return (int)res;
}

void sphere_sorter_init(SphereSorter* sorter, int initial_capacity)
{
	sorter->compute_program =
	    shader_load_compute("shaders/sphere_sort.glsl");
	if (sorter->compute_program == 0) {
		LOG_ERROR("SphereSorter", "Failed to load compute shader");
	}

	glGenBuffers(1, &sorter->instance_ssbo);
	glGenBuffers(1, &sorter->index_ssbo);
	glGenBuffers(1, &sorter->sorted_instance_ssbo);
	sorter->capacity = 0;
	sorter->min_capacity =
	    initial_capacity > 0 ? initial_capacity : DEFAULT_MIN_CAPACITY;
	sorter->cpu_capacity = sorter->min_capacity;

	/* Pre-allocate scratchpad for CPU sorting with SIMD alignment */
	if (posix_memalign(
	        (void**)&sorter->entries, SIMD_ALIGNMENT,
	        (size_t)sorter->min_capacity * sizeof(SphereSortEntry)) != 0) {
		sorter->entries = NULL;
	}
	if (posix_memalign(
	        (void**)&sorter->entries_aux, SIMD_ALIGNMENT,
	        (size_t)sorter->min_capacity * sizeof(SphereSortEntry)) != 0) {
		sorter->entries_aux = NULL;
	}
	if (posix_memalign(
	        (void**)&sorter->temp_instances, SIMD_ALIGNMENT,
	        (size_t)sorter->min_capacity * sizeof(SphereInstance)) != 0) {
		sorter->temp_instances = NULL;
	}
}

void sphere_sorter_cleanup(SphereSorter* sorter)
{
	GL_SAFE_DELETE_BUFFER(sorter->instance_ssbo);
	GL_SAFE_DELETE_BUFFER(sorter->index_ssbo);
	GL_SAFE_DELETE_BUFFER(sorter->sorted_instance_ssbo);
	GL_SAFE_DELETE_PROGRAM(sorter->compute_program);
	free(sorter->entries);
	free(sorter->entries_aux);
	free(sorter->temp_instances);
	sorter->entries = NULL;
	sorter->entries_aux = NULL;
	sorter->temp_instances = NULL;
	sorter->capacity = 0;
	sorter->cpu_capacity = 0;
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
		count_pot = next_pow2(count_pot);
	}

	/* 2. Resize buffers if needed */
	if (count_pot > sorter->capacity) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * sizeof(SphereInstance)),
		             NULL, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->index_ssbo);
		/* GPU Entry is 8 bytes (int index, float depth) */
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * GPU_ENTRY_SIZE), NULL,
		             GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER,
		             sorter->sorted_instance_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * sizeof(SphereInstance)),
		             NULL, GL_DYNAMIC_DRAW);

		sorter->capacity = count_pot;
	}

	/* 3. Upload Data to instance_ssbo */
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
	                (GLsizeiptr)(count * sizeof(SphereInstance)),
	                instances);

	/* 4. Dispatch Compute */
	glUseProgram(sorter->compute_program);

	GLint loc_stage =
	    glGetUniformLocation(sorter->compute_program, "u_stage");
	GLint loc_count =
	    glGetUniformLocation(sorter->compute_program, "u_count");
	GLint loc_count_pot =
	    glGetUniformLocation(sorter->compute_program, "u_count_pot");
	GLint loc_cam =
	    glGetUniformLocation(sorter->compute_program, "u_cam_pos");
	GLint loc_j = glGetUniformLocation(sorter->compute_program, "u_j");
	GLint loc_k = glGetUniformLocation(sorter->compute_program, "u_k");

	if (loc_count >= 0) {
		glUniform1i(loc_count, count);
	}
	if (loc_count_pot >= 0) {
		glUniform1i(loc_count_pot, count_pot);
	}
	if (loc_cam >= 0) {
		glUniform3fv(loc_cam, 1, camera_pos);
	}

	/* Bind SSBOs: 0: instances, 1: entries, 2: sorted_instances */
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sorter->instance_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sorter->index_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2,
	                 sorter->sorted_instance_ssbo);

	unsigned int u_count_pot = (unsigned int)count_pot;

	/* --- OPTIMIZATION: Single-Pass Shared Memory Sort --- */
	if (u_count_pot <= MAX_SINGLE_PASS_COUNT) {
		if (loc_stage >= 0) {
			glUniform1i(loc_stage, 3); /* Stage 3: Single Pass */
		}
		/* Dispatch exactly one workgroup */
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		return sorter->sorted_instance_ssbo;
	}

	unsigned int num_groups =
	    (u_count_pot + WORKGROUP_SIZE - 1U) / WORKGROUP_SIZE;

	/* 4a. PREPARE Stage (u_stage = 0): Compute depths and fill indices */
	if (loc_stage >= 0) {
		glUniform1i(loc_stage, 0);
	}
	glDispatchCompute(num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	/* 4b. SORT Stage (u_stage = 1): Bitonic Sort on entries (indices) */
	if (loc_stage >= 0) {
		glUniform1i(loc_stage, 1);
	}

	for (unsigned int k = INITIAL_SORT_STEP; k <= u_count_pot;
	     k <<= BIT_SHIFT_1) {
		for (unsigned int j = k >> BIT_SHIFT_1; j > 0U;
		     j >>= BIT_SHIFT_1) {
			if (loc_j >= 0) {
				glUniform1i(loc_j, (int)j);
			}
			if (loc_k >= 0) {
				glUniform1i(loc_k, (int)k);
			}
			glDispatchCompute(num_groups, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	/* 4c. PERMUTE Stage (u_stage = 2): Reorder instances into
	 * sorted_instance_ssbo */
	if (loc_stage >= 0) {
		glUniform1i(loc_stage, 2);
	}
	/* Permute only needs num_groups based on actual count, but count_pot is
	 * safe */
	glDispatchCompute(num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glUseProgram(0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	/* Return the buffer that now contains the reordered instances */
	return sorter->sorted_instance_ssbo;
}
static int compare_sphere_entries(const void* lhs, const void* rhs)
{
	const SphereSortEntry* entry_lhs = (const SphereSortEntry*)lhs;
	const SphereSortEntry* entry_rhs = (const SphereSortEntry*)rhs;

	/* Back-to-Front (descending depth) */
	if (entry_lhs->depth > entry_rhs->depth) {
		return -1;
	}
	if (entry_lhs->depth < entry_rhs->depth) {
		return 1;
	}
	return 0;
}

GLuint sphere_sorter_sort_cpu(SphereSorter* sorter,
                              const SphereInstance* instances, int count,
                              const vec3 camera_pos)
{
	if (count <= 0 || !instances) {
		return sorter->instance_ssbo;
	}

	/* 1. Ensure scratchpad capacity */
	if (count > sorter->cpu_capacity) {
		/* We use posix_memalign + manual copy to avoid alignment issues
		 * with realloc */
		void* new_entries = NULL;
		void* new_aux = NULL;
		void* new_temp = NULL;

		if (posix_memalign(&new_entries, SIMD_ALIGNMENT,
		                   (size_t)count * sizeof(SphereSortEntry)) !=
		    0) {
			new_entries = NULL;
		}
		if (posix_memalign(&new_aux, SIMD_ALIGNMENT,
		                   (size_t)count * sizeof(SphereSortEntry)) !=
		    0) {
			new_aux = NULL;
		}
		if (posix_memalign(&new_temp, SIMD_ALIGNMENT,
		                   (size_t)count * sizeof(SphereInstance)) !=
		    0) {
			new_temp = NULL;
		}

		if (!new_entries || !new_aux || !new_temp) {
			LOG_ERROR("suckless-ogl.sorter",
			          "CPU sort scratchpad allocation failed");
			free(new_entries);
			free(new_aux);
			free(new_temp);
			return sorter->instance_ssbo;
		}

		/* We don't need to copy old data as these are scratchpads
		 * for the current frame ONLY. */
		free(sorter->entries);
		free(sorter->entries_aux);
		free(sorter->temp_instances);

		sorter->entries = (SphereSortEntry*)new_entries;
		sorter->entries_aux = (SphereSortEntry*)new_aux;
		sorter->temp_instances = (SphereInstance*)new_temp;
		sorter->cpu_capacity = count;
	}

	/* Final safety check */
	if (!sorter->entries || !sorter->temp_instances) {
		LOG_ERROR("suckless-ogl.sorter",
		          "CPU sort scratchpads are NULL");
		return sorter->instance_ssbo;
	}

	/* 2. Calculate depths and fill proxy entries */
	for (int i = 0; i < count; i++) {
		sorter->entries[i].original_index = i;
		/* Translation is in the 4th column (index 3) of the model
		 * matrix */
		float* pos = (float*)instances[i].model[3];
		sorter->entries[i].depth =
		    glm_vec3_distance2(pos, (float*)camera_pos);
	}

	/* 3. Sort entries */
	qsort(sorter->entries, (size_t)count, sizeof(SphereSortEntry),
	      compare_sphere_entries);

	/* 4. Reorder instances based on sorted entries */
	for (int i = 0; i < count; i++) {
		sorter->temp_instances[i] =
		    instances[sorter->entries[i].original_index];
	}

	/* 5. Upload to GPU */
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	if (count > sorter->capacity) {
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count * sizeof(SphereInstance)),
		             sorter->temp_instances, GL_DYNAMIC_DRAW);
		sorter->capacity = count;
	} else {
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
		                (GLsizeiptr)(count * sizeof(SphereInstance)),
		                sorter->temp_instances);
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	return sorter->instance_ssbo;
}

/**
 * @brief Converts a float to a uint32_t that preserves order (IEEE 754).
 * This allows sorting floats using integer radix sort.
 */
static inline uint32_t float_to_sortable_uint(float f_val)
{
	union {
		float float_val;
		uint32_t uint_val;
	} val_conv;
	val_conv.float_val = f_val;
	uint32_t mask = (val_conv.uint_val & FLOAT_SIGN_MASK)
	                    ? FLOAT_COMPLEMENT_MASK
	                    : FLOAT_SIGN_MASK;
	return val_conv.uint_val ^ mask;
}

GLuint sphere_sorter_sort_cpu_radix(SphereSorter* sorter,
                                    const SphereInstance* instances, int count,
                                    const vec3 camera_pos)
{
	if (count <= 0 || !instances) {
		return sorter->instance_ssbo;
	}

	/* 1. Ensure scratchpad capacity */
	if (count > sorter->cpu_capacity) {
		void* new_entries = NULL;
		void* new_aux = NULL;
		void* new_temp = NULL;

		if (posix_memalign(&new_entries, SIMD_ALIGNMENT,
		                   (size_t)count * sizeof(SphereSortEntry)) !=
		    0) {
			new_entries = NULL;
		}
		if (posix_memalign(&new_aux, SIMD_ALIGNMENT,
		                   (size_t)count * sizeof(SphereSortEntry)) !=
		    0) {
			new_aux = NULL;
		}
		if (posix_memalign(&new_temp, SIMD_ALIGNMENT,
		                   (size_t)count * sizeof(SphereInstance)) !=
		    0) {
			new_temp = NULL;
		}

		if (!new_entries || !new_aux || !new_temp) {
			free(new_entries);
			free(new_aux);
			free(new_temp);
			return sorter->instance_ssbo;
		}

		free(sorter->entries);
		free(sorter->entries_aux);
		free(sorter->temp_instances);

		sorter->entries = (SphereSortEntry*)new_entries;
		sorter->entries_aux = (SphereSortEntry*)new_aux;
		sorter->temp_instances = (SphereInstance*)new_temp;
		sorter->cpu_capacity = count;
	}

	if (!sorter->entries || !sorter->entries_aux) {
		return sorter->instance_ssbo;
	}

	/* 2. Prepare Entries (calculate depth and convert to sortable uint) */
	/* We'll use SphereSortEntry.depth as the key (overlaid as uint32_t) */
	for (int i = 0; i < count; i++) {
		float depth = glm_vec3_distance2((float*)instances[i].model[3],
		                                 (float*)camera_pos);
		sorter->entries[i].original_index = i;
		/* Use union/casting to store uint inside the float field */
		uint32_t* key_ptr = (uint32_t*)&sorter->entries[i].depth;
		*key_ptr = float_to_sortable_uint(depth);
	}

	/* 3. Radix Sort (4 passes of 8 bits) */
	SphereSortEntry* current_in = sorter->entries;
	SphereSortEntry* current_out = sorter->entries_aux;

	for (int shift = 0; shift < RADIX_SHIFT_LIMIT;
	     shift += RADIX_BITS_PER_PASS) {
		int counts[RADIX_BUCKETS] = {0};
		for (int i = 0; i < count; i++) {
			uint32_t key = 0;
			// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
			memcpy(&key, &current_in[i].depth, sizeof(uint32_t));
			counts[((unsigned int)key >> (unsigned int)shift) &
			       (unsigned int)RADIX_MASK]++;
		}

		/* Back-to-Front means Descending Order.
		 * To sort descending, we reverse the prefix sum logic.
		 */
		int offsets[RADIX_BUCKETS];
		offsets[RADIX_BUCKETS - 1] = 0;
		for (int i = RADIX_BUCKETS - 2; i >= 0; i--) {
			offsets[i] = offsets[i + 1] + counts[i + 1];
		}

		for (int i = 0; i < count; i++) {
			uint32_t key = 0;
			// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
			memcpy(&key, &current_in[i].depth, sizeof(uint32_t));
			unsigned int bucket =
			    ((unsigned int)key >> (unsigned int)shift) &
			    (unsigned int)RADIX_MASK;
			current_out[offsets[bucket]++] = current_in[i];
		}

		/* Ping-pong */
		SphereSortEntry* temp_ptr = current_in;
		current_in = current_out;
		current_out = temp_ptr;
	}

	/* Result is in 'current_in' (due to last swap) */

	/* 4. Reorder instances */
	for (int i = 0; i < count; i++) {
		sorter->temp_instances[i] =
		    instances[current_in[i].original_index];
	}

	/* 5. Upload to GPU */
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	if (count > sorter->capacity) {
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count * sizeof(SphereInstance)),
		             sorter->temp_instances, GL_DYNAMIC_DRAW);
		sorter->capacity = count;
	} else {
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
		                (GLsizeiptr)(count * sizeof(SphereInstance)),
		                sorter->temp_instances);
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	return sorter->instance_ssbo;
}
