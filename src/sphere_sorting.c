#include "sphere_sorting.h"

#include "gl_common.h"           /* For SIMD_ALIGNMENT */
#include "instanced_rendering.h" /* For SphereInstance */
#include "log.h"
#include "shader.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

static const uint32_t MORTON_10BIT_MASK = 0x3FFU;
static const uint32_t MORTON_EXPAND_MASK1 = 0xFF0000FFU;
static const uint32_t MORTON_EXPAND_MASK2 = 0x0F00F00FU;
static const uint32_t MORTON_EXPAND_MASK3 = 0xC30C30C3U;
static const uint32_t MORTON_EXPAND_MASK4 = 0x49249249U;

static const uint32_t MORTON_EXPAND_FACTOR1 = 0x00010001U;
static const uint32_t MORTON_EXPAND_FACTOR2 = 0x00000101U;
static const uint32_t MORTON_EXPAND_FACTOR3 = 0x00000011U;
static const uint32_t MORTON_EXPAND_FACTOR4 = 0x00000005U;

static const float MORTON_SCALE = 1023.0F;

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
static bool ensure_cpu_capacity(SphereSorter* sorter, int count)
{
	if (count <= sorter->cpu_capacity) {
		return true;
	}

	void* new_entries = NULL;
	void* new_aux = NULL;
	void* new_temp = NULL;

	if (posix_memalign(&new_entries, SIMD_ALIGNMENT,
	                   (size_t)count * sizeof(SphereSortEntry)) != 0) {
		new_entries = NULL;
	}
	if (posix_memalign(&new_aux, SIMD_ALIGNMENT,
	                   (size_t)count * sizeof(SphereSortEntry)) != 0) {
		new_aux = NULL;
	}
	if (posix_memalign(&new_temp, SIMD_ALIGNMENT,
	                   (size_t)count * sizeof(SphereInstance)) != 0) {
		new_temp = NULL;
	}

	if (!new_entries || !new_aux || !new_temp) {
		LOG_ERROR("suckless-ogl.sorter",
		          "CPU sort scratchpad allocation failed");
		free(new_entries);
		free(new_aux);
		free(new_temp);
		return false;
	}

	/* These are per-frame scratchpads — no need to preserve old data. */
	free(sorter->entries);
	free(sorter->entries_aux);
	free(sorter->temp_instances);

	sorter->entries = (SphereSortEntry*)new_entries;
	sorter->entries_aux = (SphereSortEntry*)new_aux;
	sorter->temp_instances = (SphereInstance*)new_temp;
	sorter->cpu_capacity = count;
	return true;
}

/**
 * @brief Upload @p count sorted instances from the CPU temp buffer to the SSBO.
 * @return The SSBO handle containing the data.
 */
static GLuint upload_sorted_to_ssbo(SphereSorter* sorter, int count)
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
	return sorter->instance_ssbo;
}

