#include "gui.h"

extern "C" {
#include "app.h"
#include "app_input_state.h"
#include "app_profiling.h"
#include "camera.h"
#include "env_manager.h"
#include "gpu_profiler.h"
#include "ibl_coordinator.h"
#include "log.h"
#include "postprocess_internal.h"
#include "postprocess_presets.h"
#include "postprocess_setters.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "window.h"
}

#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define SEARCH_BUF_SIZE 128
#define GLSL_VERSION "#version 440 core"

// Glasgow Palette for split line markers (as color codes in ABGR / RGBA)
static const ImVec4 GLASBEY_PALETTE[] = {
    ImVec4(0.0f, 0.0f, 0.5f, 1.0f), ImVec4(0.0f, 0.5f, 0.0f, 1.0f),
    ImVec4(0.5f, 0.0f, 0.0f, 1.0f), ImVec4(0.0f, 0.5f, 0.5f, 1.0f),
    ImVec4(0.5f, 0.0f, 0.5f, 1.0f), ImVec4(0.5f, 0.5f, 0.0f, 1.0f),
    ImVec4(0.5f, 0.5f, 0.5f, 1.0f), ImVec4(0.0f, 0.0f, 1.0f, 1.0f)};

struct GuiState {
	ImGuiContext* ctx = nullptr;
	int active_tab = 0;
	int restore_tab = 0;
	char search_buf[SEARCH_BUF_SIZE] = {0};
	bool focus_search = false;
	bool ibl_debug_open = false;
	int ibl_scroll_target = 0;  // 0 = None, 1 = Env_Map, 2 = Irradiance, 3
	                            // = Prefilter, 4 = BRDF_LUT
	float ibl_preview_size = 256.0f;
	int ibl_mip_level = 0;
	GLuint ibl_prefilter_id = 0;
	float ibl_debug_exposure = 0.0f;
	GLuint inspector_fbo = 0;

	// Pixel inspector state
	bool inspect_active = false;
	float inspect_uv[2] = {0.0f, 0.0f};
	GLuint inspect_tex_id = 0;
	int inspect_tex_w = 0;
	int inspect_tex_h = 0;
	int inspect_mip = 0;
	float inspect_pixel[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	// Local draft state for compute tuning
	bool compute_tuning_loaded = false;
	int compute_tuning_selected_idx = 0;
	char compute_tuning_save_name[64] = {0};
	const char* compute_tuning_status_msg = nullptr;
	float compute_tuning_status_timer = 0.0f;
	const char* compute_tuning_error_msg = nullptr;
	float compute_tuning_error_timer = 0.0f;

	// Local draft parameters (mirrors settings struct in Odin)
	int spbrdf_sample_count = 1024;
	int spmap_sample_count = 512;
	float irmap_sample_delta = 0.025f;
	int mip0_slices = 1;
	int mip1_slices = 2;
	int mip2_slices = 4;
	int irdiff_slices = 8;
	int grouping_start_mip = 3;
	int downsample_progressive_threshold = 4;
};

// Forward declare local helpers
static std::vector<float> read_texture_pixel(GuiState* g, GLuint tex_id, int x,
                                             int y, int mip_level);
static void draw_image_with_inspector(GuiState* g, GLuint tex_id,
                                      ImVec2 display_size, int tex_w, int tex_h,
                                      int mip_level = 0);
static bool fuzzy_match(const char* filter, const char* label,
                        const char* keywords);
static bool section_has_matches(const char* filter, const char* keywords);

extern "C" bool gui_init(Gui_C* g, void* window)
{
	LOG_INFO("suckless-ogl.gui", "Initializing Dear ImGui (v1.92.4)...");

	IMGUI_CHECKVERSION();
	GuiState* state = new GuiState();
	g->internal_gui_ptr = state;
	g->visible = false;

	state->ctx = ImGui::CreateContext();
	if (!state->ctx) {
		LOG_ERROR("suckless-ogl.gui", "Failed to create ImGui context");
		delete state;
		g->internal_gui_ptr = nullptr;
		return false;
	}
	LOG_DEBUG("suckless-ogl.gui", "ImGui context created successfully");

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	GLFWwindow* win = static_cast<GLFWwindow*>(window);
	if (!ImGui_ImplGlfw_InitForOpenGL(win, true)) {
		LOG_ERROR("suckless-ogl.gui",
		          "Failed to initialize ImGui GLFW backend");
		ImGui::DestroyContext(state->ctx);
		delete state;
		g->internal_gui_ptr = nullptr;
		return false;
	}
	LOG_DEBUG("suckless-ogl.gui",
	          "ImGui GLFW backend initialized successfully");

	if (!ImGui_ImplOpenGL3_Init(GLSL_VERSION)) {
		LOG_ERROR("suckless-ogl.gui",
		          "Failed to initialize ImGui OpenGL3 backend");
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext(state->ctx);
		delete state;
		g->internal_gui_ptr = nullptr;
		return false;
	}
	LOG_DEBUG("suckless-ogl.gui",
	          "ImGui OpenGL3 backend initialized successfully");

	LOG_INFO("suckless-ogl.gui",
	         "Dear ImGui backend and context loaded successfully");
	return true;
}

extern "C" void gui_new_frame(Gui_C* g)
{
	GuiState* state = static_cast<GuiState*>(g->internal_gui_ptr);
	if (!state || !state->ctx)
		return;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

static void draw_tab_camera(Camera* c)
{
	if (!c)
		return;

	ImGui::Text("Position: %.1f, %.1f, %.1f", c->position[0],
	            c->position[1], c->position[2]);
	ImGui::Text("Yaw: %.1f  Pitch: %.1f", c->yaw, c->pitch);
	ImGui::Separator();

	ImGui::SliderFloat("Speed", &c->velocity, 1.0f, 100.0f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Maximum movement speed (units/sec)\nHigher = faster "
		    "camera travel");
	}
	ImGui::SliderFloat("Acceleration", &c->acceleration, 1.0f, 50.0f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "How quickly camera reaches max speed\nHigher = snappier "
		    "response, Lower = more inertia");
	}
	ImGui::SliderFloat("Friction", &c->friction, 0.5f, 0.99f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Velocity damping per frame\n0.5 = stops fast (heavy), "
		    "0.99 = slides long (ice)");
	}
	ImGui::SliderFloat("Sensitivity", &c->sensitivity, 0.01f, 1.0f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Mouse look sensitivity\nMultiplier on raw mouse delta for "
		    "yaw/pitch rotation");
	}
	ImGui::SliderFloat("Rotation Smoothing", &c->rotation_smoothing, 0.0f,
	                   0.5f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Lerp factor for yaw/pitch interpolation\n0 = instant (no "
		    "smoothing), 0.5 = heavy lag");
	}
	ImGui::SliderFloat("Mouse Smoothing", &c->mouse_smoothing_factor, 0.0f,
	                   0.5f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "EMA filter on raw mouse input\n0 = raw (no filter), 0.5 = "
		    "heavy averaging\nReduces jitter at cost of latency");
	}
	ImGui::SliderFloat("FOV", &c->zoom, 10.0f, 120.0f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Vertical field of view (degrees)\n60 = standard, 90 = "
		    "wide, 10 = telephoto zoom");
	}
	ImGui::Separator();

	ImGui::Checkbox("Head Bobbing", &c->bobbing_enabled);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Simulates head bob during movement\nAdds subtle vertical "
		    "oscillation to camera position");
	}
	if (c->bobbing_enabled) {
		ImGui::SliderFloat("Bobbing Freq", &c->bobbing_frequency, 0.5f,
		                   10.0f);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
			    "Oscillation frequency (Hz)\nHigher = faster "
			    "bobbing cycle");
		}
		ImGui::SliderFloat("Bobbing Amp", &c->bobbing_amplitude, 0.0f,
		                   0.01f);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
			    "Vertical displacement amplitude (world "
			    "units)\nHigher = more pronounced head movement");
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Reset Camera")) {
		camera_init(c, DEFAULT_CAMERA_DISTANCE, DEFAULT_CAMERA_YAW,
		            DEFAULT_CAMERA_PITCH);
	}
}

