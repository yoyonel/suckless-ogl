/**
 * @file app.h
 * @brief Core application module and state container.
 *
 * This header defines the main App structure and the high-level orchestration
 * API. It integrates all sub-modules (UI, Input, Env, Scene) for the final
 * executable.
 */

#ifndef APP_H
#define APP_H

#include "adaptive_sampler.h"
#include "app_settings.h"
#include "fps.h"
#include "gl_common.h"
#include "icosphere.h"
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "async_loader.h"
#include "billboard_rendering.h"
#include "camera.h"
#include "instanced_rendering.h"
#include "material.h"
#include "perf_timer.h"
#include "postprocess.h"
#include "shader.h"
#include "skybox.h"
#include "sphere_sorting.h"
#include "ui.h"
#include <cglm/cglm.h>

/**
 * @enum IBLState
 * @brief States for the Image-Based Lighting (IBL) progressive loading.
 */
typedef enum {
	IBL_STATE_IDLE = 0,  /**< Application is waiting for a request. */
	IBL_STATE_LUMINANCE, /**< GPU-side analysis of HDR mean luminance. */
	IBL_STATE_SPECULAR_INIT, /**< Preparation of specular map textures. */
	IBL_STATE_SPECULAR_MIPS, /**< Sliced pre-filtering of specular levels.
	                          */
	IBL_STATE_IRRADIANCE,    /**< Sliced convolution of irradiance map. */
	IBL_STATE_DONE /**< Resource cleanup and texture activation. */
} IBLState;

/**
 * @struct IBLContext
 * @brief Progressive environment processing state.
 *
 * Manages the state machine that allows high-quality IBL generation without
 * freezing the main thread.
 */
typedef struct {
	IBLState state;          /**< Current processing phase. */
	int current_mip;         /**< Mip level being computed. */
	int total_mips;          /**< Target mip count. */
	int width;               /**< Source texture width. */
	int height;              /**< Source texture height. */
	float threshold;         /**< Radiance threshold for sampling. */
	GLuint pending_hdr_tex;  /**< Source HDR texture handle. */
	GLuint pending_spec_tex; /**< Target specular map handle. */
	GLuint pending_irr_tex;  /**< Target irradiance map handle. */
	int current_slice;       /**< Cubemap face/slice being processed. */
	int total_slices;        /**< Face count (typically 6). */
	PerfTimer global_timer;  /**< Benchmarking for the entire process. */
} IBLContext;

/**
 * @struct App
 * @brief The central state container for the entire application.
 *
 * This struct encapsulates all sub-systems, GPU handles, and configuration
 * flags. It is typically allocated with SIMD alignment (via `aligned_alloc`)
 * as many child structs use `vec4` (SIMD).
 */
