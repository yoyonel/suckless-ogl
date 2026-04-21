/**
 * @file app_ui.h
 * @brief UI rendering module for the application.
 *
 * This module handles all UI-related drawing operations, including help
 * overlays, debug information, and luminance statistics.
 */

#ifndef APP_UI_H
#define APP_UI_H
#include "app_settings.h"
#include "glad/glad.h"
#include "ui.h"
#include <GLFW/glfw3.h>
#include <cglm/types.h>
#include <stdbool.h>

/* UI Scaling and Layout Defaults */
static const float DEFAULT_BASE_RESOLUTION_HEIGHT = 1080.0F;
static const float DEFAULT_KBD_KEY_SIZE = 60.0F;
static const float DEFAULT_KBD_KEY_PADDING = 10.0F;
static const float DEFAULT_KBD_KEY_RADIUS = 10.0F;
static const float DEFAULT_KBD_LABEL_SCALE = 0.75F;
static const float DEFAULT_KBD_TITLE_Y_OFFSET = 140.0F;
static const float DEFAULT_KBD_DETAIL_Y_OFFSET = 100.0F;

/* Layout and Framing */
static const float LUMINANCE_EPSILON = 0.0001F;
static const float UI_CENTER_FACTOR = 0.5F;
static const float GRAPH_TEXT_PADDING = 20.0F;
static const float DEBUG_TEXT_Y_OFFSET = DEFAULT_FONT_SIZE * 4.0F;

/* Legend Layout Factors */
static const float LEGEND_Y_FACTOR = 0.6F;
static const float LEGEND_X_START_FACTOR = 0.25F;
static const float LEGEND_STEP_FACTOR = 0.18F;
static const float LEGEND_COMBO_STEP_MULT = 1.9F;

/* Histogram and Graph Limits */
static const float GRAPH_POS_X = 20.0F;
static const float GRAPH_POS_Y_OFF = 200.0F;
static const float GRAPH_DIM_W = 300.0F;
static const float GRAPH_DIM_H = 100.0F;
enum { HISTO_BUCKETS = 256 };

/* Cyberpunk overlay visual tweaks */
static const float DEFAULT_KBD_BLOOM_SCALE = 1.3F;
static const float DEFAULT_KBD_UNBOUND_ALPHA = 0.22F;
static const float DEFAULT_KBD_BOUND_ALPHA = 0.88F;
static const float PANEL_FRAME_ALPHA = 0.72F;
static const float BLOOM_MAX_INTENSITY = 0.5F;
static const float GLOW_HOVER_ALPHA = 0.6F;
static const float BLOOM_PRESS_MAX_ALPHA = 0.9F;
static const float HELP_BG_ALPHA = 0.88F;
static const float KEY_PRESSED_ALPHA = 0.95F;
static const float KEY_DEFAULT_ALPHA = 0.4F;
static const float KEY_PRESS_BRIGHTEN_MIN = 0.6F;
static const float HELP_TEXT_ALPHA = 0.8F;
static const float HELP_EXIT_HINT_X_OFF = 300.0F;
static const float HELP_EXIT_HINT_Y_OFF = 50.0F;

/* UI Colors */
static const vec3 KEY_COLOR_DEFAULT = {0.15F, 0.15F, 0.20F};
static const vec3 KEY_COLOR_TOGGLE = {0.0F, 0.82F, 0.92F};
static const vec3 KEY_COLOR_CYCLE = {0.12F, 0.90F, 0.12F};
static const vec3 KEY_COLOR_COMBINATION = {1.0F, 0.56F, 0.05F};
static const vec3 HELP_BG_COLOR = {0.05F, 0.05F, 0.07F};
static const vec3 CYBER_TITLE_COLOR = {0.0F, 0.90F, 0.95F};
static const vec3 ENV_TEXT_COLOR = {0.7F, 0.7F, 0.7F};
static const vec3 GRAPH_TEXT_COLOR = {0.8F, 0.8F, 0.8F};
static const vec3 DEBUG_ORANGE_COLOR = {1.0F, 0.5F, 0.0F};
static const vec3 HISTO_BAR_COLOR_GREEN = {0.0F, 0.7F, 0.0F};
static const vec3 HISTO_BAR_COLOR_BLUE = {0.0F, 0.5F, 0.8F};
static const vec3 HISTO_BAR_COLOR_RED = {0.8F, 0.5F, 0.0F};

