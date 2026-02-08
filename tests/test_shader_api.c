// tests/test_shader_api.c
#include <glad/glad.h>

#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Minimal stub for window/context */
static GLFWwindow* window = NULL;

enum {
	WINDOW_WIDTH = 640,
	WINDOW_HEIGHT = 480,
	GL_VER_MAJOR = 3,
	GL_VER_MINOR = 3,
	MIN_UNIFORM_COUNT = 4,
	VEC2_SIZE = 2,
	VEC3_SIZE = 3,
	VEC4_SIZE = 4,
	MAT4_SIZE = 16,
	TEST_INT_VAL = 123,
	GL_COMPUTE_MIN_MAJOR = 4,
	GL_COMPUTE_MIN_MINOR = 3
};
static const GLint UNIFORM_NOT_FOUND = -1;
static const GLuint PROGRAM_INVALID = 0;
static const float TEST_INTENSITY = 0.5F;
static const float COLOR_R = 1.0F;
static const float COLOR_G = 0.0F;
static const float COLOR_B = 0.0F;
static const float FLOAT_ONE = 1.0F;
static const float FLOAT_TWO = 2.0F;
static const float FLOAT_THREE = 3.0F;
static const float FLOAT_FOUR = 4.0F;

void setUp(void)
{
	/* Initialize GL context if needed */
	if (!window) {
		if (!glfwInit()) {
			TEST_FAIL_MESSAGE("Failed to init GLFW");
		}

		/* Offscreen context */
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VER_MAJOR);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VER_MINOR);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
		                          "TestShaderAPI", NULL, NULL);
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
	/* Do not destroy window to keep context alive across tests if possible,
	   or destroy it properly */
}

/* Test Data - Simple Shader */
static const char* v_shader_src =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uViewProj;\n"
    "void main() {\n"
    "   gl_Position = uViewProj * uModel * vec4(aPos, 1.0);\n"
    "}\0";

static const char* f_shader_src =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 uColor;\n"
    "uniform float uIntensity;\n"
    "void main() {\n"
    "   FragColor = vec4(uColor * uIntensity, 1.0);\n"
    "}\0";

/* Shader with many types for extensive coverage */
static const char* complex_v_shader_src =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform vec2 uVec2;\n"
    "uniform vec3 uVec3;\n"
    "uniform vec4 uVec4;\n"
    "uniform mat4 uMat4;\n"
    "uniform float uFloat;\n"
    "uniform int uInt;\n"
    "void main() {\n"
    "   gl_Position = uMat4 * vec4(aPos, 1.0) + vec4(uVec3, 0.0) + uVec4 * "
    "uFloat + vec4(uVec2, 0.0, 0.0) * float(uInt);\n"
    "}\0";

/* Compute Shader Test - Minimal */
static const char* c_shader_src =
    "#version 430 core\n"
    "layout(local_size_x = 1) in;\n"
    "void main() {\n"
    "}\0";

/* Helpers to write temp files */
static void write_temp_file(const char* name, const char* content)
{
	FILE* file = fopen(name, "w");
	if (file) {
		(void)fputs(content, file);
		(void)fclose(file);
	}
}

void test_Shader_Load_And_Cache(void)
{
	write_temp_file("test_api.vert", v_shader_src);
	write_temp_file("test_api.frag", f_shader_src);

	Shader* shader_ptr = shader_load("test_api.vert", "test_api.frag");
	TEST_ASSERT_NOT_NULL_MESSAGE(shader_ptr, "Shader load failed");
	TEST_ASSERT_NOT_EQUAL(PROGRAM_INVALID, shader_ptr->program);

	/* Verify Cache Content */
	/* We expect: uModel, uViewProj, uColor, uIntensity */
	TEST_ASSERT_NOT_NULL(shader_ptr->entries);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(MIN_UNIFORM_COUNT,
	                                 shader_ptr->entry_count);

	/* Verify Locations via new API */
	GLint loc_model = shader_get_uniform_location(shader_ptr, "uModel");
	TEST_ASSERT_NOT_EQUAL(UNIFORM_NOT_FOUND, loc_model);

	GLint loc_view = shader_get_uniform_location(shader_ptr, "uViewProj");
	TEST_ASSERT_NOT_EQUAL(UNIFORM_NOT_FOUND, loc_view);

	GLint loc_color = shader_get_uniform_location(shader_ptr, "uColor");
	TEST_ASSERT_NOT_EQUAL(UNIFORM_NOT_FOUND, loc_color);

	GLint loc_int = shader_get_uniform_location(shader_ptr, "uIntensity");
	TEST_ASSERT_NOT_EQUAL(UNIFORM_NOT_FOUND, loc_int);

	/* Verify Non-Existent Uniform */
	GLint loc_fake =
	    shader_get_uniform_location(shader_ptr, "uNonExistent");
	TEST_ASSERT_EQUAL(UNIFORM_NOT_FOUND, loc_fake);

	shader_destroy(shader_ptr);
	(void)unlink("test_api.vert");
	(void)unlink("test_api.frag");
}

