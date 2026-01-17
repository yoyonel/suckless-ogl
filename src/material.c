#include "material.h"

#include "log.h"
#include <cJSON.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constantes pour les limites
#define MAX_FILE_SIZE (2L * 1024L * 1024L)

// Constantes en enum au lieu de defines
enum { MAX_MATERIAL_COUNT = 10000, RGB_COMPONENTS = 3 };

// Constantes float (doivent rester en define)
#define DEFAULT_ROUGHNESS 0.5F
#define DEFAULT_ALBEDO 0.0F
#define DEFAULT_METALLIC 0.0F

static char* read_file_to_buffer(const char* path, size_t* out_size)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		LOG_ERROR("material", "Could not open file: %s", path);
		return NULL;
	}

	(void)fseek(file, 0, SEEK_END);
	const long raw_size = ftell(file);
	(void)fseek(file, 0, SEEK_SET);

	if (raw_size <= 0 || raw_size > MAX_FILE_SIZE) {
		LOG_ERROR("material", "File size out of bounds: %s", path);
		(void)fclose(file);
		return NULL;
	}

	// Conversion avec validation stricte pour l'analyseur statique
	if (raw_size < 0 || (unsigned long)raw_size > SIZE_MAX - 1U) {
		LOG_ERROR("material",
		          "File size invalid for buffer allocation");
		(void)fclose(file);
		return NULL;
	}

	const size_t file_size = (size_t)raw_size;

	// Allouer un buffer plus grand que nécessaire pour être sûr
	const size_t buffer_size = file_size + 1U;
	char* buffer = malloc(buffer_size);
	if (buffer == NULL) {
		LOG_ERROR("material", "Failed to allocate buffer");
		(void)fclose(file);
		return NULL;
	}

	const size_t bytes_read = fread(buffer, 1, file_size, file);
	(void)fclose(file);

	if (bytes_read != file_size) {
		LOG_ERROR("material", "Failed to read complete file");
		free(buffer);
		return NULL;
	}

	// Null-terminate le buffer
	// Note: file_size est garanti < buffer_size par nos validations
	// précédentes L'analyseur statique ne peut pas tracer cela, donc on
	// supprime le warning
	buffer[file_size] = '\0';  // NOLINT(clang-analyzer-security.ArrayBound)

	*out_size = file_size;
	return buffer;
}

static void parse_material_name(cJSON* element, PBRMaterial* mat)
{
	cJSON* name_item = cJSON_GetObjectItem(element, "name");
	if (name_item == NULL || !cJSON_IsString(name_item) ||
	    name_item->valuestring == NULL) {
		mat->name[0] = '\0';
		return;
	}

	const size_t name_len = strlen(name_item->valuestring);
	const size_t max_copy = sizeof(mat->name) - 1U;
	const size_t copy_len = (name_len < max_copy) ? name_len : max_copy;

	for (size_t i = 0; i < copy_len; i++) {
		mat->name[i] = name_item->valuestring[i];
	}
	mat->name[copy_len] = '\0';
}

static void parse_albedo_array(cJSON* element, PBRMaterial* mat)
{
	cJSON* albedo_item = cJSON_GetObjectItem(element, "albedo");
	if (albedo_item == NULL || !cJSON_IsArray(albedo_item)) {
		for (int j = 0; j < RGB_COMPONENTS; j++) {
			mat->albedo[j] = DEFAULT_ALBEDO;
		}
		return;
	}

	const int albedo_size = cJSON_GetArraySize(albedo_item);
	for (int j = 0; j < RGB_COMPONENTS && j < albedo_size; j++) {
		cJSON* color_component = cJSON_GetArrayItem(albedo_item, j);
		if (color_component != NULL &&
		    cJSON_IsNumber(color_component)) {
			mat->albedo[j] = (float)color_component->valuedouble;
		} else {
			mat->albedo[j] = DEFAULT_ALBEDO;
		}
	}
}