/* Animation and Timing */
static const float GLOBAL_DIM_MAX_FALLOFF = 0.7F;
static const float GLOBAL_DIM_SMOOTH_FACTOR = 15.0F;
static const double HOVER_DECAY_DURATION = 0.15;
static const float HELP_PRESS_DURATION = 2.0F;

/* Loading Indicator and Spinner */
static const double UI_SPINNER_SPEED = 10.0;
static const float UI_LOADING_TEXT_WIDTH_FACTOR = 20.0F;
static const float UI_SPINNER_SIZE = 64.0F;
static const float UI_TEXT_OFFSET_FACTOR = 0.8F;
static const vec3 UI_SPINNER_COLOR = {90.0F / 255.0F, 111.0F / 255.0F,
                                      185.0F / 255.0F};
enum { UI_LOADING_TEXT_SIZE = 64 };

/* UI Buffers and Limits */
enum {
	DEBUG_TEXT_BUFFER_SIZE = 128,
	RANGE_TEXT_BUFFER_SIZE = 64,
	ENV_TEXT_BUFFER_SIZE = 256,
	EXPOSURE_TEXT_BUFFER_SIZE = 64,
	MODIFIER_BUFFER_SIZE = 16,
	KEYBOARD_BUFFER_SIZE = 256
};

/**
 * @enum HelpMode
 * @brief States for the F2 help overlay cycling.
 */
typedef enum {
	HELP_MODE_OFF = 0,
	HELP_MODE_KEYBOARD,
	HELP_MODE_GAMEPAD,
	HELP_MODE_COUNT
} HelpMode;

enum {
	ROW_SYSTEM = 0,
	ROW_NUMBERS = 1,
	ROW_QWERTY = 2,
	ROW_ASDF = 3,
	ROW_ZXCV = 4,
	ROW_BOTTOM = 5
};

typedef struct {
	int key;
	int row;
	float x_off; /* In units of KEY_SIZE + KEY_PADDING */
	float width; /* In units of KEY_SIZE */
	const char* label;
} KeyPos;

typedef struct {
	float key_size;
	float key_padding;
	float key_radius;
	float label_scale;
	float title_y_offset;
	float detail_y_offset;
} KeyboardLayoutConfig;

