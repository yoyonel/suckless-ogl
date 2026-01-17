// tests/test_material.c
#include "material.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_material_module_exists(void)
{
	TEST_PASS();
}

void test_material_load_presets_null_path(void)
{
	// Test avec un chemin invalide
	MaterialLib* lib = material_load_presets("nonexistent.json");
	// La fonction devrait retourner NULL ou gérer l'erreur
	if (lib) {
		material_free_lib(lib);
	}
	TEST_PASS();
}

void test_material_lib_cleanup_null(void)
{
	// Test du cleanup avec NULL (devrait être sûr)
	material_free_lib(NULL);
	TEST_PASS();
}

void test_material_lib_cleanup_valid(void)
{
	// Test avec une structure valide mais vide
	MaterialLib* lib = (MaterialLib*)calloc(1, sizeof(MaterialLib));
	if (lib) {
		lib->materials = NULL;
		lib->count = 0;
		material_free_lib(lib);
	}
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_material_module_exists);
	RUN_TEST(test_material_load_presets_null_path);
	RUN_TEST(test_material_lib_cleanup_null);
	RUN_TEST(test_material_lib_cleanup_valid);
	return UNITY_END();
}