static void draw_tab_scene(Scene* scene, PostProcess* postproc)
{
	if (!scene)
		return;

	ImGui::Checkbox("Skybox", &scene->config.show_envmap);
	ImGui::SliderFloat("Skybox Blur", &scene->config.env_lod, 0.0f, 8.0f);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "LOD level for background blur\n0 = sharp, 8 = maximum "
		    "blur");
	}

	int subdiv = scene->config.subdivisions;
	if (ImGui::SliderInt("Subdivisions", &subdiv, MIN_SUBDIV, MAX_SUBDIV)) {
		scene->config.subdivisions = subdiv;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Shared icosphere mesh detail level\n0 = 12 vertices "
		    "(fast), 6 = ~40k vertices (smooth)");
	}

	int pbr_mode = scene->config.pbr_debug_mode;
	const char* pbr_debug_modes[] = {
	    "Final PBR",         "Albedo",           "Normal",
	    "Metallic",          "Roughness",        "AO",
	    "Irradiance (Diff)", "Prefilter (Spec)", "BRDF LUT",
	    "GI Probes"};
	if (ImGui::Combo("PBR Debug Mode", &pbr_mode, pbr_debug_modes, 10)) {
		scene->config.pbr_debug_mode = pbr_mode;
	}

	ImGui::Checkbox("Wireframe", &scene->config.wireframe);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Render billboard quads or icospheres as wireframe");
	}
	ImGui::Separator();

	int sort = scene->config.sorting_mode;
	const char* sort_modes[] = {"CPU (qsort)", "CPU (Radix)",
	                            "GPU (Bitonic)"};
	if (ImGui::Combo("Sort Mode", &sort, sort_modes, 3)) {
		scene->config.sorting_mode = (SortingMode)sort;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Billboard draw order for correct transparency:\n- CPU "
		    "qsort: O(n log n) comparison sort\n- CPU Radix: O(n) "
		    "stable sort (recommended)\n- GPU Bitonic: Parallel GPU "
		    "sort");
	}

	int gi = scene->config.gi_mode;
	const char* gi_modes[] = {"OFF", "Volume 3D Tex", "SSBO"};
	if (ImGui::Combo("GI Mode", &gi, gi_modes, 3)) {
		scene->config.gi_mode = (GIMode)gi;
	}

	ImGui::Checkbox("Show Probe Grid", &scene->config.show_probe_grid);
}

static void draw_tab_rendering(Scene* scene)
{
	if (!scene)
		return;

	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
	                   "Edge Anti-Aliasing");
	ImGui::Separator();
	ImGui::TextDisabled(
	    "Standard AA: MSAA is configured at launch (%d samples)",
	    DEFAULT_SAMPLES);
	ImGui::Spacing();

	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
	                   "Specular Anti-Aliasing");
	ImGui::Separator();
	ImGui::Checkbox("Specular AA Enabled",
	                &scene->config.specular_aa_enabled);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
		    "Screen-space roughness clamping to mitigate specular "
		    "aliasing\nusing microfacet distribution filtering");
	}

	if (scene->config.specular_aa_enabled) {
		int mode = scene->config.aa_mode;
		const char* aa_modes[] = {"Screen-Space", "Curvature"};
		if (ImGui::Combo("Specular AA Mode", &mode, aa_modes, 2)) {
			scene->config.aa_mode = (AAMode)mode;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
			    "Screen-Space: GPU derivatives based on normal "
			    "maps/geometry\nCurvature: Analytic "
			    "pixel-to-sphere radius ratio");
		}
	}
}

static void draw_postfx_save_load(PostProcess* p)
{
	ImGui::TextDisabled("Presets Management");
}

