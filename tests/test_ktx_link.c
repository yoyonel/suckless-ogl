#include <ktx.h>
#include <unity.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_KtxLink_SymbolResolution(void)
{
	// On appelle une fonction réelle déclarée à la ligne 1162 du header
	// ktx.h pour vérifier que le linker résout bien les symboles.
	const char* err_str = ktxErrorString(KTX_SUCCESS);

	// Vérification factuelle sur la valeur de retour
	TEST_ASSERT_NOT_NULL(err_str);
	TEST_ASSERT_EQUAL_STRING("Operation succeeded.", err_str);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_KtxLink_SymbolResolution);
	return UNITY_END();
}
