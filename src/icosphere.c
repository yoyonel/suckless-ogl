#include "icosphere.h"

#include <stdint.h>
#include <stdlib.h>

#include <cglm/vec3.h>
#include <cglm/types.h>

enum {
	INITIAL_VEC3_CAPACITY = 128,
	INITIAL_UINT_CAPACITY = 256,
	ICOSAHEDRON_VERTEX_COUNT = 12,
	ICOSAHEDRON_INDEX_COUNT = 60,
	INDEX_SHIFT = 32
};

#define X 0.525731112119133606F
#define Z 0.850650808352039932F

/* Icosahedron base vertices */
static const vec3 icosahedron_vertices[ICOSAHEDRON_VERTEX_COUNT] = {
    {-X, 0, Z}, {X, 0, Z},   {-X, 0, -Z}, {X, 0, -Z}, {0, Z, X},  {0, Z, -X},
    {0, -Z, X}, {0, -Z, -X}, {Z, X, 0},   {-Z, X, 0}, {Z, -X, 0}, {-Z, -X, 0}};

/* Icosahedron base indices */
static const unsigned int icosahedron_indices[ICOSAHEDRON_INDEX_COUNT] = {
    0,  4, 1, 0, 9, 4, 9, 5,  4, 4, 5,  8,  4,  8, 1, 8,  10, 1,  8, 3,
    10, 5, 3, 8, 5, 2, 3, 2,  7, 3, 7,  10, 3,  7, 6, 10, 7,  11, 6, 11,
    0,  6, 0, 1, 6, 6, 1, 10, 9, 0, 11, 9,  11, 2, 9, 2,  5,  7,  2, 11};

/* Vec3Array operations */
void vec3array_init(Vec3Array* array)
{
	array->size = 0;
	array->capacity = INITIAL_VEC3_CAPACITY;
	array->data = malloc(sizeof(vec3) * array->capacity);
}

void vec3array_push(Vec3Array* array, vec3 vertex)
{
	if (array->size >= array->capacity) {
		array->capacity = (array->capacity == 0) ? INITIAL_VEC3_CAPACITY
		                                         : array->capacity * 2;
		array->data =
		    realloc(array->data, sizeof(vec3) * array->capacity);
	}
	glm_vec3_copy(vertex, array->data[array->size++]);
}

void vec3array_free(Vec3Array* array)
{
	free(array->data);
	array->data = NULL;
	array->size = 0;
	array->capacity = 0;
}

/* UintArray operations */
void uintarray_init(UintArray* array)
{
	array->size = 0;
	array->capacity = INITIAL_UINT_CAPACITY;
	array->data = malloc(sizeof(unsigned int) * array->capacity);
}

void uintarray_push(UintArray* array, unsigned int value)
{
	if (array->size >= array->capacity) {
		array->capacity = (array->capacity == 0) ? INITIAL_UINT_CAPACITY
		                                         : array->capacity * 2;
		array->data = realloc(array->data,
		                      sizeof(unsigned int) * array->capacity);
	}
	array->data[array->size++] = value;
}

void uintarray_free(UintArray* array)
{
	free(array->data);
	array->data = NULL;
	array->size = 0;
	array->capacity = 0;
}

/* Math helpers */
static void normalize_vec3(vec3 vertex, vec3 out)
{
	glm_vec3_normalize_to(vertex, out);
}

/* Hash table for midpoint caching */
typedef struct {
	unsigned int a, b;
	unsigned int midpoint;
} EdgeEntry;

typedef struct {
	EdgeEntry* entries;
	size_t capacity;
	size_t count;
} EdgeHash;

static void edge_hash_init(EdgeHash* hash, size_t capacity)
{
	hash->capacity = capacity;
	hash->count = 0;
	hash->entries = calloc(capacity, sizeof(EdgeEntry));
}

static void edge_hash_free(EdgeHash* hash)
{
	free(hash->entries);
}