static void draw_postfx_tab(PostProcess* p)
{
	if (!p) {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
		                   "Postprocess pipeline not initialized");
		return;
	}

	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Post-Processing");
	ImGui::Separator();

	// Master Toggle
	bool master_on = p->shaders.is_optimized || p->active_effects != 0;
	// We can read/modify the enabled flag
	ImGui::Checkbox("Enable Post-FX", &master_on);
	if (!master_on) {
		if (p->active_effects != 0) {
			p->active_effects = 0;
			p->ubo_dirty = true;
		}
		ImGui::Spacing();
		return;
	}

	ImGui::Spacing();

	// Preset selector
	static int selected_preset = 0;
	const PostProcessPreset* presets[] = {
	    &PRESET_DEFAULT,     &PRESET_SUBTLE,    &PRESET_CINEMATIC,
	    &PRESET_VINTAGE,     &PRESET_MATRIX,    &PRESET_BW_CONTRAST,
	    &PRESET_POSTERIZED,  &PRESET_RETRO,     &PRESET_ANALOG,
	    &PRESET_CHANNEL_GFX, &PRESET_BLUEPRINT, &PRESET_NORDIC_NOIR,
	    &PRESET_SONY_A7SIII};
	const char* preset_names[] = {
	    "Default",     "Subtle",       "Cinematic",  "Vintage",
	    "Matrix",      "B&W Contrast", "Posterized", "Retro",
	    "Analog",      "Channel GFX",  "Blueprint",  "Nordic Noir",
	    "Sony A7S III"};
	if (ImGui::Combo("Preset", &selected_preset, preset_names, 13)) {
		postprocess_apply_preset(p, presets[selected_preset]);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 1. Exposure
	bool exp_on = postprocess_is_enabled(p, POSTFX_EXPOSURE);
	if (ImGui::Checkbox("Exposure", &exp_on)) {
		postprocess_toggle(p, POSTFX_EXPOSURE);
	}
	if (exp_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##exposure", 0)) {
			float val = p->exposure.exposure;
			if (ImGui::SliderFloat("Exposure##value", &val, 0.1f,
			                       10.0f)) {
				postprocess_set_exposure(p, val);
			}
			ImGui::TreePop();
		}
	}

	// 2. Tonemapping
	bool tm_on = postprocess_is_enabled(
	    p, POSTFX_EXPOSURE);  // tonemapper uses same/standard
	// Wait, is there a specific POSTFX_TONEMAP? No, tonemapper is always
	// run in uber-shader or conditional. In pp_params.h, there is no direct
	// POSTFX_TONEMAP; it is applied on PBR. But we can configure its
	// parameters:
	if (ImGui::TreeNodeEx("Tonemapping Curve", 0)) {
		bool updated = false;
		float slope = p->tonemapper.slope;
		float toe = p->tonemapper.toe;
		float shoulder = p->tonemapper.shoulder;
		float black = p->tonemapper.black_clip;
		float white = p->tonemapper.white_clip;
		if (ImGui::SliderFloat("Slope", &slope, 0.1f, 3.0f))
			updated = true;
		if (ImGui::SliderFloat("Toe", &toe, 0.0f, 1.0f))
			updated = true;
		if (ImGui::SliderFloat("Shoulder", &shoulder, 0.0f, 2.0f))
			updated = true;
		if (ImGui::SliderFloat("Black Clip", &black, 0.0f, 0.5f))
			updated = true;
		if (ImGui::SliderFloat("White Clip", &white, 0.0f, 0.5f))
			updated = true;
		if (updated) {
			postprocess_set_tonemapper(p, slope, toe, shoulder,
			                           black, white);
		}
		ImGui::TreePop();
	}

	// 3. Vignette
	bool vig_on = postprocess_is_enabled(p, POSTFX_VIGNETTE);
	if (ImGui::Checkbox("Vignette", &vig_on)) {
		postprocess_toggle(p, POSTFX_VIGNETTE);
	}
	if (vig_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##vignette", 0)) {
			float val_i = p->vignette.intensity;
			float val_s = p->vignette.smoothness;
			float val_r = p->vignette.roundness;
			if (ImGui::SliderFloat("Intensity##vig", &val_i, 0.0f,
			                       2.0f) ||
			    ImGui::SliderFloat("Smoothness##vig", &val_s, 0.01f,
			                       2.0f) ||
			    ImGui::SliderFloat("Roundness##vig", &val_r, 0.0f,
			                       1.0f)) {
				postprocess_set_vignette(p, val_i, val_s,
				                         val_r);
			}
			ImGui::TreePop();
		}
	}

	// 4. Film Grain
	bool grain_on = postprocess_is_enabled(p, POSTFX_GRAIN);
	if (ImGui::Checkbox("Film Grain", &grain_on)) {
		postprocess_toggle(p, POSTFX_GRAIN);
	}
	if (grain_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##grain", 0)) {
			float val_i = p->grain.intensity;
			if (ImGui::SliderFloat("Intensity##grain", &val_i, 0.0f,
			                       0.2f)) {
				postprocess_set_grain(p, val_i);
			}
			ImGui::SliderFloat("Texel Size##grain",
			                   &p->grain.texel_size, 0.5f, 4.0f);
			ImGui::TreePop();
		}
	}

	// 5. Chromatic Aberration
	bool ca_on = postprocess_is_enabled(p, POSTFX_CHROM_ABBR);
	if (ImGui::Checkbox("Chromatic Aberration", &ca_on)) {
		postprocess_toggle(p, POSTFX_CHROM_ABBR);
	}
	if (ca_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##ca", 0)) {
			float val_s = p->chrom_abbr.strength;
			if (ImGui::SliderFloat("Strength##ca", &val_s, 0.0f,
			                       0.05f)) {
				postprocess_set_chrom_abbr(p, val_s);
			}
			ImGui::TreePop();
		}
	}

	// 6. Color Grading
	bool cg_on = postprocess_is_enabled(p, POSTFX_COLOR_GRADING);
	if (ImGui::Checkbox("Color Grading", &cg_on)) {
		postprocess_toggle(p, POSTFX_COLOR_GRADING);
	}
	if (cg_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##cg", 0)) {
			float sat = p->color_grading.saturation;
			float cont = p->color_grading.contrast;
			float gamma = p->color_grading.gamma;
			float gain = p->color_grading.gain;
			float offset = p->color_grading.offset;
			float lift = p->color_grading.lift;
			if (ImGui::SliderFloat("Saturation", &sat, 0.0f,
			                       2.0f) ||
			    ImGui::SliderFloat("Contrast", &cont, 0.0f, 2.0f) ||
			    ImGui::SliderFloat("Gamma##cg", &gamma, 0.1f,
			                       3.0f) ||
			    ImGui::SliderFloat("Gain", &gain, 0.0f, 2.0f) ||
			    ImGui::SliderFloat("Offset", &offset, -0.5f,
			                       0.5f) ||
			    ImGui::SliderFloat("Lift", &lift, 0.0f, 1.0f)) {
				postprocess_set_color_grading(
				    p, sat, cont, gamma, gain, offset, lift);
			}
			ImGui::TreePop();
		}
	}

	// 7. Bloom
	bool bloom_on = postprocess_is_enabled(p, POSTFX_BLOOM);
	if (ImGui::Checkbox("Bloom", &bloom_on)) {
		postprocess_toggle(p, POSTFX_BLOOM);
	}
	if (bloom_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##bloom", 0)) {
			float val_i = p->bloom.intensity;
			float val_t = p->bloom.threshold;
			float val_s = p->bloom.soft_threshold;
			if (ImGui::SliderFloat("Intensity##bloom", &val_i, 0.0f,
			                       2.0f) ||
			    ImGui::SliderFloat("Threshold##bloom", &val_t, 0.0f,
			                       5.0f) ||
			    ImGui::SliderFloat("Soft Knee##bloom", &val_s, 0.0f,
			                       1.0f)) {
				postprocess_set_bloom(p, val_i, val_t, val_s);
			}
			ImGui::SliderFloat("Radius##bloom", &p->bloom.radius,
			                   0.1f, 4.0f);

			bool bloom_dbg =
			    postprocess_is_enabled(p, POSTFX_BLOOM_DEBUG);
			if (ImGui::Checkbox("Debug##bloom", &bloom_dbg)) {
				postprocess_toggle(p, POSTFX_BLOOM_DEBUG);
			}
			ImGui::TreePop();
		}
	}

	// 8. FXAA
	bool fxaa_on = postprocess_is_enabled(p, POSTFX_FXAA);
	if (ImGui::Checkbox("FXAA", &fxaa_on)) {
		postprocess_toggle(p, POSTFX_FXAA);
	}
	if (fxaa_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##fxaa", 0)) {
			float sub = p->fxaa.subpix;
			float edge = p->fxaa.edge_threshold;
			float min_t = p->fxaa.edge_threshold_min;
			if (ImGui::SliderFloat("Subpixel Quality", &sub, 0.0f,
			                       1.0f) ||
			    ImGui::SliderFloat("Edge Threshold", &edge, 0.01f,
			                       0.5f) ||
			    ImGui::SliderFloat("Edge Threshold Min", &min_t,
			                       0.01f, 0.2f)) {
				postprocess_set_fxaa(p, sub, edge, min_t);
			}
			bool fxaa_dbg =
			    postprocess_is_enabled(p, POSTFX_FXAA_DEBUG);
			if (ImGui::Checkbox("Debug##fxaa", &fxaa_dbg)) {
				postprocess_toggle(p, POSTFX_FXAA_DEBUG);
			}
			ImGui::TreePop();
		}
	}

	// 9. Auto-Exposure
	bool ae_on = postprocess_is_enabled(p, POSTFX_AUTO_EXPOSURE);
	if (ImGui::Checkbox("Auto-Exposure", &ae_on)) {
		postprocess_toggle(p, POSTFX_AUTO_EXPOSURE);
	}
	if (ae_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##ae", 0)) {
			float val_min = p->auto_exposure.min_luminance;
			float val_max = p->auto_exposure.max_luminance;
			float speed_u = p->auto_exposure.speed_up;
			float speed_d = p->auto_exposure.speed_down;
			float key_val = p->auto_exposure.key_value;
			if (ImGui::SliderFloat("Min Luminance", &val_min,
			                       0.001f, 1.0f) ||
			    ImGui::SliderFloat("Max Luminance", &val_max,
			                       100.0f, 50000.0f) ||
			    ImGui::SliderFloat("Speed Up", &speed_u, 0.1f,
			                       10.0f) ||
			    ImGui::SliderFloat("Speed Down", &speed_d, 0.1f,
			                       10.0f) ||
			    ImGui::SliderFloat("Key Value", &key_val, 0.01f,
			                       1.0f)) {
				postprocess_set_auto_exposure(p, val_min,
				                              val_max, speed_u,
				                              speed_d, key_val);
			}
			ImGui::Spacing();
			float current_exposure =
			    fx_auto_exposure_get_current_exposure(
			        &p->auto_exposure_fx);
			ImGui::Text("Current Exposure: %.3f", current_exposure);
			ImGui::TreePop();
		}
	}

	// 10. Depth of Field
	bool dof_on = postprocess_is_enabled(p, POSTFX_DOF);
	if (ImGui::Checkbox("Depth of Field", &dof_on)) {
		postprocess_toggle(p, POSTFX_DOF);
	}
	if (dof_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##dof", 0)) {
			float focal_d = p->dof.focal_distance;
			float focal_r = p->dof.focal_range;
			float bokeh = p->dof.bokeh_scale;
			if (ImGui::SliderFloat("Focal Distance##dof", &focal_d,
			                       1.0f, 100.0f) ||
			    ImGui::SliderFloat("Focal Range##dof", &focal_r,
			                       0.5f, 50.0f) ||
			    ImGui::SliderFloat("Bokeh Scale##dof", &bokeh, 1.0f,
			                       50.0f)) {
				postprocess_set_dof(p, focal_d, focal_r, bokeh);
			}
			float anam = p->dof.anamorphic_ratio;
			if (ImGui::SliderFloat("Anamorphic##dof", &anam, 0.5f,
			                       2.0f)) {
				postprocess_set_dof_anamorphic(p, anam);
			}
			bool dof_dbg =
			    postprocess_is_enabled(p, POSTFX_DOF_DEBUG);
			if (ImGui::Checkbox("Debug##dof", &dof_dbg)) {
				postprocess_toggle(p, POSTFX_DOF_DEBUG);
			}
			ImGui::TreePop();
		}
	}

	// 11. Motion Blur
	bool mb_on = postprocess_is_enabled(p, POSTFX_MOTION_BLUR);
	if (ImGui::Checkbox("Motion Blur", &mb_on)) {
		postprocess_toggle(p, POSTFX_MOTION_BLUR);
	}
	if (mb_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##mblur", 0)) {
			ImGui::SliderFloat("Intensity##mblur",
			                   &p->motion_blur.intensity, 0.0f,
			                   2.0f);
			ImGui::SliderFloat("Max Velocity##mblur",
			                   &p->motion_blur.max_velocity, 0.005f,
			                   0.2f);
			ImGui::SliderInt("Samples##mblur",
			                 &p->motion_blur.samples, 2, 32);
			bool mb_dbg =
			    postprocess_is_enabled(p, POSTFX_MOTION_BLUR_DEBUG);
			if (ImGui::Checkbox("Debug View##mblur", &mb_dbg)) {
				postprocess_toggle(p, POSTFX_MOTION_BLUR_DEBUG);
			}
			ImGui::TreePop();
		}
	}

	// 12. Banding
	bool banding_on = postprocess_is_enabled(p, POSTFX_BANDING);
	if (ImGui::Checkbox("Banding", &banding_on)) {
		postprocess_toggle(p, POSTFX_BANDING);
	}
	if (banding_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##banding", 0)) {
			int mode = p->banding.mode;
			const char* mode_names[] = {"Linear", "Dithered",
			                            "Perceptual", "Channel",
			                            "Luminance"};
			if (ImGui::Combo("Mode##banding", &mode, mode_names,
			                 5)) {
				postprocess_set_banding(p, (BandingMode)mode,
				                        p->banding.levels);
			}
			ImGui::SliderFloat("Levels##banding",
			                   &p->banding.levels, 2.0f, 256.0f);
			if (mode == BANDING_MODE_DITHERED) {
				ImGui::SliderFloat("Dither Strength##banding",
				                   &p->banding.dither_strength,
				                   0.0f, 3.0f);
			}
			if (mode == BANDING_MODE_PERCEPTUAL) {
				ImGui::SliderFloat("Gamma##banding",
				                   &p->banding.perceptual_gamma,
				                   0.5f, 4.0f);
			}
			if (mode == BANDING_MODE_CHANNEL ||
			    mode == BANDING_MODE_LUMINANCE) {
				ImGui::SliderFloat(
				    "R Levels##banding",
				    &p->banding.channel_levels[0], 2.0f,
				    256.0f);
				ImGui::SliderFloat(
				    "G Levels##banding",
				    &p->banding.channel_levels[1], 2.0f,
				    256.0f);
				ImGui::SliderFloat(
				    "B Levels##banding",
				    &p->banding.channel_levels[2], 2.0f,
				    256.0f);
			}
			ImGui::TreePop();
		}
	}

	// 13. Fog
	bool fog_on = postprocess_is_enabled(p, POSTFX_FOG);
	if (ImGui::Checkbox("Fog", &fog_on)) {
		postprocess_toggle(p, POSTFX_FOG);
	}
	if (fog_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##fog", 0)) {
			float dens = p->fog.density;
			float start = p->fog.start;
			float height = p->fog.height_falloff;
			float col[3] = {p->fog.color[0], p->fog.color[1],
			                p->fog.color[2]};
			if (ImGui::SliderFloat("Density##fog", &dens, 0.001f,
			                       1.0f) ||
			    ImGui::SliderFloat("Start##fog", &start, 0.0f,
			                       100.0f) ||
			    ImGui::SliderFloat("Height Falloff##fog", &height,
			                       0.0f, 0.5f) ||
			    ImGui::ColorEdit3("Color##fog", col)) {
				postprocess_set_fog(p, dens, start, height,
				                    col[0], col[1], col[2]);
			}
			bool fog_dbg =
			    postprocess_is_enabled(p, POSTFX_FOG_DEBUG);
			if (ImGui::Checkbox("Debug (greyscale mask)##fog",
			                    &fog_dbg)) {
				postprocess_toggle(p, POSTFX_FOG_DEBUG);
			}
			ImGui::TreePop();
		}
	}

	// 14. LUT3D
	bool lut_on = postprocess_is_enabled(p, POSTFX_LUT3D);
	if (ImGui::Checkbox("LUT3D", &lut_on)) {
		postprocess_toggle(p, POSTFX_LUT3D);
	}
	if (lut_on) {
		ImGui::SameLine();
		if (ImGui::TreeNodeEx("Settings##lut3d", 0)) {
			ImGui::SliderFloat("Intensity##lut3d",
			                   &p->lut3d.intensity, 0.0f, 1.0f);
			ImGui::Text(
			    "LUT File Path: assets/textures/lut3d.cube");
			ImGui::TreePop();
		}
	}
}

