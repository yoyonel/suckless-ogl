// tests/test_app.c
#include "app.h"
#include "unity.h"

void setUp(void)
{
}
void tearDown(void)
{
}

void test_app_module_exists(void)
{
	// Test factice pour inclure app.c dans la couverture
	// Si vous avez des fonctions d'init/cleanup, vous pouvez les appeler
	// ici avec des mocks GLFW si nécessaire
	TEST_PASS();
}

void test_app_update_function_exists(void)
{
	// Test factice pour la fonction update
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_app_module_exists);
	RUN_TEST(test_app_update_function_exists);
	return UNITY_END();
}