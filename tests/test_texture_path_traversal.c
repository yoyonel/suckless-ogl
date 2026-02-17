#include "texture.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

void setUp(void)
{
}

void tearDown(void)
{
}

void test_texture_load_pixels_path_traversal(void)
{
	// 1. Create a valid PFM file in current directory
	const char* filename = "traversal_test.pfm";
	FILE* f = fopen(filename, "wb");
	if (!f) {
		TEST_IGNORE_MESSAGE("Cannot write to current directory");
		return;
	}
	fprintf(f, "PF\n1 1\n-1.0\n");
	float data[3] = {1.0f, 0.0f, 0.0f};
	fwrite(data, sizeof(float), 3, f);
	fclose(f);

	// 2. Create a subdirectory to traverse from
	const char* subdir = "traversal_subdir";
	mkdir(subdir, 0755);

	// 3. Construct traversal path: "traversal_subdir/../traversal_test.pfm"
	char path[256];
	sprintf(path, "%s/../%s", subdir, filename);

	// 4. Try to load it
	int width, height, channels;
	float* pixels = texture_load_pixels(path, &width, &height, &channels);

	// 5. Assert failure (NULL) due to security check
	TEST_ASSERT_NULL_MESSAGE(pixels, "Should reject path with '..'");

	// 6. Cleanup
	if (pixels)
		free(pixels);
	remove(filename);
	rmdir(subdir);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_load_pixels_path_traversal);
	return UNITY_END();
}