static void draw_gpu_timings_section(App* app)
{
	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
	                   "GPU Profiling Stages");
	ImGui::Separator();

	GPUProfiler* profiler = &app->profiling->gpu_profiler;
	if (!profiler->enabled) {
		ImGui::TextDisabled(
		    "Profiling is currently disabled in the engine.");
		return;
	}

	if (ImGui::BeginTable("##gpu_timings_c", 3,
	                      ImGuiTableFlags_BordersInnerH |
	                          ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn(
		    "Stage / Pass", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableSetupColumn(
		    "Duration (ms)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn(
		    "% of Frame", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableHeadersRow();

		float total_frame_ms = (float)app->delta_time * 1000.0f;
		for (int i = 0; i < profiler->stage_count; ++i) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%s", profiler->stages[i].name);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", profiler->stages[i].duration_ms);
			ImGui::TableNextColumn();
			float pct = 0.0f;
			if (total_frame_ms > 0.0f) {
				pct = (profiler->stages[i].duration_ms /
				       total_frame_ms) *
				      100.0f;
			}
			ImGui::Text("%.1f%%", pct);
		}
		ImGui::EndTable();
	}
}

static void draw_shader_cache_section(App* app)
{
	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
	                   "Shader Optimization & Cache");
	ImGui::Separator();

	PostProcess* p = app->postprocess;
	ImGui::Checkbox("Enable Variants", (bool*)&p->shaders.is_optimized);
	ImGui::Text("Active Variant Flags: %X", p->active_effects);
	ImGui::Text("Compiled Variants Count: %d",
	            p->shaders.is_optimized ? 1 : 0);
}

