// tests/test_skybox.c
#include "skybox.h"
#include "unity.h"
#include <cglm/cam.h>
#include <cglm/mat4.h>

static GLFWwindow* window = NULL;
static GLuint test_shader_program = 0;
static Shader test_shader_struct = {0};

static const int WINDOW_WIDTH = 640;
static const int WINDOW_HEIGHT = 480;
static const int GL_VERSION_MAJOR_VAL = 3;
static const int GL_VERSION_MINOR_VAL = 3;
static const GLint UNIFORM_INVALID = -1;
static const GLuint SHADER_ZERO = 0;
static const int TEX_SIZE_1 = 1;
static const int TEX_LEVEL_0 = 0;
static const int TEX_BORDER_0 = 0;
static const int SHADER_COUNT_1 = 1;
static const float BLUR_LOD_ZERO = 0.0F;
static const float MATRIX_VALUE_ONE = 1.0F;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VERSION_MAJOR_VAL);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VERSION_MINOR_VAL);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Test Window",
	                          NULL, NULL);
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

	// Create a minimal shader program for testing
	const char* vert_src =
	    "#version 330 core\n"
	    "layout(location = 0) in vec3 aPos;\n"
	    "void main() { gl_Position = vec4(aPos, 1.0); }";

	const char* frag_src =
	    "#version 330 core\n"
	    "uniform mat4 m_inv_view_proj;\n"
	    "uniform float blur_lod;\n"
	    "uniform sampler2D environmentMap;\n"
	    "out vec4 FragColor;\n"
	    "void main() { FragColor = vec4(1.0); }";

	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, SHADER_COUNT_1, &vert_src, NULL);
	glCompileShader(vert);

	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, SHADER_COUNT_1, &frag_src, NULL);
	glCompileShader(frag);

	test_shader_program = glCreateProgram();
	glAttachShader(test_shader_program, vert);
	glAttachShader(test_shader_program, frag);
	glLinkProgram(test_shader_program);

	glDeleteShader(vert);
	glDeleteShader(frag);

	test_shader_struct.program = test_shader_program;
}

void tearDown(void)
{
	if (test_shader_program != SHADER_ZERO) {
		glDeleteProgram(test_shader_program);
		test_shader_program = SHADER_ZERO;
	}
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}

void test_skybox_init_creates_vao_vbo(void)
{
	Skybox skybox = {0};
	skybox_init(&skybox, &test_shader_struct);

	// Verify VAO and VBO were created (non-zero IDs)
	TEST_ASSERT_NOT_EQUAL(SHADER_ZERO, skybox.vao);
	TEST_ASSERT_NOT_EQUAL(SHADER_ZERO, skybox.vbo);

	// Verify they are valid OpenGL objects
	TEST_ASSERT_TRUE(glIsVertexArray(skybox.vao));
	TEST_ASSERT_TRUE(glIsBuffer(skybox.vbo));

	skybox_cleanup(&skybox);
}

void test_skybox_init_caches_uniforms(void)
{
	Skybox skybox = {0};
	skybox_init(&skybox, &test_shader_struct);

	// Verify uniform locations were cached
	// Note: -1 is valid for inactive/optimized-out uniforms in minimal test
	// shader
	TEST_ASSERT_GREATER_OR_EQUAL(UNIFORM_INVALID, skybox.u_inv_view_proj);
	TEST_ASSERT_GREATER_OR_EQUAL(UNIFORM_INVALID, skybox.u_blur_lod);
	TEST_ASSERT_GREATER_OR_EQUAL(UNIFORM_INVALID, skybox.u_env_map);

	skybox_cleanup(&skybox);
}

void test_skybox_render_executes_without_error(void)
{
	Skybox skybox = {0};
	skybox_init(&skybox, &test_shader_struct);

	// Create a dummy texture
	GLuint env_map = SHADER_ZERO;
	glGenTextures(SHADER_COUNT_1, &env_map);
	glBindTexture(GL_TEXTURE_2D, env_map);
	glTexImage2D(GL_TEXTURE_2D, TEX_LEVEL_0, GL_RGB, TEX_SIZE_1, TEX_SIZE_1,
	             TEX_BORDER_0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	// Create identity matrix
	mat4 inv_view_proj;
	glm_mat4_identity(inv_view_proj);

	// Clear any previous errors
	while (glGetError() != GL_NO_ERROR) {
		// Loop to clear
	}

	// Render should not produce GL errors
	skybox_render(&skybox, &test_shader_struct, env_map, env_map,
	              inv_view_proj, BLUR_LOD_ZERO);

	GLenum err = glGetError();
	TEST_ASSERT_EQUAL(GL_NO_ERROR, err);

	glDeleteTextures(SHADER_COUNT_1, &env_map);
	skybox_cleanup(&skybox);
}

void test_skybox_cleanup_deletes_resources(void)
{
	Skybox skybox = {0};
	skybox_init(&skybox, &test_shader_struct);

	GLuint vao = skybox.vao;
	GLuint vbo = skybox.vbo;

	TEST_ASSERT_TRUE(glIsVertexArray(vao));
	TEST_ASSERT_TRUE(glIsBuffer(vbo));

	skybox_cleanup(&skybox);

	// After cleanup, resources should be deleted
	TEST_ASSERT_FALSE(glIsVertexArray(vao));
	TEST_ASSERT_FALSE(glIsBuffer(vbo));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_skybox_init_creates_vao_vbo);
	RUN_TEST(test_skybox_init_caches_uniforms);
	RUN_TEST(test_skybox_render_executes_without_error);
	RUN_TEST(test_skybox_cleanup_deletes_resources);
	return UNITY_END();
}