static const KeyPos KEY_LAYOUT_QWERTY[] = {
    /* Row 0: Esc + Func */
    {GLFW_KEY_ESCAPE, ROW_SYSTEM, 0.0F, 1.0F, "Esc"},
    {GLFW_KEY_F1, ROW_SYSTEM, 2.0F, 1.0F, "F1"},
    {GLFW_KEY_F2, ROW_SYSTEM, 3.0F, 1.0F, "F2"},
    {GLFW_KEY_F3, ROW_SYSTEM, 4.0F, 1.0F, "F3"},
    {GLFW_KEY_F4, ROW_SYSTEM, 5.0F, 1.0F, "F4"},
    {GLFW_KEY_F5, ROW_SYSTEM, 6.5F, 1.0F, "F5"},
    {GLFW_KEY_F6, ROW_SYSTEM, 7.5F, 1.0F, "F6"},
    {GLFW_KEY_F7, ROW_SYSTEM, 8.5F, 1.0F, "F7"},
    {GLFW_KEY_F8, ROW_SYSTEM, 9.5F, 1.0F, "F8"},
    {GLFW_KEY_F9, ROW_SYSTEM, 11.5F, 1.0F, "F9"},
    {GLFW_KEY_F10, ROW_SYSTEM, 12.5F, 1.0F, "F10"},
    {GLFW_KEY_F11, ROW_SYSTEM, 13.5F, 1.0F, "F11"},
    {GLFW_KEY_F12, ROW_SYSTEM, 14.5F, 1.2F, "F12"},

    /* Row 1: Numbers/Symbols */
    {GLFW_KEY_GRAVE_ACCENT, ROW_NUMBERS, 0.0F, 1.0F, "~"},
    {GLFW_KEY_1, ROW_NUMBERS, 1.0F, 1.0F, "1"},
    {GLFW_KEY_2, ROW_NUMBERS, 2.0F, 1.0F, "2"},
    {GLFW_KEY_3, ROW_NUMBERS, 3.0F, 1.0F, "3"},
    {GLFW_KEY_4, ROW_NUMBERS, 4.0F, 1.0F, "4"},
    {GLFW_KEY_5, ROW_NUMBERS, 5.0F, 1.0F, "5"},
    {GLFW_KEY_6, ROW_NUMBERS, 6.0F, 1.0F, "6"},
    {GLFW_KEY_7, ROW_NUMBERS, 7.0F, 1.0F, "7"},
    {GLFW_KEY_8, ROW_NUMBERS, 8.0F, 1.0F, "8"},
    {GLFW_KEY_9, ROW_NUMBERS, 9.0F, 1.0F, "9"},
    {GLFW_KEY_0, ROW_NUMBERS, 10.0F, 1.0F, "0"},

    /* Row 2: QWERTY */
    {GLFW_KEY_TAB, ROW_QWERTY, 0.0F, 1.5F, "Tab"},
    {GLFW_KEY_Q, ROW_QWERTY, 1.5F, 1.0F, "Q"},
    {GLFW_KEY_W, ROW_QWERTY, 2.5F, 1.0F, "W"},
    {GLFW_KEY_E, ROW_QWERTY, 3.5F, 1.0F, "E"},
    {GLFW_KEY_R, ROW_QWERTY, 4.5F, 1.0F, "R"},
    {GLFW_KEY_T, ROW_QWERTY, 5.5F, 1.0F, "T"},
    {GLFW_KEY_Y, ROW_QWERTY, 6.5F, 1.0F, "Y"},
    {GLFW_KEY_U, ROW_QWERTY, 7.5F, 1.0F, "U"},
    {GLFW_KEY_I, ROW_QWERTY, 8.5F, 1.0F, "I"},
    {GLFW_KEY_O, ROW_QWERTY, 9.5F, 1.0F, "O"},
    {GLFW_KEY_P, ROW_QWERTY, 10.5F, 1.0F, "P"},

    /* Row 3: ASDF */
    {GLFW_KEY_CAPS_LOCK, ROW_ASDF, 0.0F, 1.8F, "Caps"},
    {GLFW_KEY_A, ROW_ASDF, 1.8F, 1.0F, "A"},
    {GLFW_KEY_S, ROW_ASDF, 2.8F, 1.0F, "S"},
    {GLFW_KEY_D, ROW_ASDF, 3.8F, 1.0F, "D"},
    {GLFW_KEY_F, ROW_ASDF, 4.8F, 1.0F, "F"},
    {GLFW_KEY_G, ROW_ASDF, 5.8F, 1.0F, "G"},
    {GLFW_KEY_H, ROW_ASDF, 6.8F, 1.0F, "H"},
    {GLFW_KEY_J, ROW_ASDF, 7.8F, 1.0F, "J"},
    {GLFW_KEY_K, ROW_ASDF, 8.8F, 1.0F, "K"},
    {GLFW_KEY_L, ROW_ASDF, 9.8F, 1.0F, "L"},

    /* Row 4: ZXCV */
    {GLFW_KEY_LEFT_SHIFT, ROW_ZXCV, 0.0F, 2.3F, "Shift"},
    {GLFW_KEY_Z, ROW_ZXCV, 2.3F, 1.0F, "Z"},
    {GLFW_KEY_X, ROW_ZXCV, 3.3F, 1.0F, "X"},
    {GLFW_KEY_C, ROW_ZXCV, 4.3F, 1.0F, "C"},
    {GLFW_KEY_V, ROW_ZXCV, 5.3F, 1.0F, "V"},
    {GLFW_KEY_B, ROW_ZXCV, 6.3F, 1.0F, "B"},
    {GLFW_KEY_N, ROW_ZXCV, 7.3F, 1.0F, "N"},
    {GLFW_KEY_M, ROW_ZXCV, 8.3F, 1.0F, "M"},

    /* Row 5: Space/System */
    {GLFW_KEY_LEFT_CONTROL, ROW_BOTTOM, 0.0F, 1.5F, "Ctrl"},
    {GLFW_KEY_SPACE, ROW_BOTTOM, 3.8F, 6.0F, "Space"},
    {GLFW_KEY_PAGE_UP, ROW_ZXCV, 13.5F, 1.3F, "PgUp"},
    {GLFW_KEY_PAGE_DOWN, ROW_BOTTOM, 13.5F, 1.3F, "PgDn"},
    {GLFW_KEY_UP, ROW_ZXCV, 15.0F, 1.0F, "Up"},
    {GLFW_KEY_DOWN, ROW_BOTTOM, 15.0F, 1.0F, "Dn"}};