static void draw_tab_compute_tuning(GuiState* g, App* app)
{
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
	                   "Compute & Slicing Tuning");
	ImGui::TextWrapped(
	    "Staged parameter tweaking. Edits are held in local draft memory "
	    "until explicitly validated and applied.");
	ImGui::Separator();

	ImGui::SliderInt("SPBRDF Sample Count", &g->spbrdf_sample_count, 32,
	                 2048);
	ImGui::SliderInt("SPMap Sample Count", &g->spmap_sample_count, 32,
	                 2048);
	ImGui::SliderFloat("IRMap Delta", &g->irmap_sample_delta, 0.005f,
	                   0.200f, "%.3f");

	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
	                   "Sliced Amortization (Frames)");
	ImGui::DragInt("Mip 0 Slices", &g->mip0_slices, 1.0f, 1, 100);
	ImGui::DragInt("Mip 1 Slices", &g->mip1_slices, 1.0f, 1, 100);
	ImGui::DragInt("Mip 2 Slices", &g->mip2_slices, 1.0f, 1, 100);
	ImGui::DragInt("Irradiance Slices", &g->irdiff_slices, 1.0f, 1, 100);

	ImGui::Spacing();
	if (ImGui::Button("Apply & Recalculate Active Environment",
	                  ImVec2(350, 32))) {
		// Mock apply callback or notification
		g->compute_tuning_status_msg =
		    "Successfully applied and triggered recalculation!";
		g->compute_tuning_status_timer = 3.0f;
	}

	if (g->compute_tuning_status_msg &&
	    g->compute_tuning_status_timer > 0.0f) {
		ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s",
		                   g->compute_tuning_status_msg);
		g->compute_tuning_status_timer -= ImGui::GetIO().DeltaTime;
	}
}