void sphere_sorter_init(SphereSorter* sorter, int initial_capacity)
{
	sorter->compute_program =
	    shader_load_compute("shaders/sphere_sort.glsl");
	if (sorter->compute_program == 0) {
		LOG_ERROR("SphereSorter", "Failed to load compute shader");
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
	sorter->ssbo_capacity = 0;
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
	sorter->ssbo_capacity = 0;
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
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sorter->instance_ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
	                (GLsizeiptr)(count * sizeof(SphereInstance)),
	                instances);

	/* 4. Dispatch Compute (using cached uniform locations) */
	glUseProgram(sorter->compute_program);

	if (sorter->loc_count >= 0) {
		glUniform1i(sorter->loc_count, count);
	}
	if (sorter->loc_count_pot >= 0) {
		glUniform1i(sorter->loc_count_pot, count_pot);
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
			glUniform1i(sorter->loc_stage,
			            3); /* Stage 3: Single Pass */
		}
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		return sorter->sorted_instance_ssbo;
	}

	unsigned int num_groups =
	    (u_count_pot + WORKGROUP_SIZE - 1U) / WORKGROUP_SIZE;

	/* 4a. PREPARE Stage (u_stage = 0): Compute depths and fill indices */
	if (sorter->loc_stage >= 0) {
		glUniform1i(sorter->loc_stage, 0);
	}
	glDispatchCompute(num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	/* 4b. SORT Stage (u_stage = 1): Bitonic Sort on entries (indices) */
	if (sorter->loc_stage >= 0) {
		glUniform1i(sorter->loc_stage, 1);
	}

	for (unsigned int k = 2U; k <= u_count_pot; k <<= 1U) {
		for (unsigned int j = k >> 1U; j > 0U; j >>= 1U) {
			if (sorter->loc_j >= 0) {
				glUniform1i(sorter->loc_j, (int)j);
			}
			if (sorter->loc_k >= 0) {
				glUniform1i(sorter->loc_k, (int)k);
			}
			glDispatchCompute(num_groups, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	/* 4c. PERMUTE Stage (u_stage = 2): Reorder instances */
	if (sorter->loc_stage >= 0) {
		glUniform1i(sorter->loc_stage, 2);
	}
	glDispatchCompute(num_groups, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glUseProgram(0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	return sorter->sorted_instance_ssbo;
}

/* ----------------------------------------------------------------------------
 * Morton Codes (Z-Order Curve) Utilities
 * ------------------------------------------------------------------------- */

/**
 * @brief Expands 10 bits of a 32-bit integer to 30 bits.
 * Bits at 00000009876543210 become 009008007...001000.
 */
static uint32_t expand_bits(uint32_t val)
{
	uint32_t res = val & MORTON_10BIT_MASK;
	res = (res * MORTON_EXPAND_FACTOR1) & MORTON_EXPAND_MASK1;
	res = (res * MORTON_EXPAND_FACTOR2) & MORTON_EXPAND_MASK2;
	res = (res * MORTON_EXPAND_FACTOR3) & MORTON_EXPAND_MASK3;
	res = (res * MORTON_EXPAND_FACTOR4) & MORTON_EXPAND_MASK4;
	return res;
}

uint32_t calculate_morton_3d(const vec3 pos, const vec3 scene_min,
                             const vec3 scene_max)
{
	/* Normalize pos to [0, 1023] range */
	float norm_x =
	    glm_clamp((pos[0] - scene_min[0]) / (scene_max[0] - scene_min[0]),
	              0.0F, 1.0F);
	float norm_y =
	    glm_clamp((pos[1] - scene_min[1]) / (scene_max[1] - scene_min[1]),
	              0.0F, 1.0F);
	float norm_z =
	    glm_clamp((pos[2] - scene_min[2]) / (scene_max[2] - scene_min[2]),
	              0.0F, 1.0F);

	uint32_t val_ix = (uint32_t)(norm_x * MORTON_SCALE);
	uint32_t val_iy = (uint32_t)(norm_y * MORTON_SCALE);
	uint32_t val_iz = (uint32_t)(norm_z * MORTON_SCALE);

	return (expand_bits(val_ix) << 0U) | (expand_bits(val_iy) << 1U) |
	       (expand_bits(val_iz) << 2U);
}

/* ----------------------------------------------------------------------------
 * LBVH Initialization & Cleanup
 * ------------------------------------------------------------------------- */

int lbvh_init(LBVH* bvh, int initial_capacity)
{
	if (!bvh) {
		return 0;
	}
	bvh->capacity = initial_capacity;
	bvh->node_count = 0;
	bvh->nodes =
	    (LBVHNode*)calloc((size_t)initial_capacity, sizeof(LBVHNode));
	return bvh->nodes != NULL;
}

void lbvh_cleanup(LBVH* bvh)
{
	if (!bvh) {
		return;
	}
	if (bvh->nodes) {
		free(bvh->nodes);
	}
	bvh->nodes = NULL;
	bvh->node_count = 0;
	bvh->capacity = 0;
}
/* ----------------------------------------------------------------------------
 * LBVH Construction logic
 * ------------------------------------------------------------------------- */

typedef struct {
	uint32_t code;
	int orig_idx;
} SortProxy;

static int compare_proxies(const void* lhs, const void* rhs)
{
	uint32_t val_lhs = ((const SortProxy*)lhs)->code;
	uint32_t val_rhs = ((const SortProxy*)rhs)->code;
	return (val_lhs < val_rhs) ? -1 : (val_lhs > val_rhs);
}

static int find_split(const uint32_t* codes, int first, int last)
{
	uint32_t first_code = codes[first];
	uint32_t last_code = codes[last];

	if (first_code == last_code) {
		return (int)((unsigned int)(first + last) >> 1U);
	}

	/* Find the highest bit that differs (CLZ variant) */
	uint32_t common_prefix = __builtin_clz(first_code ^ last_code);

	/* Binary search for the split point */
	int split = first;
	int step = last - first;

	do {
		step = (int)((unsigned int)(step + 1) >> 1U);
		int new_split = split + step;
		if (new_split < last) {
			const uint32_t split_code = codes[new_split];
			const uint32_t diff = first_code ^ split_code;
			if (diff != 0) {
				const uint32_t split_prefix =
				    (uint32_t)__builtin_clz(diff);
				if (split_prefix > common_prefix) {
					split = new_split;
				}
			} else {
				split = new_split;
			}
		}
	} while (step > 1);

	return split;
}

static void compute_aabb(const SphereInstance* instance, float out_min[3],
                         float out_max[3])
{
	/* Center is at model[3] */
	vec3 center;
	glm_vec3_copy((float*)instance->model[3], center);

	/* Radius is the scale (assuming uniform scaling) */
	float radius = glm_vec3_distance((float*)instance->model[0],
	                                 (vec3){0.0F, 0.0F, 0.0F});

	out_min[0] = center[0] - radius;
	out_min[1] = center[1] - radius;
	out_min[2] = center[2] - radius;
	out_max[0] = center[0] + radius;
	out_max[1] = center[1] + radius;
	out_max[2] = center[2] + radius;
}

static int generate_hierarchy(LBVH* bvh, const uint32_t* codes,
                              const SphereInstance* sorted_spheres, int first,
                              int last)
{
	typedef struct {
		int first_idx, last_idx;
		int node_idx;
		bool left_resolved;
	} BuildStackFrame;

	enum { BUILD_STACK_CAPACITY = 128 };
	BuildStackFrame stack[BUILD_STACK_CAPACITY];
	int stack_ptr = 0;

	const int root_idx = bvh->node_count++;
	stack[stack_ptr++] = (BuildStackFrame){first, last, root_idx, false};

	while (stack_ptr > 0) {
		BuildStackFrame* frame = &stack[stack_ptr - 1];
		const int first_idx = frame->first_idx;
		const int last_idx = frame->last_idx;
		const int n_idx = frame->node_idx;
		LBVHNode* node_ptr = &bvh->nodes[n_idx];

		if (first_idx == last_idx) {
			/* Leaf node */
			compute_aabb(&sorted_spheres[first_idx],
			             (float*)node_ptr->aabb_min,
			             (float*)node_ptr->aabb_max);
			node_ptr->aabb_min[3] = -1.0F; /* No left child */
			node_ptr->aabb_max[3] =
			    (float)first_idx; /* Index to sorted spheres */
			node_ptr->object_idx = first_idx;
			stack_ptr--;
			continue;
		}

		if (!frame->left_resolved) {
			/* First visit: generate split and push left child */
			const int split =
			    find_split(codes, first_idx, last_idx);
			const int left_idx = bvh->node_count++;
			node_ptr->aabb_min[3] = (float)left_idx;
			frame->left_resolved = true;

			if (stack_ptr < BUILD_STACK_CAPACITY) {
				stack[stack_ptr++] = (BuildStackFrame){
				    first_idx, split, left_idx, false};
			}
		} else {
			/* Second visit: push right child */
			const int split =
			    find_split(codes, first_idx, last_idx);
			const int right_idx = bvh->node_count++;
			node_ptr->aabb_max[3] = (float)right_idx;
			node_ptr->object_idx = -1;

			/* After returning from children, we need to merge
			 * AABBs. But iterative approach requires post-order
			 * logic. Let's simplify: compute AABBs in a separate
			 * bottom-up pass.
			 */
			if (stack_ptr < BUILD_STACK_CAPACITY) {
				stack_ptr--;
				stack[stack_ptr++] = (BuildStackFrame){
				    split + 1, last_idx, right_idx, false};
			}
		}
	}

	/* Bottom-up AABB merge pass */
	for (int i = bvh->node_count - 1; i >= 0; i--) {
		LBVHNode* node_ptr_bottom = &bvh->nodes[i];
		if (node_ptr_bottom->aabb_min[3] >= 0.0F) {
			const int left_idx_bottom =
			    (int)node_ptr_bottom->aabb_min[3];
			const int right_idx_bottom =
			    (int)node_ptr_bottom->aabb_max[3];
			const LBVHNode* left_node =
			    &bvh->nodes[left_idx_bottom];
			const LBVHNode* right_node =
			    &bvh->nodes[right_idx_bottom];
			for (int j = 0; j < 3; j++) {
				node_ptr_bottom->aabb_min[j] =
				    fminf(left_node->aabb_min[j],
				          right_node->aabb_min[j]);
				node_ptr_bottom->aabb_max[j] =
				    fmaxf(left_node->aabb_max[j],
				          right_node->aabb_max[j]);
			}
		}
	}

	return root_idx;
}

void lbvh_build(LBVH* bvh, SphereInstance* instances, int count)
{
	if (!bvh || !bvh->nodes || !instances || count <= 0) {
		return;
	}

	/* 1. Pre-calculate Morton codes for all spheres */
	vec3 scene_min = {-100.0F, -100.0F, -100.0F};
	vec3 scene_max = {100.0F, 100.0F, 100.0F};

	uint32_t* codes = (uint32_t*)malloc((size_t)count * sizeof(uint32_t));
	if (!codes) {
		return;
	}

	for (int i = 0; i < count; i++) {
		vec3 center = {instances[i].model[3][0],
		               instances[i].model[3][1],
		               instances[i].model[3][2]};
		codes[i] = calculate_morton_3d(center, scene_min, scene_max);
	}

	/* 2. Sort spheres by Morton code */
	SortProxy* proxies =
	    (SortProxy*)malloc((size_t)count * sizeof(SortProxy));
	if (!proxies) {
		free(codes);
		return;
	}

	for (int i = 0; i < count; i++) {
		proxies[i].code = codes[i];
		proxies[i].orig_idx = i;
	}

	qsort(proxies, (size_t)count, sizeof(SortProxy), compare_proxies);

	SphereInstance* sorted_spheres = NULL;
	if (posix_memalign((void**)&sorted_spheres, SIMD_ALIGNMENT,
	                   (size_t)count * sizeof(SphereInstance)) != 0 ||
	    !sorted_spheres) {
		free(proxies);
		free(codes);
		return;
	}

	for (int i = 0; i < count; i++) {
		codes[i] = proxies[i].code;
		sorted_spheres[i] = instances[proxies[i].orig_idx];
	}

	/* 3. Build the tree recursively */
	bvh->node_count = 0;
	generate_hierarchy(bvh, codes, sorted_spheres, 0, count - 1);

	free(proxies);
	free(sorted_spheres);
	free(codes);
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
	qsort(sorter->entries, (size_t)count, sizeof(SphereSortEntry),
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

GLuint sphere_sorter_sort_cpu_radix(SphereSorter* sorter,
                                    const SphereInstance* instances, int count,
                                    const vec3 camera_pos)
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
		/* Use memcpy to write the sortable key into the depth field
		 * without violating strict aliasing. */
		uint32_t key = float_to_sortable_uint(depth);
		// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
		memcpy(&sorter->entries[i].depth, &key, sizeof(uint32_t));
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
			unsigned int bucket = 0;
			bucket = ((unsigned int)key >> (unsigned int)shift) &
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
	return upload_sorted_to_ssbo(sorter, count);
}
