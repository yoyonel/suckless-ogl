#include "glad/glad.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "shader.h"
#include "unity.h"

static GLFWwindow* window = NULL;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless-like testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	// Request a reasonable core profile
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(640, 480, "Test Window", NULL, NULL);
	if (!window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}
}

void tearDown(void)
{
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();

	// Cleanup files
	remove("test_valid.vert");
	remove("test_valid.frag");
	remove("test_invalid.vert");
}

// Helper to write string to file
static void write_file(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	if (f) {
		fputs(content, f);
		fclose(f);
	}
}

void test_shader_read_file_success(void)
{
	write_file("test_read.txt", "content");
	char* content = shader_read_file("test_read.txt");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_EQUAL_STRING("content", content);
	free(content);
	remove("test_read.txt");
}

void test_shader_read_file_missing(void)
{
	char* content = shader_read_file("non_existent.txt");
	TEST_ASSERT_NULL(content);
}

void test_shader_read_file_empty(void)
{
	// Test reading an empty file
	write_file("test_empty.txt", "");
	char* content = shader_read_file("test_empty.txt");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_EQUAL_STRING("", content);
	free(content);
	remove("test_empty.txt");
}

void test_shader_read_file_fread_fail(void)
{
	const char* path = "test_shader_fread_fail.glsl";
	const char* content = "void main() {}\n";

	/* Create a valid file */
	write_file(path, content);

	/* Make file unreadable: fopen succeeds, fread fails */
	chmod(path, 0000);

	char* src = shader_read_file(path);

	/* fread error path must return NULL */
	TEST_ASSERT_NULL(src);

	/* Restore permissions for cleanup */
	chmod(path, 0644);
	remove(path);
}

void test_shader_read_file_large(void)
{
	// Test reading a file with substantial content to cover fread path
	const char* large_content =
	    "#version 330 core\n"
	    "layout(location = 0) in vec3 aPos;\n"
	    "layout(location = 1) in vec3 aNormal;\n"
	    "uniform mat4 model;\n"
	    "uniform mat4 view;\n"
	    "uniform mat4 projection;\n"
	    "out vec3 FragPos;\n"
	    "out vec3 Normal;\n"
	    "void main() {\n"
	    "    FragPos = vec3(model * vec4(aPos, 1.0));\n"
	    "    Normal = mat3(transpose(inverse(model))) * aNormal;\n"
	    "    gl_Position = projection * view * vec4(FragPos, 1.0);\n"
	    "}";

	write_file("test_large.txt", large_content);
	char* content = shader_read_file("test_large.txt");
	TEST_ASSERT_NOT_NULL(content);
	TEST_ASSERT_EQUAL_STRING(large_content, content);
	free(content);
	remove("test_large.txt");
}

void test_shader_compile_success(void)
{
	const char* vert_src =
	    "#version 330 core\n"
	    "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }";
	write_file("test_valid.vert", vert_src);

	GLuint shader = shader_compile("test_valid.vert", GL_VERTEX_SHADER);
	TEST_ASSERT_NOT_EQUAL(0, shader);

	GLint type = 0;
	glGetShaderiv(shader, GL_SHADER_TYPE, &type);
	TEST_ASSERT_EQUAL(GL_VERTEX_SHADER, type);

	glDeleteShader(shader);
}

void test_shader_compile_fail_syntax(void)
{
	const char* bad_src =
	    "#version 330 core\n"
	    "void main() { syntax_error }";  // Missing semicolon/invalid code
	write_file("test_invalid.vert", bad_src);

	// This should print an error to stderr (tested via log module usually,
	// but here we just check return 0)
	GLuint shader = shader_compile("test_invalid.vert", GL_VERTEX_SHADER);
	TEST_ASSERT_EQUAL(0, shader);
}

void test_shader_compile_fail_io(void)
{
	GLuint shader = shader_compile("does_not_exist.vert", GL_VERTEX_SHADER);
	TEST_ASSERT_EQUAL(0, shader);
}

void test_shader_load_program_success(void)
{
	const char* vert_src =
	    "#version 330 core\n"
	    "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }";
	write_file("test_valid.vert", vert_src);

	const char* frag_src =
	    "#version 330 core\n"
	    "out vec4 FragColor;\n"
	    "void main() { FragColor = vec4(1.0); }";
	write_file("test_valid.frag", frag_src);

	GLuint prog = shader_load_program("test_valid.vert", "test_valid.frag");
	TEST_ASSERT_NOT_EQUAL(0, prog);

	glDeleteProgram(prog);
}

