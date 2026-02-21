#include "sphere_sorting.h"

#include "gl_common.h"           /* For SIMD_ALIGNMENT */
#include "instanced_rendering.h" /* For SphereInstance */
#include "log.h"
// No longer using utils.h in this file
#include <cglm/types.h> /* For vec3 */
#include <cglm/vec3.h>  /* For glm_vec3_distance2 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
	INITIAL_CAPACITY = 64,
	RADIX_BITS = 8,
	RADIX_BUCKETS = 256, /* 1 << RADIX_BITS */
	RADIX_MASK = 255     /* RADIX_BUCKETS - 1 */
};

/*
 * 4-pass Radix Sort (LSD) on 32-bit floats (treated as uint32_t).
 * Sorts DESCENDING (Furthest first -> Back-to-Front).
 *
 * This implementation relies on the fact that for non-negative IEEE 754 floats,
 * the integer representation preserves order. Depth (squared distance) is
 * always non-negative.
 */
static void radix_sort_depth_desc(SphereSortEntry* entries,
                                  SphereSortEntry* temp, int count)
{
	/* Histogram for 256 buckets */
	unsigned int histogram[RADIX_BUCKETS];
	unsigned int offsets[RADIX_BUCKETS];

	SphereSortEntry* src = entries;
	SphereSortEntry* dst = temp;

	for (int byte_idx = 0; byte_idx < 4; ++byte_idx) {
		/* Zero initialization is safer/cleaner than memset */
		for (int i = 0; i < RADIX_BUCKETS; ++i) {
			histogram[i] = 0;
		}

		/* 1. Build Histogram */
		for (int i = 0; i < count; ++i) {
			uint32_t val = 0;
			float depth_val = src[i].depth;
			/* Safe type punning via memcpy */
			// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
			memcpy(&val, &depth_val, sizeof(uint32_t));

			unsigned char byte =
			    (val >> ((unsigned int)byte_idx *
			             (unsigned int)RADIX_BITS)) &
			    (unsigned int)RADIX_MASK;
			histogram[byte]++;
		}

		/* 2. Compute Offsets (Descending Order: 255 -> 0) */
		unsigned int total = 0;
		for (int i = RADIX_BUCKETS - 1; i >= 0; --i) {
			offsets[i] = total;
			total += histogram[i];
		}

		/* 3. Scatter */
		for (int i = 0; i < count; ++i) {
			uint32_t val = 0;
			float depth_val = src[i].depth;
			// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
			memcpy(&val, &depth_val, sizeof(uint32_t));

			unsigned char byte =
			    (val >> ((unsigned int)byte_idx *
			             (unsigned int)RADIX_BITS)) &
			    (unsigned int)RADIX_MASK;
			dst[offsets[byte]++] = src[i];
		}

		/* 4. Swap Buffers */
		SphereSortEntry* tmp = src;
		src = dst;
		dst = tmp;
	}

	/*
	 * After 4 passes (even number), 'src' points to the buffer holding the
	 * sorted result.
	 * Start: src=entries, dst=temp
	 * Pass 1: src=entries -> dst=temp (swap -> src=temp)
	 * Pass 2: src=temp -> dst=entries (swap -> src=entries)
	 * Pass 3: src=entries -> dst=temp (swap -> src=temp)
	 * Pass 4: src=temp -> dst=entries (swap -> src=entries)
	 * Result is in 'entries'.
	 */
}

void sphere_sorter_init(SphereSorter* sorter, int initial_capacity)
{
	if (initial_capacity <= 0) {
		initial_capacity = INITIAL_CAPACITY;
	}
	sorter->capacity = initial_capacity;
	sorter->min_capacity = initial_capacity;
	sorter->entries = calloc(initial_capacity, sizeof(SphereSortEntry));
	sorter->temp_entries =
	    malloc(initial_capacity * sizeof(SphereSortEntry));

	size_t size = initial_capacity * sizeof(SphereInstance);
	if (size % SIMD_ALIGNMENT != 0) {
		size += SIMD_ALIGNMENT - (size % SIMD_ALIGNMENT);
	}
	sorter->temp_instances = aligned_alloc(SIMD_ALIGNMENT, size);

	if (!sorter->entries || !sorter->temp_instances ||
	    !sorter->temp_entries) {
		LOG_ERROR("suckless-ogl.sphere_sorter",
		          "Failed to allocate sphere sorting buffers");
	}
}

