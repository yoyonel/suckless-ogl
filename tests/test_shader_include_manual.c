// tests/test_shader_include_manual.c
#include "log.h"
#include "shader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int EXIT_SUCCESS_CODE = 0;
static const int EXIT_FAILURE_CODE = 1;

int main(void)
{
	const char* main_path = "tests/fixtures/includes/test_main.glsl";
	char* src = shader_read_file(main_path);

	if (!src) {
		(void)fprintf(stderr, "Failed to read shader file: %s\n",
		              main_path);
		return EXIT_FAILURE_CODE;
	}

	(void)printf("Loaded Source:\n%s\n", src);

	/* Verify content */
	if (strstr(src, "@header") != NULL) {
		(void)fprintf(stderr,
		              "FAIL: @header tag should have been removed.\n");
		free(src);
		return EXIT_FAILURE_CODE;
	}

	if (strstr(src, "void helper()\n{\n}") == NULL) {
		(void)fprintf(stderr, "FAIL: Included content not found.\n");
		free(src);
		return EXIT_FAILURE_CODE;
	}

	if (strstr(src, "void main()\n{\n\thelper();\n}") == NULL) {
		(void)fprintf(stderr, "FAIL: Main content not found.\n");
		free(src);
		return EXIT_FAILURE_CODE;
	}

	free(src);
	(void)printf("SUCCESS: Shader include test passed.\n");

	/* Test 2: File Not Found */
	(void)printf("Test 2: File Not Found...\n");
	char* nothing =
	    shader_read_file("tests/fixtures/includes/does_not_exist.glsl");
	if (nothing != NULL) {
		(void)fprintf(
		    stderr,
		    "FAIL: Should have returned NULL for missing file.\n");
		free(nothing);
		return EXIT_FAILURE_CODE;
	}
	(void)printf("PASS\n");

	/* Test 3: Recursion Loop */
	(void)printf("Test 3: Recursion Loop...\n");
	char* loop = shader_read_file("tests/fixtures/includes/loop.glsl");
	if (loop != NULL) {
		(void)fprintf(
		    stderr,
		    "FAIL: Should have returned NULL for recursion loop.\n");
		free(loop);
		return EXIT_FAILURE_CODE;
	}
	(void)printf("PASS\n");

	/* Test 4: Invalid Syntax */
	(void)printf("Test 4: Invalid Syntax...\n");
	char* invalid =
	    shader_read_file("tests/fixtures/includes/invalid.glsl");
	if (invalid != NULL) {
		(void)fprintf(
		    stderr,
		    "FAIL: Should have returned NULL for invalid syntax.\n");
		free(invalid);
		return EXIT_FAILURE_CODE;
	}
	(void)printf("PASS\n");

	/* Test 5: Max Depth Exceeded */
	(void)printf("Test 5: Max Depth Exceeded...\n");
	char* too_deep = shader_read_file(
	    "tests/fixtures/includes/depth_chain/depth_0.glsl");
	if (too_deep != NULL) {
		(void)fprintf(stderr,
		              "FAIL: Should have returned NULL for depth limit "
		              "exceeded.\n");
		free(too_deep);
		return EXIT_FAILURE_CODE;
	}
	(void)printf("PASS\n");

	(void)printf("ALL TESTS PASSED.\n");
	return EXIT_SUCCESS_CODE;
}
