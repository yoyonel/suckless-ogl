#ifndef ICOSPHERE_H
#define ICOSPHERE_H

#include <cglm/cglm.h>
#include <stddef.h>

/* Dynamic array for vec3 */
typedef struct {
	vec3* data;
	size_t size;
	size_t capacity;
} Vec3Array;

/* Dynamic array for unsigned int */
typedef struct {
	unsigned int* data;
	size_t size;
	size_t capacity;
} UintArray;

/* Icosphere geometry container */
typedef struct {
	Vec3Array vertices;
	Vec3Array normals;
	UintArray indices;
} IcosphereGeometry;

/* Array operations */
void vec3array_init(Vec3Array* array);
void vec3array_push(Vec3Array* array, vec3 vertex);
void vec3array_free(Vec3Array* array);

void uintarray_init(UintArray* array);
void uintarray_push(UintArray* array, unsigned int value);
void uintarray_free(UintArray* array);

/* Icosphere operations */
void icosphere_init(IcosphereGeometry* geom);
void icosphere_generate(IcosphereGeometry* geom, int subdivisions);
void icosphere_free(IcosphereGeometry* geom);

#endif /* ICOSPHERE_H */
