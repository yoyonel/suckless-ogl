#include "icosphere.h"

#include <stdlib.h>
#include <string.h>

#define X 0.525731112119133606f
#define Z 0.850650808352039932f

/* Icosahedron base vertices */
static const vec3 icosahedron_vertices[12] = {
    {-X, 0, Z}, {X, 0, Z},   {-X, 0, -Z}, {X, 0, -Z}, {0, Z, X},  {0, Z, -X},
    {0, -Z, X}, {0, -Z, -X}, {Z, X, 0},   {-Z, X, 0}, {Z, -X, 0}, {-Z, -X, 0}};

/* Icosahedron base indices */
static const unsigned int icosahedron_indices[60] = {
    0,  4, 1, 0, 9, 4, 9, 5,  4, 4, 5,  8,  4,  8, 1, 8,  10, 1,  8, 3,
    10, 5, 3, 8, 5, 2, 3, 2,  7, 3, 7,  10, 3,  7, 6, 10, 7,  11, 6, 11,
    0,  6, 0, 1, 6, 6, 1, 10, 9, 0, 11, 9,  11, 2, 9, 2,  5,  7,  2, 11};

/* Vec3Array operations */
void vec3array_init(Vec3Array* array)
{
	array->size = 0;
	array->capacity = 128;
	array->data = malloc(sizeof(vec3) * array->capacity);
}

void vec3array_push(Vec3Array* array, vec3 v)
{
	if (array->size >= array->capacity) {
		array->capacity *= 2;
		array->data =
		    realloc(array->data, sizeof(vec3) * array->capacity);
	}
	glm_vec3_copy(v, array->data[array->size++]);
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
	array->capacity = 256;
	array->data = malloc(sizeof(unsigned int) * array->capacity);
}

void uintarray_push(UintArray* array, unsigned int value)
{
	if (array->size >= array->capacity) {
		array->capacity *= 2;
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
static void normalize_vec3(vec3 v, vec3 out)
{
	glm_vec3_normalize_to(v, out);
}

static void vec3_midpoint(vec3 a, vec3 b, vec3 out)
{
	glm_vec3_add(a, b, out);
	glm_vec3_scale(out, 0.5f, out);
	glm_vec3_normalize(out);
}

/* Subdivision helpers */
static unsigned int vertex_for_edge(vec3 a, vec3 b, Vec3Array* vertices)
{
	vec3 m;
	vec3_midpoint(a, b, m);
	vec3array_push(vertices, m);
	return (unsigned int)(vertices->size - 1);
}

static void subdivide(Vec3Array* vertices, UintArray* indices, int depth)
{
	for (int d = 0; d < depth; d++) {
		UintArray new_indices;
		uintarray_init(&new_indices);

		for (size_t i = 0; i < indices->size; i += 3) {
			vec3 v0, v1, v2;
			glm_vec3_copy(vertices->data[indices->data[i + 0]], v0);
			glm_vec3_copy(vertices->data[indices->data[i + 1]], v1);
			glm_vec3_copy(vertices->data[indices->data[i + 2]], v2);

			unsigned int a = vertex_for_edge(v0, v1, vertices);
			unsigned int b = vertex_for_edge(v1, v2, vertices);
			unsigned int c = vertex_for_edge(v2, v0, vertices);

			/* Create 4 new triangles */
			uintarray_push(&new_indices, indices->data[i + 0]);
			uintarray_push(&new_indices, a);
			uintarray_push(&new_indices, c);

			uintarray_push(&new_indices, indices->data[i + 1]);
			uintarray_push(&new_indices, b);
			uintarray_push(&new_indices, a);

			uintarray_push(&new_indices, indices->data[i + 2]);
			uintarray_push(&new_indices, c);
			uintarray_push(&new_indices, b);

			uintarray_push(&new_indices, a);
			uintarray_push(&new_indices, b);
			uintarray_push(&new_indices, c);
		}

		free(indices->data);
		*indices = new_indices;
	}
}

static void compute_normals(Vec3Array* vertices, Vec3Array* normals)
{
	normals->size = 0;
	for (size_t i = 0; i < vertices->size; i++) {
		vec3 n;
		normalize_vec3(vertices->data[i], n);
		vec3array_push(normals, n);
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
	for (int i = 0; i < 12; i++) {
		vec3array_push(&geom->vertices,
		               (vec3){icosahedron_vertices[i][0],
		                      icosahedron_vertices[i][1],
		                      icosahedron_vertices[i][2]});
	}

	/* Add base icosahedron indices */
	for (int i = 0; i < 60; i++) {
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
