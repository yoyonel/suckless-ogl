#include "log.h"
#include "shader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char* main_path = "tests/fixtures/includes/test_main.glsl";
	char* src = shader_read_file(main_path);

	if (!src) {
		fprintf(stderr, "Failed to read shader file: %s\n", main_path);
		return 1;
	}

	printf("Loaded Source:\n%s\n", src);

	/* Verify content */
	if (strstr(src, "@header") != NULL) {
		fprintf(stderr,
		        "FAIL: @header tag should have been removed.\n");
		free(src);
		return 1;
	}

	if (strstr(src, "void helper()\n{\n}") == NULL) {
		fprintf(stderr, "FAIL: Included content not found.\n");
		free(src);
		return 1;
	}

	if (strstr(src, "void main()\n{\n\thelper();\n}") == NULL) {
		fprintf(stderr, "FAIL: Main content not found.\n");
		free(src);
		return 1;
	}

	free(src);
	printf("SUCCESS: Shader include test passed.\n");

	/* Test 2: File Not Found */
	printf("Test 2: File Not Found...\n");
	char* nothing =
	    shader_read_file("tests/fixtures/includes/does_not_exist.glsl");
	if (nothing != NULL) {
		fprintf(stderr,
		        "FAIL: Should have returned NULL for missing file.\n");
		free(nothing);
		return 1;
	}
	printf("PASS\n");

	/* Test 3: Recursion Loop */
	printf("Test 3: Recursion Loop...\n");
	char* loop = shader_read_file("tests/fixtures/includes/loop.glsl");
	if (loop != NULL) {
		fprintf(
		    stderr,
		    "FAIL: Should have returned NULL for recursion loop.\n");
		free(loop);
		return 1;
	}
	printf("PASS\n");

	/* Test 4: Invalid Syntax */
	printf("Test 4: Invalid Syntax...\n");
	char* invalid =
	    shader_read_file("tests/fixtures/includes/invalid.glsl");
	if (invalid != NULL) {
		fprintf(
		    stderr,
		    "FAIL: Should have returned NULL for invalid syntax.\n");
		free(invalid);
		return 1;
	}
	printf("PASS\n");

	/* Test 5: Max Depth Exceeded */
	printf("Test 5: Max Depth Exceeded...\n");
	char* too_deep = shader_read_file(
	    "tests/fixtures/includes/depth_chain/depth_0.glsl");
	if (too_deep != NULL) {
		fprintf(stderr,
		        "FAIL: Should have returned NULL for depth limit "
		        "exceeded.\n");
		free(too_deep);
		return 1;
	}
	printf("PASS\n");

	printf("ALL TESTS PASSED.\n");
	return 0;
}
