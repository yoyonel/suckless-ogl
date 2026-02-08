// tests/test_path_traversal.c
#include "shader.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_shader_path_traversal(void)
{
	// 1. Create a secret file outside the expected shader dir
	FILE* file = fopen("secret_data.txt", "w");
	if (file) {
		if (fprintf(file, "THIS_IS_SECRET") < 0) {
			// Handle write error if strictly needed, or just ignore
			// for test setup
		}
		(void)fclose(file);
	} else {
		TEST_FAIL_MESSAGE("Could not create secret file");
	}

	// 2. Create a shader file in a subdir that tries to access the secret
	// We'll use a subdir "traversal_test"
	const mode_t DIR_PERMS = 0777;
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	if (mkdir("traversal_test", DIR_PERMS) != 0) {
		// Might already exist, ignore
	}

	const char* shader_path = "traversal_test/malicious.glsl";
	file = fopen(shader_path, "w");
	if (file) {
		// @header "../secret_data.txt"
		(void)fprintf(file, "@header \"../secret_data.txt\"\n");
		(void)fprintf(file, "void main() {}\n");
		(void)fclose(file);
	} else {
		TEST_FAIL_MESSAGE("Could not create malicious shader file");
	}

	// 3. Try to load the shader
	// We expect NULL now because of the security check
	char* source = shader_read_file(shader_path);

	if (source != NULL) {
		free(source);
		// Cleanup before failing
		(void)remove(shader_path);
		(void)rmdir("traversal_test");
		(void)remove("secret_data.txt");
		TEST_FAIL_MESSAGE(
		    "Security check failed: Shader with traversal path was "
		    "loaded!");
	}

	// Cleanup
	(void)remove(shader_path);
	(void)rmdir("traversal_test");
	(void)remove("secret_data.txt");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_shader_path_traversal);
	return UNITY_END();
}
