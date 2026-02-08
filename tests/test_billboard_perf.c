#include "billboard_rendering.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <time.h>
#include <stdio.h>

// Mock Shader struct since we need Shader* type but not full functionality
#include "shader.h"

static GLFWwindow* window;

void setUp(void) {
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

void tearDown(void) {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void test_billboard_debug_perf(void) {
    // Setup dummy BillboardGroup
    BillboardGroup group = {0};
    group.instance_count = 100;

    // Create a real VAO to be safe
    glGenVertexArrays(1, &group.vao);

    // Create a dummy shader program
    GLuint program = glCreateProgram();
    // Create a dummy vertex and fragment shader to make it valid
    const char* vs_source = "#version 330 core\nvoid main(){gl_Position=vec4(0.0);}";
    const char* fs_source = "#version 330 core\nout vec4 c;void main(){c=vec4(1.0);}";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_source, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_source, NULL);
    glCompileShader(fs);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    Shader shader;
    shader.program = program;
    shader.entries = NULL;
    shader.entry_count = 0;
    shader.entry_capacity = 0;
    shader.name = "TestShader";

    // Warmup
    glUseProgram(program);
    for(int i=0; i<100; ++i) {
        billboard_group_draw_debug_fill(&group);
    }

    // Measure
    double start = glfwGetTime();
    for(int i=0; i<100000; ++i) {
        billboard_group_draw_debug_fill(&group);
    }
    double end = glfwGetTime();

    printf("OPTIMIZED TIME: %f seconds\n", end - start);

    glDeleteVertexArrays(1, &group.vao);
    glDeleteProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_billboard_debug_perf);
    return UNITY_END();
}
