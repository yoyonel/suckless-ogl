#ifndef POSTPROCESS_INPUT_H
#define POSTPROCESS_INPUT_H

#include "action_notifier.h"
#include "effect_benchmark.h"
#include "postprocess.h"
#include <GLFW/glfw3.h>

/**
 * @struct PostProcessInputContext
 * @brief Context required for post-processing input handling.
 *
 * Encapsulates the state required to toggle effects and manage presets,
 * decoupling the input logic from the main App struct.
 */
typedef struct {
	PostProcess* postprocess; /**< The post-processing pipeline state. */
	ActionNotifier* notifier; /**< System for user feedback messages. */
	EffectBenchmark* effect_bench; /**< Benchmarking tool state. */
	GLFWwindow* window; /**< Window handle for modifier checks. */
} PostProcessInputContext;

/**
 * @brief Handles key input events related to post-processing.
 *
 * Processes keys for toggling effects (Bloom, DOF, etc.), cycling presets,
 * and adjusting exposure.
 *
 * @param ctx Pointer to the input context.
 * @param key The GLFW key code.
 * @param mods The GLFW modifier flags.
 */
void postprocess_input_handle_key(const PostProcessInputContext* ctx, int key,
                                  int mods);

#endif /* POSTPROCESS_INPUT_H */