typedef struct App {
	/* --- Pointers and Dynamic Objects --- */
	PostProcess postprocess;      /**< Main post-processing pipeline. */
	GLFWwindow* window;           /**< The GLFW window context. */
	double last_mouse_x;          /**< Previous X for Delta calculations. */
	double last_mouse_y;          /**< Previous Y for Delta calculations. */
	double last_frame_time;       /**< Absolute time of last frame start. */
	double delta_time;            /**< Time elapsed since last frame. */
	uint64_t frame_count;         /**< Monotonic frame counter. */
	Shader* pbr_instanced_shader; /**< Shared PBR shader for opaque geo. */
	Shader*
	    pbr_billboard_shader;  /**< Shader for volumetric/alpha spheres. */
	Shader* debug_shader;      /**< Generic debug/visualization shader. */
	MaterialLib* material_lib; /**< Loaded material presets. */
	char** hdr_files;          /**< List of found HDR files in assets. */

	/* --- Sub-Modules (RAII/In-Place) --- */
	FpsCounter fps_counter;      /**< Rolling average FPS manager. */
	IcosphereGeometry geometry;  /**< High-poly sphere mesh data. */
	AdaptiveSampler fps_sampler; /**< Jitter compensation for input. */
	UIContext ui;                /**< Overlay and text rendering state. */
	InstancedGroup
	    instanced_group; /**< Managed buffers for opaque spheres. */
	BillboardGroup billboard_group; /**< Managed buffers for billboards. */

#ifdef USE_TRANSPARENT_BILLBOARDS
	SphereSorter sphere_sorter;       /**< Sorter for alpha blending. */
	SphereInstance* sphere_instances; /**< Persistent array for sorting. */
	int sphere_instance_count;        /**< Active sphere count. */
#endif

	Skybox skybox;      /**< Environment renderer. */
	Camera camera;      /**< View/Proj state. */
	IBLContext ibl_ctx; /**< IBL Loader state machine. */

	/* --- App State Flags and Values --- */
	int width;                     /**< Current window/viewport width. */
	int height;                    /**< Current window/viewport height. */
	int is_fullscreen;             /**< Fullscreen toggle state. */
	int show_exposure_debug;       /**< Enable auto-exposure histogram. */
	int pbr_debug_mode;            /**< Swap to wireframe/normal/roughness
	                                  visualization. */
	int show_imgui_demo;           /**< Reserved for ImGui integration. */
	int show_help;                 /**< Overlay help text. */
	int show_info_overlay;         /**< Show FPS and stats. */
	int text_overlay_mode;         /**< Verbosity level of text UI. */
	int saved_x, saved_y;          /**< Cached pos for window restore. */
	int saved_width, saved_height; /**< Cached size for window restore. */
	int subdivisions;              /**< LOD of the shared icosphere. */
	int wireframe;                 /**< OpenGL wireframe mode toggle. */
	int show_envmap;               /**< Draw skybox toggle. */
	int first_mouse;               /**< Input initialization flag. */
	int camera_enabled;            /**< Pause camera movement. */
	int billboard_mode;    /**< Toggle for billboard rendering path. */
	int show_debug_tex;    /**< Reserved for texture debugging. */
	int hdr_count;         /**< Number of available environment maps. */
	int current_hdr_index; /**< Index of active HDR in file list. */
	int env_map_loading;   /**< Async lock for HDR loading. */

	/* --- Global GPU Resources --- */
	GLuint sphere_vao;           /**< Shared geometry VAO. */
	GLuint sphere_vbo;           /**< Shared vertex buffer. */
	GLuint sphere_nbo;           /**< Shared normal buffer. */
	GLuint sphere_ebo;           /**< Shared index buffer. */
	GLuint quad_vbo;             /**< Shared full-screen quad (FSQ). */
	GLuint skybox_shader;        /**< Legacy skybox program ID. */
	GLuint hdr_texture;          /**< Active HDR cubemap. */
	GLuint spec_prefiltered_tex; /**< Active Specular map. */
	GLuint irradiance_tex;       /**< Active Irradiance map. */
	GLuint brdf_lut_tex;         /**< Shared BRDF lookup table. */
	GLuint empty_vao;            /**< Vertex-less drawing VAO. */
	GLuint shader_spmap;         /**< Internal IBL specular shader. */
	GLuint shader_irmap;         /**< Internal IBL irradiance shader. */
	GLuint shader_lum_pass1;     /**< Luminance downsample pass. */
	GLuint shader_lum_pass2;     /**< Mean luminance compute pass. */
	GLuint exposure_pbo; /**< Pixel Buffer Object for mean luma readback. */
	GLuint dummy_black_tex; /**< Safe fallback (0,0,0,1). */
	GLuint dummy_white_tex; /**< Safe fallback (1,1,1,1). */
	GLuint lum_ssbo[2];     /**< Double-buffered storage for luminance. */

	/* --- Global Configuration Uniforms --- */
	float env_lod;          /**< Skybox blurriness. */
	float debug_lod;        /**< Reserved for texture debugging. */
	float u_metallic;       /**< Override metallic for all objects. */
	float u_roughness;      /**< Override roughness for all objects. */
	float u_ao;             /**< Override AO for all objects. */
	float u_exposure;       /**< Manual exposure compensation. */
	float auto_threshold;   /**< Dynamic exposure target. */
	float current_exposure; /**< Integrated GPU exposure value. */

#ifdef USE_SSBO_RENDERING
	SSBOGroup ssbo_group;    /**< SSBO rendering context. */
	Shader* pbr_ssbo_shader; /**< Optimized SSBO shader. */
#endif

} App;

/* --- Core Control Flow --- */

/**
 * @brief Fully initializes the application state, window, and OpenGL context.
 * @param app Pointer to the aligned App struct.
 * @param width Window width.
 * @param height Window height.
 * @param title Window title.
 * @return 1 on success, 0 on fatal error.
 */
int app_init(App* app, int width, int height, const char* title);

/**
 * @brief Safely releases all GPU and CPU resources held by the application.
 * @param app Pointer to the application state.
 */
void app_cleanup(App* app);

/**
 * @brief Enters the main application rendering and event loop.
 * @param app Pointer to the application state.
 * @note Blocks until the window is closed.
 */
void app_run(App* app);

/**
 * @brief One-frame logic update (physics, timers, camera).
 * @param app Pointer to the application state.
 */
void app_update(App* app);

/**
 * @brief One-frame rendering orchestration.
 * @param app Pointer to the application state.
 */
void app_render(App* app);

#include "app_env.h"
#include "app_input.h"
#include "app_scene.h"
#include "app_ui.h"

#endif /* APP_H */