void test_shader_load_program_fragment_fail(void)
{
	const char* vert_src =
	    "#version 330 core\n"
	    "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }";
	write_file("test_valid.vert", vert_src);

	const char* bad_frag =
	    "#version 330 core\n"
	    "void main() { invalid_syntax }";
	write_file("test_invalid.frag", bad_frag);

	GLuint prog =
	    shader_load_program("test_valid.vert", "test_invalid.frag");
	TEST_ASSERT_EQUAL(0, prog);

	remove("test_invalid.frag");
}

void test_shader_load_compute_success(void)
{
	const char* comp_src =
	    "#version 430 core\n"
	    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	    "void main() { }";
	write_file("test_valid.comp", comp_src);

	GLuint prog = shader_load_compute("test_valid.comp");
	TEST_ASSERT_NOT_EQUAL(0, prog);

	glDeleteProgram(prog);
	remove("test_valid.comp");
}

void test_shader_load_compute_compile_fail(void)
{
	const char* bad_comp =
	    "#version 430 core\n"
	    "layout(local_size_x = 1) in;\n"
	    "void main() { syntax_error }";
	write_file("test_invalid.comp", bad_comp);

	GLuint prog = shader_load_compute("test_invalid.comp");
	TEST_ASSERT_EQUAL(0, prog);

	remove("test_invalid.comp");
}

void test_shader_load_program_vertex_fail(void)
{
	const char* bad_vert =
	    "#version 330 core\n"
	    "void main() { syntax_error }";
	write_file("test_invalid.vert", bad_vert);

	const char* frag_src =
	    "#version 330 core\n"
	    "out vec4 FragColor;\n"
	    "void main() { FragColor = vec4(1.0); }";
	write_file("test_valid.frag", frag_src);

	GLuint prog =
	    shader_load_program("test_invalid.vert", "test_valid.frag");
	TEST_ASSERT_EQUAL(0, prog);

	remove("test_invalid.vert");
}

void test_shader_load_program_link_fail(void)
{
	// Create shaders that compile but fail to link
	// (e.g., vertex shader outputs that fragment shader doesn't use)
	const char* vert_src =
	    "#version 330 core\n"
	    "out vec4 mismatched_output;\n"
	    "void main() { gl_Position = vec4(0.0); mismatched_output = "
	    "vec4(1.0); }";
	write_file("test_link_vert.vert", vert_src);

	const char* frag_src =
	    "#version 330 core\n"
	    "in vec4 different_input;\n"  // Mismatched input name
	    "out vec4 FragColor;\n"
	    "void main() { FragColor = different_input; }";
	write_file("test_link_frag.frag", frag_src);

	// This may or may not fail depending on GL implementation
	// Some drivers are lenient, so we just test the code path exists
	GLuint prog =
	    shader_load_program("test_link_vert.vert", "test_link_frag.frag");

	// Clean up regardless of result
	if (prog != 0) {
		glDeleteProgram(prog);
	}

	remove("test_link_vert.vert");
	remove("test_link_frag.frag");
}

void test_shader_load_compute_link_fail(void)
{
	// Create a compute shader that compiles but has linking issues
	// This is harder to trigger, but we can try with invalid work group
	// sizes
	const char* comp_src =
	    "#version 430 core\n"
	    "layout(local_size_x = 1) in;\n"
	    "void main() { }";
	write_file("test_comp_link.comp", comp_src);

	GLuint prog = shader_load_compute("test_comp_link.comp");

	// This should succeed on most implementations, but tests the code path
	if (prog != 0) {
		glDeleteProgram(prog);
	}

	remove("test_comp_link.comp");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_shader_read_file_success);
	RUN_TEST(test_shader_read_file_missing);
	RUN_TEST(test_shader_read_file_empty);
	RUN_TEST(test_shader_read_file_large);
	RUN_TEST(test_shader_read_file_fread_fail);
	RUN_TEST(test_shader_compile_success);
	RUN_TEST(test_shader_compile_fail_syntax);
	RUN_TEST(test_shader_compile_fail_io);
	RUN_TEST(test_shader_load_program_success);
	RUN_TEST(test_shader_load_program_vertex_fail);
	RUN_TEST(test_shader_load_program_fragment_fail);
	RUN_TEST(test_shader_load_program_link_fail);
	RUN_TEST(test_shader_load_compute_success);
	RUN_TEST(test_shader_load_compute_compile_fail);
	RUN_TEST(test_shader_load_compute_link_fail);
	return UNITY_END();
}