static unsigned int get_midpoint(unsigned int point1, unsigned int point2,
                                 Vec3Array* vertices, EdgeHash* hash)
{
	/* Order indices to ensure unique key */
	unsigned int idx_a = point1 < point2 ? point1 : point2;
	unsigned int idx_b = point1 < point2 ? point2 : point1;

	/* Simple hash for vertex pair */
	uint64_t key = ((uint64_t)idx_a << (uint32_t)INDEX_SHIFT) | idx_b;
	size_t index = (size_t)(key % hash->capacity);

	/* Open addressing with linear probing */
	while (hash->entries[index].midpoint != 0) {
		if (hash->entries[index].a == idx_a && hash->entries[index].b == idx_b) {
			return hash->entries[index].midpoint;
		}
		index = (index + 1) % hash->capacity;
	}

	/* Not found, create new vertex */
	vec3 vertex1;
	vec3 vertex2;
	vec3 midpoint_vec;
	glm_vec3_copy(vertices->data[point1], vertex1);
	glm_vec3_copy(vertices->data[point2], vertex2);
	glm_vec3_add(vertex1, vertex2, midpoint_vec);

	static const float MIDPOINT_SCALE = 0.5F;
	glm_vec3_scale(midpoint_vec, MIDPOINT_SCALE, midpoint_vec);
	glm_vec3_normalize(midpoint_vec);
	vec3array_push(vertices, midpoint_vec);

	unsigned int midpoint = (unsigned int)(vertices->size - 1);

	/* Store in hash table (assume enough capacity for now) */
	hash->entries[index].a = idx_a;
	hash->entries[index].b = idx_b;
	hash->entries[index].midpoint = midpoint;
	hash->count++;

	return midpoint;
}

static void subdivide(Vec3Array* vertices, UintArray* indices, int depth)
{
	for (int d_idx = 0; d_idx < depth; d_idx++) {
		UintArray new_indices;
		uintarray_init(&new_indices);

		/* The number of unique edges is at most 3 * number of
		   triangles. Use a generous factor for the hash table to avoid
		   collisions. */
		EdgeHash hash;
		edge_hash_init(&hash, indices->size * 4);

		for (size_t i = 0; i < indices->size; i += 3) {
			unsigned int vertex0_idx = indices->data[i + 0];
			unsigned int vertex1_idx = indices->data[i + 1];
			unsigned int vertex2_idx = indices->data[i + 2];

			unsigned int mid0_idx = get_midpoint(vertex0_idx, vertex1_idx, vertices, &hash);
			unsigned int mid1_idx = get_midpoint(vertex1_idx, vertex2_idx, vertices, &hash);
			unsigned int mid2_idx = get_midpoint(vertex2_idx, vertex0_idx, vertices, &hash);

			/* Create 4 new triangles */
			uintarray_push(&new_indices, vertex0_idx);
			uintarray_push(&new_indices, mid0_idx);
			uintarray_push(&new_indices, mid2_idx);

			uintarray_push(&new_indices, vertex1_idx);
			uintarray_push(&new_indices, mid1_idx);
			uintarray_push(&new_indices, mid0_idx);

			uintarray_push(&new_indices, vertex2_idx);
			uintarray_push(&new_indices, mid2_idx);
			uintarray_push(&new_indices, mid1_idx);

			uintarray_push(&new_indices, mid0_idx);
			uintarray_push(&new_indices, mid1_idx);
			uintarray_push(&new_indices, mid2_idx);
		}

		free(indices->data);
		*indices = new_indices;
		edge_hash_free(&hash);
	}
}

static void compute_normals(Vec3Array* vertices, Vec3Array* normals)
{
	normals->size = 0;
	for (size_t i = 0; i < vertices->size; i++) {
		vec3 normal;
		normalize_vec3(vertices->data[i], normal);
		vec3array_push(normals, normal);
	}
}

/* Icosphere operations */
void icosphere_init(IcosphereGeometry* geom)
{
	vec3array_init(&geom->vertices);
	vec3array_init(&geom->normals);
	uintarray_init(&geom->indices);
}

void icosphere_generate(IcosphereGeometry* geom, int subdivisions)
{
	/* Reset arrays */
	geom->vertices.size = 0;
	geom->normals.size = 0;
	geom->indices.size = 0;

	/* Add base icosahedron vertices */
	for (int i = 0; i < ICOSAHEDRON_VERTEX_COUNT; i++) {
		vec3array_push(&geom->vertices,
		               (vec3){icosahedron_vertices[i][0],
		                      icosahedron_vertices[i][1],
		                      icosahedron_vertices[i][2]});
	}

	/* Add base icosahedron indices */
	for (int i = 0; i < ICOSAHEDRON_INDEX_COUNT; i++) {
		uintarray_push(&geom->indices, icosahedron_indices[i]);
	}

	/* Subdivide */
	subdivide(&geom->vertices, &geom->indices, subdivisions);

	/* Compute normals */
	compute_normals(&geom->vertices, &geom->normals);
}

void icosphere_free(IcosphereGeometry* geom)
{
	vec3array_free(&geom->vertices);
	vec3array_free(&geom->normals);
	uintarray_free(&geom->indices);
}