static void draw_tab_ibl_debug(GuiState* g, App* app)
{
	ImGui::SliderFloat("Preview Size", &g->ibl_preview_size, 64.0f, 512.0f);
	ImGui::SliderFloat("Preview Exposure (EV)", &g->ibl_debug_exposure,
	                   -6.0f, 6.0f);
	if (ImGui::IsItemDeactivatedAfterEdit() ||
	    ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
		g->ibl_debug_exposure = 0.0f;
	}
	ImGui::Separator();

	float preview_w = g->ibl_preview_size;
	float preview_h = preview_w * 0.5f;

	Scene* scene = app->scene;
	SceneGPUResources* gpu = scene->gpu;

	// 1. Environment Map
	if (gpu->hdr_texture != 0) {
		if (g->ibl_scroll_target == 1) {
			ImGui::SetScrollHereY(0.0f);
			g->ibl_scroll_target = 0;
		}
		if (ImGui::CollapsingHeader("Environment Map (Source HDR)",
		                            ImGuiTreeNodeFlags_DefaultOpen)) {
			int w = scene->lighting.ibl_coord.width;
			int h = scene->lighting.ibl_coord.height;
			ImGui::Text("ID: %d  Size: %dx%d  Format: RGBA16F",
			            gpu->hdr_texture, w, h);
			draw_image_with_inspector(g, gpu->hdr_texture,
			                          ImVec2(preview_w, preview_h),
			                          w, h);
			ImGui::Spacing();
		}
	}

	// 2. Irradiance Map
	if (gpu->irradiance_tex != 0) {
		if (g->ibl_scroll_target == 2) {
			ImGui::SetScrollHereY(0.0f);
			g->ibl_scroll_target = 0;
		}
		if (ImGui::CollapsingHeader("Irradiance Map (Diffuse IBL)",
		                            ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("ID: %d  Size: %dx%d  Format: RGBA16F",
			            gpu->irradiance_tex, 64, 64);
			draw_image_with_inspector(g, gpu->irradiance_tex,
			                          ImVec2(preview_w, preview_w),
			                          64, 64);
			ImGui::Spacing();
		}
	}

	// 3. Prefilter Map
	if (gpu->spec_prefiltered_tex != 0) {
		if (g->ibl_scroll_target == 3) {
			ImGui::SetScrollHereY(0.0f);
			g->ibl_scroll_target = 0;
		}
		if (ImGui::CollapsingHeader("Prefilter Map (Specular IBL)",
		                            ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text(
			    "ID: %d  Size: 1024x1024  Mips: 5  Format: RGBA16F",
			    gpu->spec_prefiltered_tex);
			ImGui::SliderInt("Mip Level (Roughness)",
			                 &g->ibl_mip_level, 0, 4);
			float roughness = (float)g->ibl_mip_level / 4.0f;
			ImGui::Text("Roughness: %.2f", roughness);

			// Clamp LOD to force selected mip level display
			float mip_f = (float)g->ibl_mip_level;
			glBindTexture(GL_TEXTURE_2D, gpu->spec_prefiltered_tex);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD,
			                mip_f);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD,
			                mip_f);
			glBindTexture(GL_TEXTURE_2D, 0);
			g->ibl_prefilter_id = gpu->spec_prefiltered_tex;

			draw_image_with_inspector(g, gpu->spec_prefiltered_tex,
			                          ImVec2(preview_w, preview_w),
			                          1024, 1024, g->ibl_mip_level);
			ImGui::Spacing();
		}
	}

	// 4. BRDF LUT
	if (gpu->brdf_lut_tex != 0) {
		if (g->ibl_scroll_target == 4) {
			ImGui::SetScrollHereY(0.0f);
			g->ibl_scroll_target = 0;
		}
		if (ImGui::CollapsingHeader("BRDF LUT (Split-Sum)",
		                            ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("ID: %d  Size: 512x512  Format: RG16F",
			            gpu->brdf_lut_tex);
			ImGui::Text("X-axis: NdotV | Y-axis: Roughness");
			draw_image_with_inspector(g, gpu->brdf_lut_tex,
			                          ImVec2(preview_w, preview_w),
			                          512, 512);
			ImGui::Spacing();
		}
	}
}

static void draw_filtered_view(GuiState* g, App* app, const char* filter)
{
	ImGui::Text("Filtered view for: %s", filter);
}

extern "C" void gui_update(Gui_C* g, App* app)
{
	GuiState* state = static_cast<GuiState*>(g->internal_gui_ptr);
	if (!state || !state->ctx || !g->visible)
		return;

	ImGui::SetNextWindowSize(ImVec2(400.0f, 560.0f),
	                         ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Engine Controls", &g->visible)) {
		if (state->focus_search) {
			ImGui::SetKeyboardFocusHere();
			state->focus_search = false;
		}
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##search", "Search parameters...",
		                         state->search_buf, SEARCH_BUF_SIZE);

		const char* filter = state->search_buf;
		bool has_filter = std::strlen(filter) > 0;

		ImGui::Separator();

		if (has_filter) {
			draw_filtered_view(state, app, filter);
		} else {
			if (ImGui::BeginTabBar("##tabs")) {
				bool restoring = state->restore_tab > 0;
				auto tab_flags =
				    [&](int idx) -> ImGuiTabItemFlags {
					if (restoring &&
					    state->active_tab == idx) {
						return ImGuiTabItemFlags_SetSelected;
					}
					return 0;
				};

				if (ImGui::BeginTabItem("Camera", nullptr,
				                        tab_flags(0))) {
					if (!restoring)
						state->active_tab = 0;
					draw_tab_camera(&app->input->camera);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Scene", nullptr,
				                        tab_flags(1))) {
					if (!restoring)
						state->active_tab = 1;
					draw_tab_scene(app->scene,
					               app->postprocess);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Rendering", nullptr,
				                        tab_flags(2))) {
					if (!restoring)
						state->active_tab = 2;
					draw_tab_rendering(app->scene);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Post-FX", nullptr,
				                        tab_flags(3))) {
					if (!restoring)
						state->active_tab = 3;
					draw_postfx_tab(app->postprocess);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Profiling", nullptr,
				                        tab_flags(4))) {
					if (!restoring)
						state->active_tab = 4;
					draw_gpu_timings_section(app);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Shaders", nullptr,
				                        tab_flags(5))) {
					if (!restoring)
						state->active_tab = 5;
					draw_shader_cache_section(app);
					ImGui::EndTabItem();
				}

				ImGuiTabItemFlags ibl_tab_f = tab_flags(6);
				if (state->ibl_debug_open) {
					ibl_tab_f |=
					    ImGuiTabItemFlags_SetSelected;
				}
				if (ImGui::BeginTabItem("IBL Debug", nullptr,
				                        ibl_tab_f)) {
					if (!restoring)
						state->active_tab = 6;
					state->ibl_debug_open = false;
					draw_tab_ibl_debug(state, app);
					ImGui::EndTabItem();
				} else {
					state->ibl_debug_open = false;
				}

				if (ImGui::BeginTabItem("Compute Tuning",
				                        nullptr,
				                        tab_flags(7))) {
					if (!restoring)
						state->active_tab = 7;
					draw_tab_compute_tuning(state, app);
					ImGui::EndTabItem();
				}

				if (state->restore_tab > 0) {
					state->restore_tab--;
				}
				ImGui::EndTabBar();
			}
		}
	}
	ImGui::End();
}

