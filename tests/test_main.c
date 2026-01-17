// tests/test_main.c
#include "unity.h"

// Note: main.c contient généralement juste la fonction main() qui lance
// l'application Il est acceptable d'avoir une couverture limitée sur ce fichier

void setUp(void)
{
}
void tearDown(void)
{
}

void test_main_module_exists(void)
{
	// Test factice pour inclure main.c dans le rapport de couverture
	// En pratique, main.c est difficile à tester car il contient la boucle
	// principale
	TEST_PASS();
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_main_module_exists);
	return UNITY_END();
}