void sphere_sorter_cleanup(SphereSorter* sorter)
{
	if (sorter->entries) {
		free(sorter->entries);
		sorter->entries = NULL;
	}
	if (sorter->temp_entries) {
		free(sorter->temp_entries);
		sorter->temp_entries = NULL;
	}
	if (sorter->temp_instances) {
		free(sorter->temp_instances);
		sorter->temp_instances = NULL;
	}
	sorter->capacity = 0;
	sorter->min_capacity = 0;
}

void sphere_sorter_sort(SphereSorter* sorter, SphereInstance** instances_ptr,
                        int count, vec3 camera_pos)
{
	if (count <= 0 || !instances_ptr || !*instances_ptr) {
		return;
	}

	SphereInstance* instances = *instances_ptr;

	/*
	 * Determine required capacity.
	 * We must maintain at least min_capacity to ensure the App receives
	 * a buffer large enough for its max usage (assuming max <=
	 * min_capacity). If count exceeds min_capacity, we grow.
	 */
	int required_capacity =
	    (count > sorter->min_capacity) ? count : sorter->min_capacity;

	/* 1. Ensure Capacity */
	if (required_capacity > sorter->capacity) {
		/* If growing beyond min_capacity, double for amortized O(1).
		   Otherwise, just restore to min_capacity. */
		int new_cap = required_capacity;
		if (required_capacity > sorter->min_capacity) {
			new_cap = required_capacity * 2;
		}

		SphereSortEntry* new_entries =
		    realloc(sorter->entries, new_cap * sizeof(SphereSortEntry));
		SphereSortEntry* new_temp_entries = realloc(
		    sorter->temp_entries, new_cap * sizeof(SphereSortEntry));

		/* Free old aligned buffer and allocate new one */
		/* realloc is not guaranteed to preserve alignment > default,
		   and aligned_realloc is not standard C11 */
		free(sorter->temp_instances);
		sorter->temp_instances = NULL;
		/* Size must be multiple of alignment */
		size_t size = new_cap * sizeof(SphereInstance);
		if (size % SIMD_ALIGNMENT != 0) {
			size += SIMD_ALIGNMENT - (size % SIMD_ALIGNMENT);
		}
		SphereInstance* new_temp = aligned_alloc(SIMD_ALIGNMENT, size);

		if (!new_entries || !new_temp || !new_temp_entries) {
			LOG_ERROR("suckless-ogl.sphere_sorter",
			          "Failed to resize sorting buffers to %d",
			          new_cap);
			/* Try to recover or maintain old state if possible,
			   but temp list is gone. */
			if (new_entries) {
				sorter->entries = new_entries;
			}
			if (new_temp_entries) {
				sorter->temp_entries = new_temp_entries;
			}
			/* Capacity stays same if failed? Or broken state?
			   Ideally handle better, but just bail. */
			if (new_temp) {
				free(new_temp);
			}
			return;
		}

		free(sorter->temp_instances);
		sorter->entries = new_entries;
		sorter->temp_entries = new_temp_entries;
		sorter->temp_instances = new_temp;
		sorter->capacity = new_cap;
	}

	/* 2. Compute Depths and Indices */
	for (int i = 0; i < count; ++i) {
		/* model[3] is the translation column in column-major mat4 */
		vec3 pos = {instances[i].model[3][0], instances[i].model[3][1],
		            instances[i].model[3][2]};

		sorter->entries[i].original_index = i;
		sorter->entries[i].depth = glm_vec3_distance2(pos, camera_pos);
	}

	/* 3. Sort Indices (Radix Sort) */
	radix_sort_depth_desc(sorter->entries, sorter->temp_entries, count);

	/* 4. Reorder Instances into Temp Buffer */
	for (int i = 0; i < count; ++i) {
		int old_idx = sorter->entries[i].original_index;
		/* Struct copy */
		sorter->temp_instances[i] = instances[old_idx];
	}

	/* 5. Swap pointers instead of copying back */
	*instances_ptr = sorter->temp_instances;
	sorter->temp_instances = instances;

	/*
	 * We've swapped buffers. The buffer we now hold (old 'instances')
	 * has an unknown capacity (at least 'count').
	 * However, we enforce that the external buffer MUST be at least
	 * 'min_capacity' in size. We rely on this contract to avoid
	 * unnecessary reallocations when count < min_capacity.
	 */
	sorter->capacity =
	    (count > sorter->min_capacity) ? count : sorter->min_capacity;
}