/* --- Gamepad overlay layout --- */

/**
 * @struct GamepadControlPos
 * @brief Position and metadata for a single gamepad control in the overlay.
 */
typedef struct {
	float x_off;   /**< Horizontal offset in layout units. */
	float y_off;   /**< Vertical offset in layout units. */
	float width;   /**< Width in layout units. */
	float height;  /**< Height in layout units. */
	int is_bound;  /**< Non-zero if this control has an active binding. */
	int bind_type; /**< 0=action, 1=toggle, 2=cycle. */
	int gp_btn;    /**< GLFW button index, or -1 if not a button. */
	int gp_axis;   /**< GLFW axis index, or -1 if not an axis. */
	int gp_axis2;  /**< Second axis for sticks, or -1. */
	const char* label;  /**< Short label drawn on the control. */
	const char* action; /**< Action name (e.g. "Camera Move"). */
	const char* desc;   /**< Detailed description. */
} GamepadControlPos;

enum {
	GP_ROW_TRIGGERS = 0,
	GP_ROW_BUMPERS = 1,
	GP_ROW_FACE = 2,
	GP_ROW_STICKS = 3,
	GP_ROW_SYSTEM = 4
};

/* Layout: 12 units wide × 7 units tall, DualShock-inspired.
 * Left side: L2, L1, D-pad, L-Stick.
 * Right side: R2, R1, face buttons, R-Stick.
 * Center: Share, Options.
 */
static const GamepadControlPos GAMEPAD_LAYOUT[] = {
    /* L2 / R2 (triggers — wide, top) */
    {0.5F, 0.0F, 2.5F, 0.8F, 1, 0, -1, GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, -1, "L2",
     "Move Down", "Left trigger: moves the camera downward (proportional)."},
    {9.0F, 0.0F, 2.5F, 0.8F, 1, 0, -1, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, -1,
     "R2", "Move Up", "Right trigger: moves the camera upward (proportional)."},

    /* L1 / R1 (bumpers — narrower, below triggers) */
    {0.5F, 1.1F, 2.5F, 0.6F, 1, 2, GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, -1, -1,
     "L1", "Prev Env", "Left bumper: cycles to the previous environment map."},
    {9.0F, 1.1F, 2.5F, 0.6F, 1, 2, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, -1, -1,
     "R1", "Next Env", "Right bumper: cycles to the next environment map."},

    /* D-pad (left side, unbound) */
    {1.5F, 2.8F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_DPAD_UP, -1, -1,
     "\xE2\x96\xB2", "", ""},
    {0.8F, 3.5F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, -1, -1,
     "\xE2\x97\x80", "", ""},
    {2.2F, 3.5F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, -1, -1,
     "\xE2\x96\xB6", "", ""},
    {1.5F, 4.2F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, -1, -1,
     "\xE2\x96\xBC", "", ""},

    /* Face buttons (right side, unbound) */
    {10.0F, 2.8F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_Y, -1, -1, "Y", "", ""},
    {9.3F, 3.5F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_X, -1, -1, "X", "", ""},
    {10.7F, 3.5F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_B, -1, -1, "B", "", ""},
    {10.0F, 4.2F, 0.7F, 0.7F, 0, 0, GLFW_GAMEPAD_BUTTON_A, -1, -1, "A", "", ""},

    /* Left Stick (active — camera movement) */
    {1.0F, 5.5F, 1.8F, 1.2F, 1, 0, -1, GLFW_GAMEPAD_AXIS_LEFT_X,
     GLFW_GAMEPAD_AXIS_LEFT_Y, "L Stick", "Camera Move",
     "Left analog stick: proportional camera movement (forward/back/strafe)."},

    /* Right Stick (active — camera look) */
    {9.2F, 5.5F, 1.8F, 1.2F, 1, 0, -1, GLFW_GAMEPAD_AXIS_RIGHT_X,
     GLFW_GAMEPAD_AXIS_RIGHT_Y, "R Stick", "Camera Look",
     "Right analog stick: proportional camera look (yaw/pitch)."},

    /* Center buttons */
    {4.5F, 3.5F, 1.2F, 0.6F, 1, 1, GLFW_GAMEPAD_BUTTON_BACK, -1, -1, "Share",
     "Camera Reset", "Reset camera position, orientation and LOD to defaults."},
    {6.3F, 3.5F, 1.2F, 0.6F, 0, 0, GLFW_GAMEPAD_BUTTON_START, -1, -1, "Options",
     "", ""},
};

