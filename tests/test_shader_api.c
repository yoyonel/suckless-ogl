#include <glad/glad.h>

#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Minimal stub for window/context */
static GLFWwindow* window = NULL;

void setUp(void)
{
	/* Initialize GL context if needed */
	if (!window) {
		if (!glfwInit()) {
			TEST_FAIL_MESSAGE("Failed to init GLFW");
		}

		/* Offscreen context */
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		window =
		    glfwCreateWindow(640, 480, "TestShaderAPI", NULL, NULL);
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
	FILE* f = fopen(name, "w");
	if (f) {
		fputs(content, f);
		fclose(f);
	}
}

void test_Shader_Load_And_Cache(void)
{
	write_temp_file("test_api.vert", v_shader_src);
	write_temp_file("test_api.frag", f_shader_src);

	Shader* s = shader_load("test_api.vert", "test_api.frag");
	TEST_ASSERT_NOT_NULL_MESSAGE(s, "Shader load failed");
	TEST_ASSERT_NOT_EQUAL(0, s->program);

	/* Verify Cache Content */
	/* We expect: uModel, uViewProj, uColor, uIntensity */
	TEST_ASSERT_NOT_NULL(s->entries);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(4, s->entry_count);

	/* Verify Locations via new API */
	GLint loc_model = shader_get_uniform_location(s, "uModel");
	TEST_ASSERT_NOT_EQUAL(-1, loc_model);

	GLint loc_view = shader_get_uniform_location(s, "uViewProj");
	TEST_ASSERT_NOT_EQUAL(-1, loc_view);

	GLint loc_color = shader_get_uniform_location(s, "uColor");
	TEST_ASSERT_NOT_EQUAL(-1, loc_color);

	GLint loc_int = shader_get_uniform_location(s, "uIntensity");
	TEST_ASSERT_NOT_EQUAL(-1, loc_int);

	/* Verify Non-Existent Uniform */
	GLint loc_fake = shader_get_uniform_location(s, "uNonExistent");
	TEST_ASSERT_EQUAL(-1, loc_fake);

	shader_destroy(s);
	unlink("test_api.vert");
	unlink("test_api.frag");
}

void test_Shader_Setters(void)
{
	write_temp_file("test_api.vert", v_shader_src);
	write_temp_file("test_api.frag", f_shader_src);

	Shader* s = shader_load("test_api.vert", "test_api.frag");
	TEST_ASSERT_NOT_NULL(s);

	shader_use(s);

	/* Call setters */
	shader_set_float(s, "uIntensity", 0.5f);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	float color[3] = {1.0f, 0.0f, 0.0f};
	shader_set_vec3(s, "uColor", color);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	/* Set non-existent - should be no-op/log warning but no crash */
	shader_set_int(s, "uFake", 123);
	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	shader_destroy(s);
	unlink("test_api.vert");
	unlink("test_api.frag");
}

/* Covers shader_load_compute_program AND all setter variants */
void test_Shader_Complex_Types_And_Compute(void)
{
	/* 1. Test standard shader with ALL types (Vec2, Vec4, Mat4, Int) */
	write_temp_file("complex.vert", complex_v_shader_src);
	write_temp_file("complex.frag", f_shader_src); /* Reuse simple frag */

	Shader* s = shader_load("complex.vert", "complex.frag");
	TEST_ASSERT_NOT_NULL(s);
	shader_use(s);

	float vec2[2] = {1.0f, 2.0f};
	float vec3[3] = {1.0f, 2.0f, 3.0f};
	float vec4[4] = {1.0f, 2.0f, 3.0f, 4.0f};
	float mat4[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

	/* Test all setters with valid uniforms */
	shader_set_vec2(s, "uVec2", vec2);
	shader_set_vec3(s, "uVec3", vec3);
	shader_set_vec4(s, "uVec4", vec4);
	shader_set_mat4(s, "uMat4", mat4);
	shader_set_float(s, "uFloat", 1.0f);
	shader_set_int(s, "uInt", 1);

	TEST_ASSERT_EQUAL(GL_NO_ERROR, glGetError());

	shader_destroy(s);
	unlink("complex.vert");
	unlink("complex.frag");

	/* 2. Test Compute Shader Loading (Coverage for
	 * shader_load_compute_program) */
	GLint major, minor;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);

	if (major >= 4 && (major > 4 || minor >= 3)) {
		write_temp_file("test_api.comp", c_shader_src);
		Shader* cs = shader_load_compute_program("test_api.comp");
		if (cs) {
			shader_destroy(cs);
		}
		unlink("test_api.comp");
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
