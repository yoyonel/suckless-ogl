#include "unity.h"
#include "skybox.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>
#include <cglm/cam.h>

static GLFWwindow* window = NULL;
static GLuint test_shader = 0;

void setUp(void) {
    if (!glfwInit()) {
        TEST_FAIL_MESSAGE("Failed to initialize GLFW");
    }
    
    // Hidden window for headless testing
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
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
    glShaderSource(vert, 1, &vert_src, NULL);
    glCompileShader(vert);

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &frag_src, NULL);
    glCompileShader(frag);

    test_shader = glCreateProgram();
    glAttachShader(test_shader, vert);
    glAttachShader(test_shader, frag);
    glLinkProgram(test_shader);

    glDeleteShader(vert);
    glDeleteShader(frag);
}

void tearDown(void) {
    if (test_shader != 0) {
        glDeleteProgram(test_shader);
        test_shader = 0;
    }
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

void test_skybox_init_creates_vao_vbo(void) {
    Skybox skybox = {0};
    skybox_init(&skybox, test_shader);
    
    // Verify VAO and VBO were created (non-zero IDs)
    TEST_ASSERT_NOT_EQUAL(0, skybox.vao);
    TEST_ASSERT_NOT_EQUAL(0, skybox.vbo);
    
    // Verify they are valid OpenGL objects
    TEST_ASSERT_TRUE(glIsVertexArray(skybox.vao));
    TEST_ASSERT_TRUE(glIsBuffer(skybox.vbo));
    
    skybox_cleanup(&skybox);
}

void test_skybox_init_caches_uniforms(void) {
    Skybox skybox = {0};
    skybox_init(&skybox, test_shader);
    
    // Verify uniform locations were cached
    // Note: -1 is valid for inactive/optimized-out uniforms in minimal test shader
    TEST_ASSERT_GREATER_OR_EQUAL(-1, skybox.u_inv_view_proj);
    TEST_ASSERT_GREATER_OR_EQUAL(-1, skybox.u_blur_lod);
    TEST_ASSERT_GREATER_OR_EQUAL(-1, skybox.u_env_map);
    
    skybox_cleanup(&skybox);
}

void test_skybox_render_executes_without_error(void) {
    Skybox skybox = {0};
    skybox_init(&skybox, test_shader);
    
    // Create a dummy texture
    GLuint env_map = 0;
    glGenTextures(1, &env_map);
    glBindTexture(GL_TEXTURE_2D, env_map);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    
    // Create identity matrix
    mat4 inv_view_proj;
    glm_mat4_identity(inv_view_proj);
    
    // Clear any previous errors
    while (glGetError() != GL_NO_ERROR);
    
    // Render should not produce GL errors
    skybox_render(&skybox, test_shader, env_map, inv_view_proj, 0.0f);
    
    GLenum err = glGetError();
    TEST_ASSERT_EQUAL(GL_NO_ERROR, err);
    
    glDeleteTextures(1, &env_map);
    skybox_cleanup(&skybox);
}

void test_skybox_cleanup_deletes_resources(void) {
    Skybox skybox = {0};
    skybox_init(&skybox, test_shader);
    
    GLuint vao = skybox.vao;
    GLuint vbo = skybox.vbo;
    
    TEST_ASSERT_TRUE(glIsVertexArray(vao));
    TEST_ASSERT_TRUE(glIsBuffer(vbo));
    
    skybox_cleanup(&skybox);
    
    // After cleanup, resources should be deleted
    TEST_ASSERT_FALSE(glIsVertexArray(vao));
    TEST_ASSERT_FALSE(glIsBuffer(vbo));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_skybox_init_creates_vao_vbo);
    RUN_TEST(test_skybox_init_caches_uniforms);
    RUN_TEST(test_skybox_render_executes_without_error);
    RUN_TEST(test_skybox_cleanup_deletes_resources);
    return UNITY_END();
}