extern "C" void gui_render(Gui_C* g)
{
	GuiState* state = static_cast<GuiState*>(g->internal_gui_ptr);
	if (!state || !state->ctx)
		return;

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Restore prefilter LOD constraints after drawing
	if (state->ibl_prefilter_id != 0) {
		glBindTexture(GL_TEXTURE_2D, state->ibl_prefilter_id);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, -1000.0f);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 1000.0f);
		glBindTexture(GL_TEXTURE_2D, 0);
		state->ibl_prefilter_id = 0;
	}
}

extern "C" void gui_toggle(Gui_C* g)
{
	g->visible = !g->visible;
}

extern "C" void gui_destroy(Gui_C* g)
{
	LOG_INFO("suckless-ogl.gui", "Shutting down Dear ImGui...");
	GuiState* state = static_cast<GuiState*>(g->internal_gui_ptr);
	if (!state)
		return;

	if (state->inspector_fbo != 0) {
		glDeleteFramebuffers(1, &state->inspector_fbo);
		state->inspector_fbo = 0;
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext(state->ctx);

	delete state;
	g->internal_gui_ptr = nullptr;
	LOG_INFO("suckless-ogl.gui", "Dear ImGui shut down successfully");
}

extern "C" bool gui_wants_keyboard(Gui_C* g)
{
	GuiState* state = static_cast<GuiState*>(g->internal_gui_ptr);
	if (!state || !state->ctx || !g->visible)
		return false;

	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard;
}

extern "C" bool gui_wants_mouse(Gui_C* g)
{
	GuiState* state = static_cast<GuiState*>(g->internal_gui_ptr);
	if (!state || !state->ctx || !g->visible)
		return false;

	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureMouse;
}

// --- Internal Helper Implementations ---

static std::vector<float> read_texture_pixel(GuiState* g, GLuint tex_id, int x,
                                             int y, int mip_level)
{
	std::vector<float> pixel(4, 0.0f);
	if (g->inspector_fbo == 0) {
		glGenFramebuffers(1, &g->inspector_fbo);
	}
	GLint prev_fbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

	glBindFramebuffer(GL_FRAMEBUFFER, g->inspector_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, tex_id, mip_level);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE) {
		glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());
	}
	glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
	return pixel;
}

