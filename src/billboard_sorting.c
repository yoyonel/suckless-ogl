#include "billboard_sorting.h"

#include "gl_common.h" /* For SIMD_ALIGNMENT */
#include "gl_debug.h"
#include "log.h"
#include "platform/platform_utils.h"
#include "profiler.h"
#include "shader.h"
#include <stdbool.h>
#include <stdlib.h>

/* Verify that C struct matches the GLSL layout (128 bytes). */
enum { EXPECTED_SPHERE_INSTANCE_SIZE = 128 };

_Static_assert(sizeof(SphereInstance) == EXPECTED_SPHERE_INSTANCE_SIZE,
               "SphereInstance size must match GLSL std430 layout (128 B)");

enum { WORKGROUP_SIZE = 1024 };

enum { DEFAULT_MIN_CAPACITY = 64 };

enum { MAX_SINGLE_PASS_COUNT = 1024 };

enum { RADIX_BITS_PER_PASS = 8 };

enum { RADIX_BUCKETS = 256 };

enum { RADIX_SHIFT_LIMIT = 32 };

enum { RADIX_MASK = 0xFFU };

static const uint32_t FLOAT_SIGN_MASK = 0x80000000U;
static const uint32_t FLOAT_COMPLEMENT_MASK = 0xFFFFFFFFU;

/** Bit-shift constants for next_pow2 (named to satisfy linter). */
static const unsigned int BIT_SHIFT_8 = 8U;
static const unsigned int BIT_SHIFT_16 = 16U;

/** Size of the Entry struct in the compute shader: int (4) + float (4). */
static const size_t GPU_ENTRY_SIZE = 8;

static int next_pow2(int val)
{
	if (val <= 0) {
		return 1;
	}
	unsigned int res = (unsigned int)val;
	res--;
	res |= res >> 1U;
	res |= res >> 2U;
	res |= res >> 4U;
	res |= res >> BIT_SHIFT_8;
	res |= res >> BIT_SHIFT_16;
	res++;
	return (int)res;
}

/* ------------------------------------------------------------------ */
/*  Shared helpers                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Grow CPU scratchpads to hold at least @p count elements.
 * @return true on success, false on allocation failure.
 */
static bool ensure_cpu_capacity(BillboardSorter* sorter, int count)
{
	if (count <= sorter->cpu_capacity) {
		return true;
	}

	void* new_entries = NULL;
	void* new_aux = NULL;
	void* new_temp = NULL;

	new_entries = platform_aligned_alloc(
	    (size_t)count * sizeof(BillboardSortEntry), SIMD_ALIGNMENT);
	new_aux = platform_aligned_alloc(
	    (size_t)count * sizeof(BillboardSortEntry), SIMD_ALIGNMENT);
	new_temp = platform_aligned_alloc(
	    (size_t)count * sizeof(SphereInstance), SIMD_ALIGNMENT);

	if (!new_entries || !new_aux || !new_temp) {
		LOG_ERROR("suckless-ogl.sorter",
		          "CPU sort scratchpad allocation failed");
		platform_aligned_free(new_entries);
		platform_aligned_free(new_aux);
		platform_aligned_free(new_temp);
		return false;
	}

	/* These are per-frame scratchpads — no need to preserve old data. */
	platform_aligned_free(sorter->entries);
	platform_aligned_free(sorter->entries_aux);
	platform_aligned_free(sorter->temp_instances);

	sorter->entries = (BillboardSortEntry*)new_entries;
	sorter->entries_aux = (BillboardSortEntry*)new_aux;
	sorter->temp_instances = (SphereInstance*)new_temp;
	sorter->cpu_capacity = count;
	return true;
}

/**
 * @brief Upload @p count sorted instances from the CPU temp buffer to the SSBO.
 * @return The SSBO handle containing the data.
 */
