#include "app_binding.h"

#include <stddef.h>

enum { MAX_BINDINGS = 128 };

static void add_binding_impl(AppBindingRegistry* registry, int key, int mods,
                             const char* action, const char* desc,
                             BindingCategory cat, BindingType type)
{
	if (registry->count >= MAX_APP_BINDINGS) {
		return;
	}
	AppBinding* binding = &registry->bindings[registry->count++];
	binding->key = key;
	binding->mods = mods;
	binding->action = action;
	binding->desc = desc;
	binding->category = cat;
	binding->type = type;
}

void app_binding_registry_init(AppBindingRegistry* registry)
{
	registry->count = 0;

#define add_binding(k, m, a, d, c, t) \
	add_binding_impl(registry, k, m, a, d, c, t)

	/* Movement */
	add_binding(GLFW_KEY_W, 0, "Move Forward", "Moves the camera forward.",
	            BINDING_CAT_MOVEMENT, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_S, 0, "Move Backward",
	            "Moves the camera backward.", BINDING_CAT_MOVEMENT,
	            BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_A, 0, "Move Left",
	            "Strafe the camera to the left.", BINDING_CAT_MOVEMENT,
	            BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_D, 0, "Move Right",
	            "Strafe the camera to the right.", BINDING_CAT_MOVEMENT,
	            BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_Q, 0, "Move Up", "Moves the camera upwards.",
	            BINDING_CAT_MOVEMENT, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_E, 0, "Move Down", "Moves the camera downwards.",
	            BINDING_CAT_MOVEMENT, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_C, 0, "Camera Control",
	            "Toggles mouse-driven camera orientation control.",
	            BINDING_CAT_MOVEMENT, BINDING_TYPE_TOGGLE);

	/* Visuals */
	add_binding(GLFW_KEY_Z, 0, "Toggle Wireframe",
	            "Toggles polygonal wireframe rendering mode.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_L, 0, "Toggle Billboards",
	            "Toggles billboard instancing mode for spheres.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_Y, 0, "Cycle GI Mode",
	            "Cycles between different Global Illumination methods.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_Y, GLFW_MOD_SHIFT, "Toggle Probes",
	            "Shows the 3D grid of Global Illumination probes.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_K, 0, "Toggle Skybox",
	            "Shows or hides the environment map/skybox.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_TOGGLE);
	add_binding(
	    GLFW_KEY_F5, 0, "Cycle PBR Debug",
	    "Cycles through PBR material channels (Albedo, Normals, etc).",
	    BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_UP, 0, "Increase Subdiv",
	            "Increases the geometric detail of spheres (Subdivisions).",
	            BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_DOWN, 0, "Decrease Subdiv",
	            "Decreases the geometric detail of spheres (Subdivisions).",
	            BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_PAGE_UP, 0, "Next HDR Map",
	            "Cycles to the next high dynamic range environment map.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(
	    GLFW_KEY_PAGE_DOWN, 0, "Prev HDR Map",
	    "Cycles to the previous high dynamic range environment map.",
	    BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_PAGE_UP, GLFW_MOD_SHIFT, "Env LOD Up",
	            "Increases the blurriness (LOD) of the environment map.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_PAGE_DOWN, GLFW_MOD_SHIFT, "Env LOD Down",
	            "Decreases the blurriness (LOD) of the environment map.",
	            BINDING_CAT_VISUALS, BINDING_TYPE_CYCLE);

	/* PostFX */
	add_binding(GLFW_KEY_B, 0, "Toggle Bloom",
	            "Toggles the bloom/glow effect.", BINDING_CAT_POSTFX,
	            BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_B, GLFW_MOD_SHIFT, "Cycle Bloom Debug Stage",
	            "Cycles through bloom intermediate maps and stages.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_B, GLFW_MOD_ALT, "Cycle Bloom Debug Mip",
	            "Cycles through bloom levels (mips) in debug mode.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_M, 0, "Toggle Motion Blur",
	            "Toggles the velocity-based motion blur.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_M, GLFW_MOD_SHIFT, "Cycle MB Debug",
	            "Cycles through motion blur and velocity debug views.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_J, 0, "Toggle Auto-Exposure",
	            "Enables or disables automatic scene exposure.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_J, GLFW_MOD_SHIFT, "Exposure Debug",
	            "Shows the luminance histogram for auto-exposure tuning.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_V, 0, "Toggle Vignette",
	            "Toggles the dark border vignette effect.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_G, 0, "Toggle Grain",
	            "Toggles film grain noise effect.", BINDING_CAT_POSTFX,
	            BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_H, 0, "Toggle DOF", "Toggles Depth of Field blur.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_H, GLFW_MOD_SHIFT, "Toggle DOF Debug",
	            "Shows focus areas for Depth of Field tuning.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_X, 0, "Toggle FXAA",
	            "Toggles Fast Approximate Anti-Aliasing.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_X, GLFW_MOD_SHIFT, "Toggle FXAA Debug",
	            "Shows edges detected by the FXAA algorithm.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_N, 0, "Toggle Specular AA",
	            "Toggles anti-aliasing for specular highlights.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_N, GLFW_MOD_SHIFT, "Cycle AA Mode",
	            "Cycles through available anti-aliasing methods.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_U, 0, "Toggle Chromatic",
	            "Toggles chromatic aberration effect.", BINDING_CAT_POSTFX,
	            BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_7, 0, "Style: Banding",
	            "Cycles through banding and posterization styles.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_8, 0, "FX Benchmark",
	            "Starts/Stops the effects performance benchmark.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_1, 0, "Style: Clean",
	            "Resets all post-processing to a pure, clean state.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_2, 0, "Style: Subtle",
	            "Applies a subtle, natural color grading.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_3, 0, "Style: Cinematic",
	            "Applies a high-contrast cinematic film look.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_4, 0, "Style: Vintage",
	            "Applies a faded, vintage photographic style.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_5, 0, "Style: Matrix",
	            "Applies a green-tinted digital 'Matrix' style.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_6, 0, "Style: Noir",
	            "Applies a high-contrast black and white look.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_9, 0, "Style: Nordic Noir",
	            "Foggy neon-lit night with teal-orange split toning.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_0, 0, "Reset PostFX",
	            "Resets all effects and exposure to default values.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_KP_0, 0, "Reset PostFX (KP)",
	            "Resets all effects and exposure to default values.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_KP_ADD, 0, "Increase Exposure",
	            "Manually increases the virtual camera exposure.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_KP_SUBTRACT, 0, "Decrease Exposure",
	            "Manually decreases the virtual camera exposure.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);

	/* System */
	add_binding(GLFW_KEY_F1, 0, "Cycle Overlays",
	            "Cycles through different debug information overlays.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_F2, 0, "Toggle Help",
	            "Shows or hides this interactive keyboard help.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F3, 0, "Toggle Profiler",
	            "Toggles the GPU timeline profiler visibility.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F4, 0, "Log GPU Metrics",
	            "Toggles logging of GPU metrics to the console.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F6, 0, "Stencil Debug",
	            "Toggles stencil buffer debug visualization.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F7, 0, "Toggle Fog",
	            "Toggles atmospheric fog. Use SHIFT+F7 for Debug view.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F8, 0, "Sony A7S III",
	            "Toggles Sony Alpha 7S III S-Cinetone camera profile.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F8, GLFW_MOD_SHIFT, "Cycle 3D LUT",
	            "Cycles through cinematic 3D LUT 'Characters'.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_F10, GLFW_MOD_SHIFT, "Toggle LUT Viz",
	            "Toggles 3D LUT Lattice deformation visualization.",
	            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F9, 0, "Toggle Perf Mode",
	            "Toggles Performance Mode (disables heavy effects).",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_F12, 0, "Take Screenshot",
	            "Saves the current frame as a PNG image.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_O, 0, "Cycle Sorting",
	            "Cycles through sphere sorting algorithms (CPU/GPU).",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_T, 0, "Env Transition",
	            "Cycles through environment transition modes.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_CYCLE);
	add_binding(GLFW_KEY_SPACE, 0, "Reset Camera",
	            "Resets camera position and environment settings.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_ESCAPE, 0, "Exit", "Closes the application.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_F, 0, "Toggle Fullscreen",
	            "Toggles between windowed and fullscreen mode.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_TOGGLE);
	add_binding(GLFW_KEY_P, 0, "Quick Capture",
	            "Saves current frame to 'capture_frame.png'.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_ACTION);
	add_binding(GLFW_KEY_R, 0, "Hot-Reload Shaders",
	            "Attempts to recompile all shaders during runtime.",
	            BINDING_CAT_SYSTEM, BINDING_TYPE_ACTION);

#undef add_binding
}

const AppBinding* app_binding_registry_get(const AppBindingRegistry* registry,
                                           int key, int mods)
{
	for (int i = 0; i < registry->count; i++) {
		if (registry->bindings[i].key == key &&
		    registry->bindings[i].mods == mods) {
			return &registry->bindings[i];
		}
	}
	return NULL;
}

int app_binding_registry_get_count(const AppBindingRegistry* registry)
{
	return registry->count;
}

const AppBinding* app_binding_registry_at(const AppBindingRegistry* registry,
                                          int index)
{
	if (index < 0 || index >= registry->count) {
		return NULL;
	}
	return &registry->bindings[index];
}