static void parse_material_properties(cJSON* element, PBRMaterial* mat)
{
	cJSON* metallic_item = cJSON_GetObjectItem(element, "metallic");
	if (metallic_item != NULL && cJSON_IsNumber(metallic_item)) {
		mat->metallic = (float)metallic_item->valuedouble;
	} else {
		mat->metallic = DEFAULT_METALLIC;
	}

	cJSON* roughness_item = cJSON_GetObjectItem(element, "roughness");
	if (roughness_item != NULL && cJSON_IsNumber(roughness_item)) {
		mat->roughness = (float)roughness_item->valuedouble;
	} else {
		mat->roughness = DEFAULT_ROUGHNESS;
	}
}

static PBRMaterial* allocate_materials(int count)
{
	if (count <= 0 || count > MAX_MATERIAL_COUNT) {
		LOG_ERROR("material", "Invalid material count: %d", count);
		return NULL;
	}

	const size_t size_check = (size_t)count;
	if (SIZE_MAX / sizeof(PBRMaterial) < size_check) {
		LOG_ERROR("material",
		          "Material count too large for allocation");
		return NULL;
	}

	PBRMaterial* materials = malloc(sizeof(PBRMaterial) * size_check);
	if (materials == NULL) {
		LOG_ERROR("material", "Failed to allocate materials array");
		return NULL;
	}

	// Initialisation explicite pour éviter le warning memset_s
	for (int i = 0; i < count; i++) {
		materials[i].name[0] = '\0';
		materials[i].metallic = DEFAULT_METALLIC;
		materials[i].roughness = DEFAULT_ROUGHNESS;
		for (int j = 0; j < RGB_COMPONENTS; j++) {
			materials[i].albedo[j] = DEFAULT_ALBEDO;
		}
	}

	return materials;
}

static int parse_materials_from_json(cJSON* json_root, PBRMaterial* materials,
                                     int max_count)
{
	int mat_index = 0;
	cJSON* element = NULL;

	cJSON_ArrayForEach(element, json_root)
	{
		if (mat_index >= max_count) {
			LOG_ERROR("material", "Array index out of bounds");
			break;
		}

		PBRMaterial* current_mat = &materials[mat_index];
		parse_material_name(element, current_mat);
		parse_albedo_array(element, current_mat);
		parse_material_properties(element, current_mat);

		mat_index++;
	}

	return mat_index;
}

MaterialLib* material_load_presets(const char* path)
{
	size_t buffer_size = 0;
	char* buffer = read_file_to_buffer(path, &buffer_size);
	if (buffer == NULL) {
		return NULL;
	}

	cJSON* json_root = cJSON_Parse(buffer);
	free(buffer);

	if (json_root == NULL) {
		const char* error_ptr = cJSON_GetErrorPtr();
		LOG_ERROR("material", "JSON Error: %s",
		          error_ptr != NULL ? error_ptr : "unknown");
		return NULL;
	}

	const int array_size = cJSON_GetArraySize(json_root);
	PBRMaterial* materials = allocate_materials(array_size);
	if (materials == NULL) {
		cJSON_Delete(json_root);
		return NULL;
	}

	const int loaded_count =
	    parse_materials_from_json(json_root, materials, array_size);
	cJSON_Delete(json_root);

	MaterialLib* lib = malloc(sizeof(MaterialLib));
	if (lib == NULL) {
		LOG_ERROR("material", "Failed to allocate MaterialLib");
		free(materials);
		return NULL;
	}

	lib->count = loaded_count;
	lib->materials = materials;

	LOG_INFO("material", "Loaded %d material presets", lib->count);
	return lib;
}

void material_free_lib(MaterialLib* lib)
{
	if (lib == NULL) {
		return;
	}

	if (lib->materials != NULL) {
		free(lib->materials);
		lib->materials = NULL;
	}

	free(lib);
	LOG_INFO("material", "Material library memory freed successfully.");
}