static GLuint upload_sorted_to_ssbo(BillboardSorter* sorter, int count)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	if (count > sorter->ssbo_capacity) {
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count * sizeof(SphereInstance)),
		             sorter->temp_instances, GL_DYNAMIC_DRAW);
		sorter->ssbo_capacity = count;
	} else {
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
		                (GLsizeiptr)(count * sizeof(SphereInstance)),
		                sorter->temp_instances);
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	/* Bind sorted data at binding 2 so billboard vertex shader can
	 * read it via gl_InstanceID (GPU sort already does this). */
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sorter->instance_ssbo);
	return sorter->instance_ssbo;
}

void billboard_sorter_init(BillboardSorter* sorter, int initial_capacity)
{
	sorter->compute_program =
	    shader_load_compute("shaders/sphere_sort.glsl");
	if (sorter->compute_program == 0) {
		LOG_ERROR("BillboardSorter", "Failed to load compute shader");
	}

	/* Cache uniform locations once. */
	sorter->loc_stage =
	    glGetUniformLocation(sorter->compute_program, "u_stage");
	sorter->loc_count =
	    glGetUniformLocation(sorter->compute_program, "u_count");
	sorter->loc_count_pot =
	    glGetUniformLocation(sorter->compute_program, "u_count_pot");
	sorter->loc_cam =
	    glGetUniformLocation(sorter->compute_program, "u_cam_pos");
	sorter->loc_j = glGetUniformLocation(sorter->compute_program, "u_j");
	sorter->loc_k = glGetUniformLocation(sorter->compute_program, "u_k");

	glGenBuffers(1, &sorter->instance_ssbo);
	glGenBuffers(1, &sorter->index_ssbo);
	glGenBuffers(1, &sorter->sorted_instance_ssbo);

	/* Bind buffers once to instantiate them in the driver before labeling
	 */
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->index_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->sorted_instance_ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	/* --- AJOUT DES LABELS POUR RENDERDOC --- */
	glObjectLabel(GL_BUFFER, sorter->instance_ssbo, -1,
	              "SSBO_Instances_Unsorted");
	glObjectLabel(GL_BUFFER, sorter->index_ssbo, -1,
	              "SSBO_Sort_Entries_Keys");
	glObjectLabel(GL_BUFFER, sorter->sorted_instance_ssbo, -1,
	              "SSBO_Instances_Sorted");
	/* --------------------------------------- */

	sorter->ssbo_capacity = 0;
	sorter->min_capacity =
	    initial_capacity > 0 ? initial_capacity : DEFAULT_MIN_CAPACITY;
	sorter->cpu_capacity = sorter->min_capacity;

	/* Pre-allocate scratchpad for CPU sorting with SIMD alignment */
	sorter->entries = (BillboardSortEntry*)platform_aligned_alloc(
	    (size_t)sorter->min_capacity * sizeof(BillboardSortEntry),
	    SIMD_ALIGNMENT);
	sorter->entries_aux = (BillboardSortEntry*)platform_aligned_alloc(
	    (size_t)sorter->min_capacity * sizeof(BillboardSortEntry),
	    SIMD_ALIGNMENT);
	sorter->temp_instances = (SphereInstance*)platform_aligned_alloc(
	    (size_t)sorter->min_capacity * sizeof(SphereInstance),
	    SIMD_ALIGNMENT);
}

void billboard_sorter_cleanup(BillboardSorter* sorter)
{
	GL_SAFE_DELETE_BUFFER(sorter->instance_ssbo);
	GL_SAFE_DELETE_BUFFER(sorter->index_ssbo);
	GL_SAFE_DELETE_BUFFER(sorter->sorted_instance_ssbo);
	GL_SAFE_DELETE_PROGRAM(sorter->compute_program);
	platform_aligned_free(sorter->entries);
	platform_aligned_free(sorter->entries_aux);
	platform_aligned_free(sorter->temp_instances);
	sorter->entries = NULL;
	sorter->entries_aux = NULL;
	sorter->temp_instances = NULL;
	sorter->ssbo_capacity = 0;
	sorter->cpu_capacity = 0;
	sorter->min_capacity = 0;
}

