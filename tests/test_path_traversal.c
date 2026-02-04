#include "shader.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	FILE* f = fopen("secret_data.txt", "w");
	if (f) {
		fprintf(f, "THIS_IS_SECRET");
		fclose(f);
	} else {
		TEST_FAIL_MESSAGE("Could not create secret file");
	}

	// 2. Create a shader file in a subdir that tries to access the secret
	// We'll use a subdir "traversal_test"
	mkdir("traversal_test", 0777);
	const char* shader_path = "traversal_test/malicious.glsl";
	f = fopen(shader_path, "w");
	if (f) {
		// @header "../secret_data.txt"
		fprintf(f, "@header \"../secret_data.txt\"\n");
		fprintf(f, "void main() {}\n");
		fclose(f);
	} else {
		TEST_FAIL_MESSAGE("Could not create malicious shader file");
	}

	// 3. Try to load the shader
	// We expect NULL now because of the security check
	char* source = shader_read_file(shader_path);

	if (source != NULL) {
		free(source);
		// Cleanup before failing
		remove(shader_path);
		rmdir("traversal_test");
		remove("secret_data.txt");
		TEST_FAIL_MESSAGE(
		    "Security check failed: Shader with traversal path was "
		    "loaded!");
	}

	// Cleanup
	remove(shader_path);
	rmdir("traversal_test");
	remove("secret_data.txt");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_shader_path_traversal);
	return UNITY_END();
}
