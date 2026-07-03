#include <glad/glad.h>

#include "billboard_renderer.h"
#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static GLFWwindow* window;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to init GLFW");
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	window = glfwCreateWindow(640, 480, "Test", NULL, NULL);
	if (!window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create window");
	}
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		TEST_FAIL_MESSAGE("Failed to init GLAD");
	}
}

void tearDown(void)
{
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}

void test_billboard_debug_perf(void)
{
	/* Setup BillboardRenderer */
	BillboardRenderer renderer = {0};
	billboard_renderer_init(&renderer, 100);

	/* Pre-fill geometry buffers with dummy VBOs to satisfy internal assets
	 * logic */
	GLuint dummy_vbo;
	glGenBuffers(1, &dummy_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, dummy_vbo);
	glBufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	billboard_renderer_prepare(&renderer, dummy_vbo, dummy_vbo, dummy_vbo);

	/* Generate dummy instances */
	SphereInstance* instances = calloc(100, sizeof(SphereInstance));
	for (int i = 0; i < 100; ++i) {
		instances[i].model[0][0] = 1.0f;
		instances[i].model[1][1] = 1.0f;
		instances[i].model[2][2] = 1.0f;
		instances[i].model[3][3] = 1.0f;
	}

	BillboardRenderParams params = {0};
	params.instances = instances;
	params.instance_count = 100;
	params.wireframe = true;
	params.sorting_mode = SORTING_MODE_CPU_QSORT;

	/* Create a dummy shader program */
	GLuint program = glCreateProgram();
	const char* vs_source =
	    "#version 330 core\nvoid main(){gl_Position=vec4(0.0);}";
	const char* fs_source =
	    "#version 330 core\nout vec4 c;void main(){c=vec4(1.0);}";

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_source, NULL);
	glCompileShader(vs);

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_source, NULL);
	glCompileShader(fs);

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	/* Warmup */
	glUseProgram(program);
	for (int i = 0; i < 10; ++i) {
		billboard_renderer_draw(&renderer, &params, program, program,
		                        NULL);
	}

	/* Measure */
	double start = glfwGetTime();
	for (int i = 0; i < 1000; ++i) {
		billboard_renderer_draw(&renderer, &params, program, program,
		                        NULL);
	}
	double end = glfwGetTime();

	printf(
	    "OPTIMIZED BILLBOARD DRAW TIME (1000 runs, 100 inst): %f seconds\n",
	    end - start);

	glDeleteBuffers(1, &dummy_vbo);
	glDeleteProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	billboard_renderer_cleanup(&renderer);
	free(instances);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_billboard_debug_perf);
	return UNITY_END();
}