static void draw_image_with_inspector(GuiState* g, GLuint tex_id,
                                      ImVec2 display_size, int tex_w, int tex_h,
                                      int mip_level)
{
	float tint = std::pow(2.0f, g->ibl_debug_exposure);
	ImGui::ImageWithBg((ImTextureID)(uintptr_t)tex_id, display_size,
	                   ImVec2(0, 1), ImVec2(1, 0), ImVec4(0, 0, 0, 0),
	                   ImVec4(tint, tint, tint, 1.0f));

	ImVec2 item_min = ImGui::GetItemRectMin();
	ImVec2 item_max = ImGui::GetItemRectMax();

	// Click to inspect
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		ImVec2 mouse_pos = ImGui::GetMousePos();
		float rel_x = (mouse_pos.x - item_min.x) / display_size.x;
		float rel_y = (mouse_pos.y - item_min.y) / display_size.y;

		g->inspect_active = true;
		g->inspect_uv[0] = rel_x;
		g->inspect_uv[1] = 1.0f - rel_y;  // Flip Y for GL
		g->inspect_tex_id = tex_id;
		g->inspect_tex_w = tex_w;
		g->inspect_tex_h = tex_h;
		g->inspect_mip = mip_level;

		int mip_w = std::max(1, tex_w >> mip_level);
		int mip_h = std::max(1, tex_h >> mip_level);
		int tx = std::max(
		    0, std::min(mip_w - 1, (int)(g->inspect_uv[0] * mip_w)));
		int ty = std::max(
		    0, std::min(mip_h - 1, (int)(g->inspect_uv[1] * mip_h)));
		auto p = read_texture_pixel(g, tex_id, tx, ty, mip_level);
		std::copy(p.begin(), p.end(), g->inspect_pixel);
	}

	bool is_inspected = g->inspect_active && g->inspect_tex_id == tex_id &&
	                    g->inspect_mip == mip_level;
	if (!is_inspected)
		return;

	// Zoom rectangular overlay
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	int mip_w = std::max(1, tex_w >> mip_level);
	int mip_h = std::max(1, tex_h >> mip_level);

	float half_region_x = 16.0f * 0.5f / (float)mip_w;
	float half_region_y = 16.0f * 0.5f / (float)mip_h;

	float center_screen_x = item_min.x + g->inspect_uv[0] * display_size.x;
	float center_screen_y =
	    item_min.y + (1.0f - g->inspect_uv[1]) * display_size.y;

	float rect_half_w = half_region_x * display_size.x;
	float rect_half_h = half_region_y * display_size.y;

	ImVec2 rect_min(std::max(center_screen_x - rect_half_w, item_min.x),
	                std::max(center_screen_y - rect_half_h, item_min.y));
	ImVec2 rect_max(std::min(center_screen_x + rect_half_w, item_max.x),
	                std::min(center_screen_y + rect_half_h, item_max.y));

	// Draw yellow box (ABGR or RGBA)
	draw_list->AddRect(rect_min, rect_max, 0xFF00FFFF, 0.0f, 0, 2.0f);

	// Crosshair
	draw_list->AddLine(ImVec2(center_screen_x - 5.0f, center_screen_y),
	                   ImVec2(center_screen_x + 5.0f, center_screen_y),
	                   0xFF00FFFF, 1.0f);
	draw_list->AddLine(ImVec2(center_screen_x, center_screen_y - 5.0f),
	                   ImVec2(center_screen_x, center_screen_y + 5.0f),
	                   0xFF00FFFF, 1.0f);

	// Zoomed preview inline
	ImGui::SameLine();
	ImGui::BeginGroup();

	float half_region = 8.0f;
	float zoom_uv_half_x = half_region / (float)mip_w;
	float zoom_uv_half_y = half_region / (float)mip_h;

	ImVec2 zoom_uv0(
	    std::max(0.0f, std::min(1.0f, g->inspect_uv[0] - zoom_uv_half_x)),
	    std::max(0.0f, std::min(1.0f, g->inspect_uv[1] + zoom_uv_half_y)));
	ImVec2 zoom_uv1(
	    std::max(0.0f, std::min(1.0f, g->inspect_uv[0] + zoom_uv_half_x)),
	    std::max(0.0f, std::min(1.0f, g->inspect_uv[1] - zoom_uv_half_y)));

	ImGui::Image((ImTextureID)(uintptr_t)tex_id, ImVec2(128.0f, 128.0f),
	             zoom_uv0, zoom_uv1);

	int texel_x =
	    std::max(0, std::min(mip_w - 1, (int)(g->inspect_uv[0] * mip_w)));
	int texel_y =
	    std::max(0, std::min(mip_h - 1, (int)(g->inspect_uv[1] * mip_h)));

	ImGui::Text("(%d, %d)", texel_x, texel_y);
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "R %.4f",
	                   g->inspect_pixel[0]);
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "G %.4f",
	                   g->inspect_pixel[1]);
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 1.0f, 1.0f), "B %.4f",
	                   g->inspect_pixel[2]);
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "A %.4f",
	                   g->inspect_pixel[3]);

	float lum = g->inspect_pixel[0] * 0.2126f +
	            g->inspect_pixel[1] * 0.7152f +
	            g->inspect_pixel[2] * 0.0722f;
	ImGui::Text("L %.4f", lum);

	float max_c = std::max({g->inspect_pixel[0], g->inspect_pixel[1],
	                        g->inspect_pixel[2], 1.0f});
	ImVec4 swatch(g->inspect_pixel[0] / max_c, g->inspect_pixel[1] / max_c,
	              g->inspect_pixel[2] / max_c, 1.0f);
	ImGui::ColorButton("##swatch", swatch, ImGuiColorEditFlags_NoTooltip,
	                   ImVec2(24, 24));

	ImGui::EndGroup();
}
