#include "lbvh.h"

#include "gl_common.h" /* For SIMD_ALIGNMENT */
#include "log.h"
#include "utils.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Morton Code constants (10 bits per dimension) */
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

	uint32_t common_prefix =
	    (uint32_t)__builtin_clz(first_code ^ last_code);

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
	vec3 center;
	glm_vec3_copy((float*)instance->model[3], center);
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
			compute_aabb(&sorted_spheres[first_idx],
			             (float*)node_ptr->aabb_min,
			             (float*)node_ptr->aabb_max);
			node_ptr->aabb_min[3] = -1.0F;
			node_ptr->aabb_max[3] = (float)first_idx;
			node_ptr->object_idx = first_idx;
			stack_ptr--;
			continue;
		}

		const int split = find_split(codes, first_idx, last_idx);

		if (!frame->left_resolved) {
			const int left_idx = bvh->node_count++;
			node_ptr->aabb_min[3] = (float)left_idx;
			frame->left_resolved = true;
			if (stack_ptr < BUILD_STACK_CAPACITY) {
				stack[stack_ptr++] = (BuildStackFrame){
				    first_idx, split, left_idx, false};
			}
		} else {
			const int right_idx = bvh->node_count++;
			node_ptr->aabb_max[3] = (float)right_idx;
			node_ptr->object_idx = -1;
			stack_ptr--;
			if (stack_ptr < BUILD_STACK_CAPACITY) {
				stack[stack_ptr++] = (BuildStackFrame){
				    split + 1, last_idx, right_idx, false};
			}
		}
	}

	for (int i = bvh->node_count - 1; i >= 0; i--) {
		LBVHNode* node_ptr = &bvh->nodes[i];
		if (node_ptr->aabb_min[3] >= 0.0F) {
			const LBVHNode* left =
			    &bvh->nodes[(int)node_ptr->aabb_min[3]];
			const LBVHNode* right =
			    &bvh->nodes[(int)node_ptr->aabb_max[3]];
			for (int j = 0; j < 3; j++) {
				node_ptr->aabb_min[j] = fminf(
				    left->aabb_min[j], right->aabb_min[j]);
				node_ptr->aabb_max[j] = fmaxf(
				    left->aabb_max[j], right->aabb_max[j]);
			}
		}
	}

	return root_idx;
}

int lbvh_init(LBVH* bvh, int initial_object_capacity)
{
	if (!bvh) {
		return 0;
	}

	int node_capacity = 2 * initial_object_capacity;
	bvh->nodes = (LBVHNode*)calloc((size_t)node_capacity, sizeof(LBVHNode));
	bvh->capacity = node_capacity;
	bvh->node_count = 0;

	bvh->scratch_capacity = initial_object_capacity;
	bvh->morton_codes = (uint32_t*)malloc((size_t)initial_object_capacity *
	                                      sizeof(uint32_t));
	bvh->proxies = (SortProxy*)malloc((size_t)initial_object_capacity *
	                                  sizeof(SortProxy));

	if (posix_memalign((void**)&bvh->sorted_spheres, SIMD_ALIGNMENT,
	                   (size_t)initial_object_capacity *
	                       sizeof(SphereInstance)) != 0) {
		bvh->sorted_spheres = NULL;
	}

	if (!bvh->nodes || !bvh->morton_codes || !bvh->proxies ||
	    !bvh->sorted_spheres) {
		lbvh_cleanup(bvh);
		return 0;
	}

	return 1;
}

void lbvh_cleanup(LBVH* bvh)
{
	if (!bvh) {
		return;
	}
	free(bvh->nodes);
	free(bvh->morton_codes);
	free(bvh->proxies);
	free(bvh->sorted_spheres);
	(void)safe_memset(bvh, sizeof(LBVH), 0, sizeof(LBVH));
}

/**
 * @brief Ensures scratchpads can hold at least @p count elements.
 */
static bool lbvh_ensure_scratch_capacity(LBVH* bvh, int count)
{
	if (count <= bvh->scratch_capacity) {
		return true;
	}

	void* new_codes =
	    realloc(bvh->morton_codes, (size_t)count * sizeof(uint32_t));
	if (!new_codes) {
		return false;
	}
	bvh->morton_codes = (uint32_t*)new_codes;

	void* new_proxies =
	    realloc(bvh->proxies, (size_t)count * sizeof(SortProxy));
	if (!new_proxies) {
		return false;
	}
	bvh->proxies = (SortProxy*)new_proxies;

	void* new_sorted = NULL;
	if (posix_memalign(&new_sorted, SIMD_ALIGNMENT,
	                   (size_t)count * sizeof(SphereInstance)) != 0) {
		return false;
	}
	free(bvh->sorted_spheres);
	bvh->sorted_spheres = (SphereInstance*)new_sorted;

	/* Also resize node tree if needed */
	int node_capacity = 2 * count;
	if (node_capacity > bvh->capacity) {
		void* new_nodes = realloc(
		    bvh->nodes, (size_t)node_capacity * sizeof(LBVHNode));
		if (!new_nodes) {
			return false;
		}
		bvh->nodes = (LBVHNode*)new_nodes;
		bvh->capacity = node_capacity;
	}

	bvh->scratch_capacity = count;
	return true;
}

void lbvh_build(LBVH* bvh, const SphereInstance* instances, int count)
{
	if (!bvh || !instances || count <= 0) {
		return;
	}

	if (!lbvh_ensure_scratch_capacity(bvh, count)) {
		LOG_ERROR("LBVH", "Failed to resize scratch buffers");
		return;
	}

	vec3 scene_min = {-100.0F, -100.0F, -100.0F};
	vec3 scene_max = {100.0F, 100.0F, 100.0F};

	/* 1. Generate Morton codes and proxies */
	for (int i = 0; i < count; i++) {
		vec3 center = {instances[i].model[3][0],
		               instances[i].model[3][1],
		               instances[i].model[3][2]};
		bvh->proxies[i].code =
		    calculate_morton_3d(center, scene_min, scene_max);
		bvh->proxies[i].orig_idx = i;
	}

	/* 2. Sort proxies */
	qsort(bvh->proxies, (size_t)count, sizeof(SortProxy), compare_proxies);

	/* 3. Reorder instances and extract codes */
	for (int i = 0; i < count; i++) {
		bvh->morton_codes[i] = bvh->proxies[i].code;
		bvh->sorted_spheres[i] = instances[bvh->proxies[i].orig_idx];
	}

	/* 4. Build hierarchy */
	bvh->node_count = 0;
	generate_hierarchy(bvh, bvh->morton_codes, bvh->sorted_spheres, 0,
	                   count - 1);
}
