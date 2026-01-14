#include "unity.h"
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_shader_read_existing_file(void) {
    const char* filename = "test_dummy.txt";
    const char* content = "Hello World from Test";
    
    // Create dummy file
    FILE* f = fopen(filename, "w");
    fputs(content, f);
    fclose(f);
    
    // Read it back
    char* result = shader_read_file(filename);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING(content, result);
    
    free(result);
    remove(filename);
}

void test_shader_read_nonexistent_file(void) {
    char* result = shader_read_file("nonexistent_file_12345.txt");
    TEST_ASSERT_NULL(result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shader_read_existing_file);
    RUN_TEST(test_shader_read_nonexistent_file);
    return UNITY_END();
}