void test_Shader_Setters(void)
{
	write_temp_file("test_api.vert", v_shader_src);
	write_temp_file("test_api.frag", f_shader_src);

	Shader* shader_ptr = shader_load("test_api.vert", "test_api.frag");
	TEST_ASSERT_NOT_NULL(shader_ptr);

	shader_use(shader_ptr);

	/* Call setters */
	shader_set_float(shader_ptr, "uIntensity", TEST_INTENSITY);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	float color[VEC3_SIZE] = {COLOR_R, COLOR_G, COLOR_B};
	shader_set_vec3(shader_ptr, "uColor", color);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	/* Set non-existent - should be no-op/log warning but no crash */
	shader_set_int(shader_ptr, "uFake", TEST_INT_VAL);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	shader_destroy(shader_ptr);
	(void)unlink("test_api.vert");
	(void)unlink("test_api.frag");
}

/* Covers shader_load_compute_program AND all setter variants */
void test_Shader_Complex_Types_And_Compute(void)
{
	/* 1. Test standard shader with ALL types (Vec2, Vec4, Mat4, Int) */
	write_temp_file("complex.vert", complex_v_shader_src);
	write_temp_file("complex.frag", f_shader_src); /* Reuse simple frag */

	Shader* shader_ptr = shader_load("complex.vert", "complex.frag");
	TEST_ASSERT_NOT_NULL(shader_ptr);
	shader_use(shader_ptr);

	float vec2[VEC2_SIZE] = {FLOAT_ONE, FLOAT_TWO};
	float vec3[VEC3_SIZE] = {FLOAT_ONE, FLOAT_TWO, FLOAT_THREE};
	float vec4[VEC4_SIZE] = {FLOAT_ONE, FLOAT_TWO, FLOAT_THREE, FLOAT_FOUR};
	float mat4[MAT4_SIZE] = {FLOAT_ONE, COLOR_G,   COLOR_G,   COLOR_G,
	                         COLOR_G,   FLOAT_ONE, COLOR_G,   COLOR_G,
	                         COLOR_G,   COLOR_G,   FLOAT_ONE, COLOR_G,
	                         COLOR_G,   COLOR_G,   COLOR_G,   FLOAT_ONE};

	/* Test all setters with valid uniforms */
	shader_set_vec2(shader_ptr, "uVec2", vec2);
	shader_set_vec3(shader_ptr, "uVec3", vec3);
	shader_set_vec4(shader_ptr, "uVec4", vec4);
	shader_set_mat4(shader_ptr, "uMat4", mat4);
	shader_set_float(shader_ptr, "uFloat", FLOAT_ONE);
	shader_set_int(shader_ptr, "uInt", 1);

	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	shader_destroy(shader_ptr);
	(void)unlink("complex.vert");
	(void)unlink("complex.frag");

	/* 2. Test Compute Shader Loading (Coverage for
	 * shader_load_compute_program) */
	GLint major = 0;
	GLint minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);

	if (major >= GL_COMPUTE_MIN_MAJOR &&
	    (major > GL_COMPUTE_MIN_MAJOR || minor >= GL_COMPUTE_MIN_MINOR)) {
		write_temp_file("test_api.comp", c_shader_src);
		Shader* compute_shader =
		    shader_load_compute_program("test_api.comp");
		if (compute_shader) {
			shader_destroy(compute_shader);
		}
		(void)unlink("test_api.comp");
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_Shader_Load_And_Cache);
	RUN_TEST(test_Shader_Setters);
	RUN_TEST(test_Shader_Complex_Types_And_Compute);

	if (window) {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	return UNITY_END();
}
