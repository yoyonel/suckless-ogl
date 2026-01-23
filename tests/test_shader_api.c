#include <glad/glad.h>

#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
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
	/* Note: active uniforms might be optimized out if not used?
	   In this simple shader, they are used. */

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

	/* Call setters - we can't easily verify the result without
	   rendering/feedback, but we verify they don't crash and ideally GL
	   error check */

	shader_set_float(s, "uIntensity", 0.5f);
	/* GL Error Check */
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

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_Shader_Load_And_Cache);
	RUN_TEST(test_Shader_Setters);

	if (window) {
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	return UNITY_END();
}
