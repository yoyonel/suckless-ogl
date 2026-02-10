#include <glad/glad.h>

#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <unistd.h>

static GLFWwindow* window = NULL;

enum {
	WINDOW_WIDTH = 640,
	WINDOW_HEIGHT = 480,
	GL_VER_MAJOR = 3,
	GL_VER_MINOR = 3,
	MAT4_SIZE = 16,
	VEC4_SIZE = 4,
	VEC3_SIZE = 3,
	VEC2_SIZE = 2
};

static const char* const v_shader_src =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform float uFloat;\n"
    "uniform int uInt;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos, 1.0) * uFloat * float(uInt);\n"
    "}\0";

static const char* const f_shader_src =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec2 uVec2;\n"
    "uniform vec3 uVec3;\n"
    "uniform vec4 uVec4;\n"
    "uniform mat4 uMat4;\n"
    "void main() {\n"
    "   FragColor = vec4(uVec3, 1.0) + vec4(uVec2, 0.0, 1.0) + uVec4 + "
    "uMat4[0];\n"
    "}\0";

static void write_temp_file(const char* name, const char* content)
{
	FILE* file = fopen(name, "w");
	if (file) {
		(void)fputs(content, file);
		(void)fclose(file);
	}
}

void setUp(void)
{
	if (!window) {
		if (!glfwInit()) {
			TEST_FAIL_MESSAGE("Failed to init GLFW");
		}
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VER_MAJOR);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VER_MINOR);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
		                          "TestLoc", NULL, NULL);
		if (!window) {
			TEST_FAIL_MESSAGE("Failed to create GLFW window");
		}
		glfwMakeContextCurrent(window);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			TEST_FAIL_MESSAGE("Failed to init GLAD");
		}
	}
}

void tearDown(void)
{
}

void test_Loc_Setters_Valid(void)
{
	write_temp_file("loc_test.vert", v_shader_src);
	write_temp_file("loc_test.frag", f_shader_src);

	Shader* shader = shader_load("loc_test.vert", "loc_test.frag");
	TEST_ASSERT_NOT_NULL(shader);
	shader_use(shader);

	float mat4_val[MAT4_SIZE] = {0};
	float vec4_val[VEC4_SIZE] = {0};
	float vec3_val[VEC3_SIZE] = {0};
	float vec2_val[VEC2_SIZE] = {0};

	GLint loc_float = shader_get_uniform_location(shader, "uFloat");
	GLint loc_int = shader_get_uniform_location(shader, "uInt");
	GLint loc_vec2 = shader_get_uniform_location(shader, "uVec2");
	GLint loc_vec3 = shader_get_uniform_location(shader, "uVec3");
	GLint loc_vec4 = shader_get_uniform_location(shader, "uVec4");
	GLint loc_mat4 = shader_get_uniform_location(shader, "uMat4");

	shader_set_float_loc(loc_float, 1.0F);
	shader_set_int_loc(loc_int, 1);
	shader_set_vec2_loc(loc_vec2, vec2_val);
	shader_set_vec3_loc(loc_vec3, vec3_val);
	shader_set_vec4_loc(loc_vec4, vec4_val);
	shader_set_mat4_loc(loc_mat4, mat4_val);

	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	shader_destroy(shader);
	(void)unlink("loc_test.vert");
	(void)unlink("loc_test.frag");
}

void test_Loc_Setters_Invalid(void)
{
	/* Passing -1 should be safe and not generate error */
	GLint invalid_loc = -1;
	float val[MAT4_SIZE] = {0};

	shader_set_float_loc(invalid_loc, 1.0F);
	shader_set_int_loc(invalid_loc, 1);
	shader_set_vec2_loc(invalid_loc, val);
	shader_set_vec3_loc(invalid_loc, val);
	shader_set_vec4_loc(invalid_loc, val);
	shader_set_mat4_loc(invalid_loc, val);

	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_Loc_Setters_Valid);
	RUN_TEST(test_Loc_Setters_Invalid);

	if (window) {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	return UNITY_END();
}
