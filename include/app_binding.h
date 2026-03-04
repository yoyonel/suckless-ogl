#ifndef APP_BINDING_H
#define APP_BINDING_H

#include <GLFW/glfw3.h>

/**
 * @enum BindingCategory
 * @brief Categories for organization in the help UI.
 */
typedef enum {
	BINDING_CAT_MOVEMENT,
	BINDING_CAT_VISUALS,
	BINDING_CAT_POSTFX,
	BINDING_CAT_SYSTEM,
	BINDING_CAT_COUNT
} BindingCategory;

/**
 * @enum BindingType
 * @brief Types of actions for color-coding in the UI.
 */
typedef enum {
	BINDING_TYPE_ACTION, /**< Single direct action (default). */
	BINDING_TYPE_TOGGLE, /**< On/Off toggle. */
	BINDING_TYPE_CYCLE,  /**< Cycle through multiple values. */
	BINDING_TYPE_COUNT
} BindingType;

/**
 * @struct AppBinding
 * @brief Definition of a single key binding with its help metadata.
 */
typedef struct {
	int key;            /**< GLFW key code. */
	int mods;           /**< GLFW modifier flags (Shift, Ctrl, etc). */
	const char* action; /**< Short action name (e.g. "Move Forward"). */
	const char* desc;   /**< Detailed explanation for the dry-run/hover. */
	BindingCategory category;
	BindingType type;
} AppBinding;

enum { MAX_APP_BINDINGS = 128 };

/**
 * @struct AppBindingRegistry
 * @brief Central registry that holds application key bindings.
 */
typedef struct {
	AppBinding bindings[MAX_APP_BINDINGS];
	int count;
} AppBindingRegistry;

/**
 * @brief Initializes the binding registry and populates it with all application
 * shortcuts.
 * @param registry Pointer to the registry to initialize.
 */
void app_binding_registry_init(AppBindingRegistry* registry);

/**
 * @brief Retrieves the binding for a specific key/mods combination.
 * @param registry Pointer to the initialized registry.
 * @param key GLFW key code.
 * @param mods GLFW modifier flags.
 * @return Pointer to binding, or NULL if not found.
 */
const AppBinding* app_binding_registry_get(const AppBindingRegistry* registry,
                                           int key, int mods);

/**
 * @brief Returns the total number of registered bindings.
 * @param registry Pointer to the initialized registry.
 */
int app_binding_registry_get_count(const AppBindingRegistry* registry);

/**
 * @brief Returns a pointer to the binding at the given index.
 * @param registry Pointer to the initialized registry.
 * @param index Index of the binding to retrieve.
 */
const AppBinding* app_binding_registry_at(const AppBindingRegistry* registry,
                                          int index);

#endif /* APP_BINDING_H */