enum {
	GAMEPAD_LAYOUT_COUNT =
	    sizeof(GAMEPAD_LAYOUT) / sizeof(GAMEPAD_LAYOUT[0])
};

typedef struct {
	UIContext ui;
	KeyboardLayoutConfig kbd_config;

	HelpMode show_help;
	int show_info_overlay;
	int text_overlay_mode;
	int show_exposure_debug;

	int help_hovered_key;
	int help_pressed_key;
	int help_pressed_mods;
	double help_press_timer;
	double help_global_dim;
	double help_hover_decay; /**< Grace period for dimming when mouse leaves
	                          * a key
	                          */
	double help_gp_decay; /**< Grace period for dimming when gamepad input
	                       * stops — same role as help_hover_decay but for
	                       * gamepad controls.
	                       */

	/* Cyberpunk keyboard overlay textures (PNG assets) */
	GLuint
	    kbd_tex_frame; /**< Panel frame texture (cyan border + scanlines) */
	GLuint
	    kbd_tex_key_base; /**< Single keycap texture (tinted per binding) */

	/** Set to 1 when the overlay auto-disabled the camera on open, so we
	 *  can re-enable it exactly the same way (simulate 'C' press) on close.
	 */
	int help_captured_camera;
} AppUIOverlay;

typedef struct App App;

/**
 * @brief Draws the help overlay with keyboard shortcuts.
 * @param app Pointer to the application state.
 */
void app_draw_help_overlay(const App* app);

/**
 * @brief Draws the gamepad help overlay with controller bindings.
 * @param app Pointer to the application state.
 */
void app_draw_gamepad_help_overlay(const App* app);

/**
 * @brief Draws the debug overlay with performance metrics and settings.
 * @param app Pointer to the application state.
 */
void app_draw_debug_overlay(const App* app);

/**
 * @brief Main UI rendering entry point.
 *
 * Orchestrates all UI elements (overlays, histograms, loading spinners).
 * @param app Pointer to the application state.
 */
void app_render_ui(const App* app);

/* --- Internal helper functions --- */

/**
 * @brief Computes the luminance histogram from current frame data.
 *
 * Scans the luminance buffer to populate buckets for visualization.
 * @param app Pointer to the application state.
 * @param buckets Array to fill with histogram data.
 * @param size Number of buckets in the array.
 * @param[out] min_lum Minimum luminance found in the data.
 * @param[out] max_lum Maximum luminance found in the data.
 * @return Number of samples processed or error code.
 */
int compute_luminance_histogram(const App* app, int* buckets, int size,
                                float* min_lum, float* max_lum);

void app_ui_init(AppUIOverlay* overlay);
void app_ui_cleanup(AppUIOverlay* overlay);
void app_ui_update(AppUIOverlay* overlay, double delta_time);
void app_ui_handle_mouse(AppUIOverlay* overlay, double xpos, double ypos,
                         int width, int height);

#endif /* APP_UI_H */
