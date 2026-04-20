#ifndef _glfw3_h_
#define _glfw3_h_

typedef void GLFWwindow;
typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWvidmode {
	int width;
	int height;
	int refreshRate;
} GLFWvidmode;

#define GLFW_PRESS 1
#define GLFW_RELEASE 0
#define GLFW_REPEAT 2

#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_W 87
#define GLFW_KEY_A 65
#define GLFW_KEY_S 83
#define GLFW_KEY_D 68
#define GLFW_KEY_Q 81
#define GLFW_KEY_E 69
#define GLFW_KEY_P 80
#define GLFW_KEY_Z 90
#define GLFW_KEY_C 67
#define GLFW_KEY_F 70
#define GLFW_KEY_G 71
#define GLFW_KEY_H 72
#define GLFW_KEY_J 74
#define GLFW_KEY_K 75
#define GLFW_KEY_L 76
#define GLFW_KEY_M 77
#define GLFW_KEY_R 82
#define GLFW_KEY_T 84
#define GLFW_KEY_V 86
#define GLFW_KEY_X 88
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_UP 265
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_PAGE_UP 266
#define GLFW_KEY_PAGE_DOWN 267
#define GLFW_KEY_F1 290
#define GLFW_KEY_F2 291
#define GLFW_KEY_F3 292
#define GLFW_KEY_F4 293
#define GLFW_KEY_F5 294
#define GLFW_KEY_F6 295
#define GLFW_KEY_F9 298
#define GLFW_KEY_F12 301
#define GLFW_KEY_0 48
#define GLFW_KEY_1 49
#define GLFW_KEY_2 50
#define GLFW_KEY_3 51
#define GLFW_KEY_4 52
#define GLFW_KEY_5 53
#define GLFW_KEY_6 54
#define GLFW_KEY_7 55
#define GLFW_KEY_8 56
#define GLFW_KEY_KP_0 320
#define GLFW_KEY_KP_ADD 334
#define GLFW_KEY_KP_SUBTRACT 333

#define GLFW_MOD_SHIFT 0x0001
#define GLFW_KEY_LEFT_SHIFT 340
#define GLFW_KEY_RIGHT_SHIFT 344

#define GLFW_CURSOR 0x00033001
#define GLFW_CURSOR_NORMAL 0x00034001
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_TRUE 1
#define GLFW_FALSE 0

/* Joystick / Gamepad */
#define GLFW_JOYSTICK_1 0
#define GLFW_GAMEPAD_AXIS_LEFT_X 0
#define GLFW_GAMEPAD_AXIS_LEFT_Y 1
#define GLFW_GAMEPAD_AXIS_RIGHT_X 2
#define GLFW_GAMEPAD_AXIS_RIGHT_Y 3
#define GLFW_GAMEPAD_AXIS_LEFT_TRIGGER 4
#define GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define GLFW_GAMEPAD_BUTTON_A 0
#define GLFW_GAMEPAD_BUTTON_B 1
#define GLFW_GAMEPAD_BUTTON_X 2
#define GLFW_GAMEPAD_BUTTON_Y 3
#define GLFW_GAMEPAD_BUTTON_LEFT_BUMPER 4
#define GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER 5
#define GLFW_GAMEPAD_BUTTON_CROSS GLFW_GAMEPAD_BUTTON_A

typedef struct {
	unsigned char buttons[15];
	float axes[6];
} GLFWgamepadstate;

int glfwJoystickIsGamepad(int jid);
int glfwGetGamepadState(int jid, GLFWgamepadstate* state);
const char* glfwGetGamepadName(int jid);

void* glfwGetWindowUserPointer(GLFWwindow* window);
void glfwSetWindowShouldClose(GLFWwindow* window, int value);
void glfwSetInputMode(GLFWwindow* window, int mode, int value);
int glfwGetKey(GLFWwindow* window, int key);
double glfwGetTime(void);
GLFWmonitor* glfwGetPrimaryMonitor(void);
const GLFWvidmode* glfwGetVideoMode(GLFWmonitor* monitor);
void glfwGetWindowPos(GLFWwindow* window, int* xpos, int* ypos);
void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);
void glfwSetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int xpos,
                          int ypos, int width, int height, int refreshRate);
void glfwFocusWindow(GLFWwindow* window);

typedef void (*GLFWscrollfun)(GLFWwindow* window, double xoffset,
                              double yoffset);
typedef void (*GLFWframebuffersizefun)(GLFWwindow* window, int width,
                                       int height);

void glfwSetScrollCallback(GLFWwindow* window, GLFWscrollfun callback);
void glfwSetFramebufferSizeCallback(GLFWwindow* window,
                                    GLFWframebuffersizefun callback);
int glfwWindowShouldClose(GLFWwindow* window);
void glfwSwapBuffers(GLFWwindow* window);
void glfwPollEvents(void);

#endif
