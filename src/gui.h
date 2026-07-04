#ifndef GUI_H
#define GUI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct App App;

/**
 * @struct Gui_C
 * @brief Opaque wrapper around C++ Gui context and state.
 */
typedef struct Gui_C {
	void* internal_gui_ptr;
	bool visible;
} Gui_C;

/**
 * @brief Initializes ImGui context and backends.
 */
bool gui_init(Gui_C* g, void* window);

/**
 * @brief Begins a new ImGui frame.
 */
void gui_new_frame(Gui_C* g);

/**
 * @brief Updates and issues ImGui layout widgets.
 */
void gui_update(Gui_C* g, App* app);

/**
 * @brief Renders ImGui draw data.
 */
void gui_render(Gui_C* g);

/**
 * @brief Toggles ImGui window visibility.
 */
void gui_toggle(Gui_C* g);

/**
 * @brief Shuts down backends and destroys ImGui context.
 */
void gui_destroy(Gui_C* g);

/**
 * @brief Checks if ImGui wants to capture keyboard input.
 */
bool gui_wants_keyboard(Gui_C* g);

/**
 * @brief Checks if ImGui wants to capture mouse input.
 */
bool gui_wants_mouse(Gui_C* g);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
