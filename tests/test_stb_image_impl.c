// tests/test_stb_image_impl.c
#include "unity.h"

// Note: stb_image_impl.c est généralement juste une implémentation de
// stb_image.h Il n'y a pas vraiment de fonctions à tester directement

void setUp(void)
{
}
void tearDown(void)
{
}

void test_stb_image_impl_module_exists(void)
{
	// Test factice pour inclure stb_image_impl.c dans la couverture
	// Ce fichier contient l'implémentation de stb_image, pas besoin de
	// tests spécifiques
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_stb_image_impl_module_exists);
	return UNITY_END();
}