GLuint billboard_sorter_sort_gpu(BillboardSorter* sorter,
                                 const SphereInstance* instances, int count,
                                 const vec3 camera_pos)
{
	if (count <= 0 || sorter->compute_program == 0) {
		return sorter->instance_ssbo;
	}

	/* 1. Ensure SSBO Capacity (Power of Two) */
	int count_pot = next_pow2(count);
	if (count_pot < sorter->min_capacity) {
		count_pot = next_pow2(sorter->min_capacity);
	}

	/* 2. Resize buffers if needed */
	if (count_pot > sorter->ssbo_capacity) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * sizeof(SphereInstance)),
		             NULL, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->index_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * GPU_ENTRY_SIZE), NULL,
		             GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER,
		             sorter->sorted_instance_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
		             (GLsizeiptr)(count_pot * sizeof(SphereInstance)),
		             NULL, GL_DYNAMIC_DRAW);

		sorter->ssbo_capacity = count_pot;
	}

	/* 3. Upload Data to instance_ssbo */
	PROFILE_ZONE(sort_upload_ctx, "GPU Sort: SSBO Upload");
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
	                (GLsizeiptr)(count * sizeof(SphereInstance)),
	                instances);
	PROFILE_ZONE_END(sort_upload_ctx);

	/* 4. Dispatch Compute (using cached uniform locations) */
	PROFILE_ZONE(sort_dispatch_ctx, "GPU Sort: Compute Dispatch");
	glUseProgram(sorter->compute_program);

	if (sorter->loc_count >= 0) {
		glUniform1ui(sorter->loc_count, (unsigned int)count);
	}
	if (sorter->loc_count_pot >= 0) {
		glUniform1ui(sorter->loc_count_pot, (unsigned int)count_pot);
	}
	if (sorter->loc_cam >= 0) {
		glUniform3fv(sorter->loc_cam, 1, camera_pos);
	}

	/* Bind SSBOs: 0: instances, 1: entries, 2: sorted_instances */
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sorter->instance_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sorter->index_ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2,
	                 sorter->sorted_instance_ssbo);

	unsigned int u_count_pot = (unsigned int)count_pot;

	/* --- OPTIMIZATION: Single-Pass Shared Memory Sort --- */
	if (u_count_pot <= MAX_SINGLE_PASS_COUNT) {
		if (sorter->loc_stage >= 0) {
			glUniform1ui(sorter->loc_stage,
			             3U); /* Stage 3: Single Pass */
		}

		gl_debug_push_group("GPU Sort: Single-Pass Shared Memory Sort");
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		gl_debug_pop_group();

		glUseProgram(0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		PROFILE_ZONE_END(sort_dispatch_ctx);
		return sorter->sorted_instance_ssbo;
	}

	unsigned int num_groups =
	    (u_count_pot + WORKGROUP_SIZE - 1U) / WORKGROUP_SIZE;

	/* 4a. PREPARE Stage (u_stage = 0): Compute depths and fill indices */
	if (sorter->loc_stage >= 0) {
		glUniform1ui(sorter->loc_stage, 0U);
	}

	gl_debug_push_group("GPU Sort: Prepare");
	glDispatchCompute(num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	gl_debug_pop_group();

	/* 4b. SORT Stage (u_stage = 1): Bitonic Sort on entries (indices) */
	if (sorter->loc_stage >= 0) {
		glUniform1ui(sorter->loc_stage, 1U);
	}

	gl_debug_push_group("GPU Sort: Bitonic Sort");
	for (unsigned int k = 2U; k <= u_count_pot; k <<= 1U) {
		for (unsigned int j = k >> 1U; j > 0U; j >>= 1U) {
			if (sorter->loc_j >= 0) {
				glUniform1ui(sorter->loc_j, j);
			}
			if (sorter->loc_k >= 0) {
				glUniform1ui(sorter->loc_k, k);
			}
			glDispatchCompute(num_groups, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}
	gl_debug_pop_group();

	/* 4c. PERMUTE Stage (u_stage = 2): Reorder instances */
	if (sorter->loc_stage >= 0) {
		glUniform1ui(sorter->loc_stage, 2U);
	}

	gl_debug_push_group("GPU Sort: Permute");
	glDispatchCompute(num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	gl_debug_pop_group();

	glUseProgram(0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	PROFILE_ZONE_END(sort_dispatch_ctx);

	return sorter->sorted_instance_ssbo;
}

static int compare_sphere_entries(const void* lhs, const void* rhs)
{
	const BillboardSortEntry* entry_lhs = (const BillboardSortEntry*)lhs;
	const BillboardSortEntry* entry_rhs = (const BillboardSortEntry*)rhs;

	/* Back-to-Front (descending depth) */
	if (entry_lhs->depth > entry_rhs->depth) {
		return -1;
	}
	if (entry_lhs->depth < entry_rhs->depth) {
		return 1;
	}
	return 0;
}

GLuint billboard_sorter_sort_cpu(BillboardSorter* sorter,
                                 const SphereInstance* instances, int count,
                                 const vec3 camera_pos)
{
	if (count <= 0 || !instances) {
		return sorter->instance_ssbo;
	}

	if (!ensure_cpu_capacity(sorter, count)) {
		return sorter->instance_ssbo;
	}

	/* Safety check */
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
	qsort(sorter->entries, (size_t)count, sizeof(BillboardSortEntry),
	      compare_sphere_entries);

	/* 4. Reorder instances based on sorted entries */
	for (int i = 0; i < count; i++) {
		sorter->temp_instances[i] =
		    instances[sorter->entries[i].original_index];
	}

	/* 5. Upload to GPU */
	return upload_sorted_to_ssbo(sorter, count);
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

GLuint billboard_sorter_sort_cpu_radix(BillboardSorter* sorter,
                                       const SphereInstance* instances,
                                       int count, const vec3 camera_pos)
{
	if (count <= 0 || !instances) {
		return sorter->instance_ssbo;
	}

	if (!ensure_cpu_capacity(sorter, count)) {
		return sorter->instance_ssbo;
	}

	if (!sorter->entries || !sorter->entries_aux) {
		return sorter->instance_ssbo;
	}

	/* 2. Prepare Entries (calculate depth and convert to sortable uint) */
	for (int i = 0; i < count; i++) {
		float depth = glm_vec3_distance2((float*)instances[i].model[3],
		                                 (float*)camera_pos);
		sorter->entries[i].original_index = i;

		/* Write sortable key into the depth field via union to avoid
		 * strict aliasing issues (same approach as
		 * float_to_sortable_uint). */
		union {
			float f;
			uint32_t u;
		} pun;

		pun.u = float_to_sortable_uint(depth);
		sorter->entries[i].depth = pun.f;
	}

	/* 3. Radix Sort (4 passes of 8 bits) */
	BillboardSortEntry* current_in = sorter->entries;
	BillboardSortEntry* current_out = sorter->entries_aux;

	for (int shift = 0; shift < RADIX_SHIFT_LIMIT;
	     shift += RADIX_BITS_PER_PASS) {
		int counts[RADIX_BUCKETS] = {0};
		for (int i = 0; i < count; i++) {
			union {
				float f;
				uint32_t u;
			} pun;

			pun.f = current_in[i].depth;
			counts[((unsigned int)pun.u >> (unsigned int)shift) &
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
			union {
				float f;
				uint32_t u;
			} pun;

			pun.f = current_in[i].depth;
			unsigned int bucket =
			    ((unsigned int)pun.u >> (unsigned int)shift) &
			    (unsigned int)RADIX_MASK;
			current_out[offsets[bucket]++] = current_in[i];
		}

		/* Ping-pong */
		BillboardSortEntry* temp_ptr = current_in;
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
	return upload_sorted_to_ssbo(sorter, count);
}
