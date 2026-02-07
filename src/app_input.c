#include "app_input.h"

#include "action_notifier.h"
#include "app.h"
#include "app_env.h"
#include "app_scene.h"
#include "app_settings.h"
#include "camera.h"
#include "glad/glad.h"
#include "log.h"
#include "perf_mode.h"
#include "postprocess.h" /* Explicit include for types */
#include "postprocess_presets.h"
#include "utils.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PBR_DEBUG_MODE_COUNT = 9 };

/* Notification constants */
enum { NOTIF_BUF_SIZE = 128 };

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	App* app = (App*)glfwGetWindowUserPointer(window);
	app->width = width;
	app->height = height;
	glViewport(0, 0, width, height);

	/* Redimensionner le post-processing */
	postprocess_resize(&app->postprocess, width, height);
}

void handle_preset_input(App* app, int key)
{
	switch (key) {
		case GLFW_KEY_1: /* Preset: Aucun */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(&app->postprocess,
			                         app->auto_threshold);
			LOG_INFO("suckless-ogl.app",
			         "Style: Aucun (rendu pur) - Exposure: %.2f",
			         app->auto_threshold);
			action_notifier_push(&app->notifier,
			                     "Style: Pure Render",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_2: /* Preset: Subtle */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_SUBTLE);
			LOG_INFO("suckless-ogl.app", "Style: Subtle");
			action_notifier_push(&app->notifier, "Style: Subtle",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_3: /* Preset: Cinématique */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_CINEMATIC);
			LOG_INFO("suckless-ogl.app", "Style: Cinématique");
			action_notifier_push(&app->notifier, "Style: Cinematic",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_4: /* Preset: Vintage */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_VINTAGE);
			LOG_INFO("suckless-ogl.app", "Style: Vintage");
			action_notifier_push(&app->notifier, "Style: Vintage",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_5: /* Style: "Matrix" */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_MATRIX);
			LOG_INFO("suckless-ogl.app", "Style: Matrix Grading");
			action_notifier_push(&app->notifier, "Style: Matrix",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_6: /* Style: "Noir et Blanc Contrasté" */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_BW_CONTRAST);
			LOG_INFO("suckless-ogl.app", "Style: Noir & Blanc");
			action_notifier_push(&app->notifier,
			                     "Style: B&W Contrast",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_7: { /* Style Cycle: All Banding Styles */
			const PostProcessPreset* banding_presets[] = {
			    &PRESET_POSTERIZED,  /* 1. Posterization */
			    &PRESET_RETRO,       /* 2. Dithered */
			    &PRESET_ANALOG,      /* 3. Perceptual */
			    &PRESET_CHANNEL_GFX, /* 4. Channel */
			    &PRESET_BLUEPRINT    /* 5. Blueprint */
			};
			const char* banding_names[] = {
			    "Posterization (Pop Art)",
			    "Retro Computing (Dithered)", "Analog (Perceptual)",
			    "CGA/VGA Style (Channel)", "Blueprint (Luminance)"};
			int num_styles = sizeof(banding_presets) /
			                 sizeof(banding_presets[0]);

			/* Check if banding is currently off, if so, start at
			   idx 0. Else cycle to next. */
			if (!postprocess_is_enabled(&app->postprocess,
			                            POSTFX_BANDING)) {
				app->banding_style_idx = 0;
			} else {
				app->banding_style_idx =
				    (app->banding_style_idx + 1) % num_styles;
			}

			postprocess_apply_preset(
			    &app->postprocess,
			    banding_presets[app->banding_style_idx]);
			LOG_INFO("suckless-ogl.app",
			         "Banding Style [%d/%d]: %s",
			         app->banding_style_idx + 1, num_styles,
			         banding_names[app->banding_style_idx]);

			char buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(
			    buf, sizeof(buf), "Banding: %s",
			    banding_names[app->banding_style_idx]);
			action_notifier_push(&app->notifier, buf,
			                     NOTIF_DUR_LONG);
			break;
		}
		case GLFW_KEY_8:
			if (!effect_benchmark_is_running(&app->effect_bench)) {
				effect_benchmark_start(&app->effect_bench);
				action_notifier_push(&app->notifier,
				                     "FX Benchmark: Started",
				                     NOTIF_DUR_LONG);
			} else {
				action_notifier_push(
				    &app->notifier,
				    "FX Benchmark: Already running",
				    NOTIF_DUR_NORMAL);
			}
			break;
		case GLFW_KEY_0:
		case GLFW_KEY_KP_0:
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(&app->postprocess,
			                         app->auto_threshold);
			LOG_INFO("suckless-ogl.app",
			         "Color Grading: Reset to Defaults");
			action_notifier_push(&app->notifier,
			                     "FX: Reset to Defaults",
			                     NOTIF_DUR_LONG);
			break;
		default:
			break;
	}
}

static void toggle_postfx(App* app, PostProcessEffect feature, const char* name)
{
	postprocess_toggle(&app->postprocess, feature);
	int enabled = postprocess_is_enabled(&app->postprocess, feature);
	LOG_INFO("suckless-ogl.app", "%s: %s", name, enabled ? "ON" : "OFF");

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "%s: %s", name,
	                    enabled ? "ON" : "OFF");
	action_notifier_push(&app->notifier, buf, NOTIF_DUR_NORMAL);
}

static void toggle_postfx_complex(App* app, PostProcessEffect feature,
                                  PostProcessEffect debug_feature,
                                  const char* name, const char* debug_name)
{
	if (glfwGetKey(app->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
	    glfwGetKey(app->window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
		toggle_postfx(app, debug_feature, debug_name);
	} else {
		toggle_postfx(app, feature, name);
	}
}

static void handle_exposure_input(App* app, int key)
{
	float current = app->postprocess.exposure.exposure;
	float next_val = current;

	if (key == GLFW_KEY_KP_ADD) {
		next_val += DEFAULT_EXPOSURE_STEP;
	} else if (key == GLFW_KEY_KP_SUBTRACT) {
		next_val = (current > DEFAULT_MIN_EXPOSURE)
		               ? current - DEFAULT_EXPOSURE_STEP
		               : DEFAULT_MIN_EXPOSURE;
	}

	postprocess_set_exposure(&app->postprocess, next_val);
	LOG_INFO("suckless-ogl.app", "Exposure: %.2f",
	         app->postprocess.exposure.exposure);

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "Exposure: %.2f",
	                    app->postprocess.exposure.exposure);
	action_notifier_push(&app->notifier, buf, NOTIF_DUR_SHORT);
}

static void handle_pbr_debug_mode(App* app)
{
	app->pbr_debug_mode = (app->pbr_debug_mode + 1) % PBR_DEBUG_MODE_COUNT;
	const char* modeNames[] = {
	    "Final PBR",         "Albedo",           "Normal",
	    "Metallic",          "Roughness",        "AO",
	    "Irradiance (Diff)", "Prefilter (Spec)", "BRDF LUT"};
	LOG_INFO("suckless-ogl.app", "PBR Debug Mode: %s",
	         modeNames[app->pbr_debug_mode]);

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "Debug: %s",
	                    modeNames[app->pbr_debug_mode]);
	action_notifier_push(&app->notifier, buf, NOTIF_DUR_LONG);
}

void handle_postprocess_input(App* app, int key)
{
	switch (key) {
		case GLFW_KEY_V:
			toggle_postfx(app, POSTFX_VIGNETTE, "Vignette");
			break;
		case GLFW_KEY_G:
			toggle_postfx(app, POSTFX_GRAIN, "Grain");
			break;
		case GLFW_KEY_B:
			toggle_postfx(app, POSTFX_BLOOM, "Bloom");
			break;
		case GLFW_KEY_H:
			toggle_postfx_complex(app, POSTFX_DOF, POSTFX_DOF_DEBUG,
			                      "DOF", "DOF DEBUG");
			break;
		case GLFW_KEY_M:
			if (glfwGetKey(app->window, GLFW_KEY_LEFT_SHIFT) ==
			        GLFW_PRESS ||
			    glfwGetKey(app->window, GLFW_KEY_RIGHT_SHIFT) ==
			        GLFW_PRESS) {
				/* SHIFT+M: Cycle Debug Views: Off → MB Debug
				   → Vector Field → Off */
				int mb_dbg = postprocess_is_enabled(
				    &app->postprocess,
				    POSTFX_MOTION_BLUR_DEBUG);
				int vf_dbg = postprocess_is_enabled(
				    &app->postprocess,
				    POSTFX_VECTOR_FIELD_DEBUG);

				const char* mode_name = NULL;
				if (!mb_dbg && !vf_dbg) {
					/* Off → Motion Blur Debug */
					postprocess_enable(
					    &app->postprocess,
					    POSTFX_MOTION_BLUR_DEBUG);
					mode_name = "Motion Blur Debug (RG)";
				} else if (mb_dbg) {
					/* MB Debug → Vector Field */
					postprocess_disable(
					    &app->postprocess,
					    POSTFX_MOTION_BLUR_DEBUG);
					postprocess_enable(
					    &app->postprocess,
					    POSTFX_VECTOR_FIELD_DEBUG);
					mode_name = "Vector Field Debug";
				} else {
					/* Vector Field → Off */
					postprocess_disable(
					    &app->postprocess,
					    POSTFX_VECTOR_FIELD_DEBUG);
					mode_name = "Debug: OFF";
				}
				LOG_INFO("suckless-ogl.app",
				         "Velocity Debug: %s", mode_name);
				action_notifier_push(&app->notifier, mode_name,
				                     NOTIF_DUR_NORMAL);
			} else {
				/* M: Toggle Motion Blur */
				toggle_postfx(app, POSTFX_MOTION_BLUR,
				              "Motion Blur");
			}
			break;
		case GLFW_KEY_R:
			LOG_INFO("suckless-ogl.app",
			         "Shader reloading not implemented yet");
			action_notifier_push(&app->notifier,
			                     "Hot-Reload: Not Implemented",
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_KP_ADD:
		case GLFW_KEY_KP_SUBTRACT:
			handle_exposure_input(app, key);
			break;
		case GLFW_KEY_J:
			toggle_postfx_complex(
			    app, POSTFX_AUTO_EXPOSURE, POSTFX_EXPOSURE_DEBUG,
			    "Auto Exposure", "Auto Exposure Debug");
			break;
		case GLFW_KEY_F5:
			handle_pbr_debug_mode(app);
			break;
		default:
			handle_preset_input(app, key);
			break;
	}
}

void app_handle_env_input(App* app, int action, int mods, int key)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT) {
		return;
	}
	if (key == GLFW_KEY_PAGE_UP) {
		if (check_flag(mods, GLFW_MOD_SHIFT)) {
			app->env_lod += LOD_STEP;
			if (app->env_lod > MAX_ENV_LOD) {
				app->env_lod = MAX_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->env_lod);
			char lod_buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(lod_buf, sizeof(lod_buf),
			                    "Env LOD: %.1F", app->env_lod);
			action_notifier_push(&app->notifier, lod_buf,
			                     NOTIF_DUR_SHORT);
		} else if (app->hdr_count > 1) {
			app->current_hdr_index =
			    (app->current_hdr_index + 1) % app->hdr_count;
			app_load_env_map(
			    app, app->hdr_files[app->current_hdr_index]);

			char buf[NOTIF_BUF_SIZE];
			const char* filename =
			    app->hdr_files[app->current_hdr_index];
			/* Try to strip path if possible for cleaner display */
			const char* last_slash = strrchr(filename, '/');
			if (last_slash) {
				filename = last_slash + 1;
			}
			(void)safe_snprintf(buf, sizeof(buf), "HDR: %s",
			                    filename);
			action_notifier_push(&app->notifier, buf,
			                     NOTIF_DUR_LONG);
		}
	} else if (key == GLFW_KEY_PAGE_DOWN) {
		if (check_flag(mods, GLFW_MOD_SHIFT)) {
			app->env_lod -= LOD_STEP;
			if (app->env_lod < MIN_ENV_LOD) {
				app->env_lod = MIN_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->env_lod);
			char lod_buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(lod_buf, sizeof(lod_buf),
			                    "Env LOD: %.1F", app->env_lod);
			action_notifier_push(&app->notifier, lod_buf,
			                     NOTIF_DUR_SHORT);
		} else if (app->hdr_count > 1) {
			app->current_hdr_index--;
			if (app->current_hdr_index < 0) {
				app->current_hdr_index = app->hdr_count - 1;
			}
			app_load_env_map(
			    app, app->hdr_files[app->current_hdr_index]);

			char buf[NOTIF_BUF_SIZE];
			const char* filename =
			    app->hdr_files[app->current_hdr_index];
			const char* last_slash = strrchr(filename, '/');
			if (last_slash) {
				filename = last_slash + 1;
			}
			(void)safe_snprintf(buf, sizeof(buf), "HDR: %s",
			                    filename);
			action_notifier_push(&app->notifier, buf,
			                     NOTIF_DUR_LONG);
		}
	}
}

static void handle_overlay_input(App* app)
{
	static const char* mode_names[] = {
	    "Off", "FPS + Position", "FPS + Position + Envmap",
	    "FPS + Position + Envmap + Exposure"};
	static const int mode_count =
	    sizeof(mode_names) / sizeof(mode_names[0]);
	app->text_overlay_mode = (app->text_overlay_mode + 1) % mode_count;
	LOG_INFO("suckless-ogl.app", "Text Overlay: %s",
	         mode_names[app->text_overlay_mode]);

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "Overlay: %s",
	                    mode_names[app->text_overlay_mode]);
	action_notifier_push(&app->notifier, buf, NOTIF_DUR_LONG);
}

static void handle_subdiv_input(App* app, int key)
{
	int changed = 0;
	if (key == GLFW_KEY_UP && app->subdivisions < MAX_SUBDIV) {
		app->subdivisions++;
		changed = 1;
	} else if (key == GLFW_KEY_DOWN && app->subdivisions > MIN_SUBDIV) {
		app->subdivisions--;
		changed = 1;
	}

	if (changed) {
		char buf[NOTIF_BUF_SIZE];
		(void)safe_snprintf(buf, sizeof(buf), "Subdiv: %d",
		                    app->subdivisions);
		action_notifier_push(&app->notifier, buf, NOTIF_DUR_SHORT);
	}
}

static void handle_camera_toggle(App* app)
{
	app->camera_enabled = !app->camera_enabled;
	if (app->camera_enabled) {
		glfwSetInputMode(app->window, GLFW_CURSOR,
		                 GLFW_CURSOR_DISABLED);
		app->first_mouse = 1;
	} else {
		glfwSetInputMode(app->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	LOG_INFO("suckless-ogl.app", "Camera control: %s",
	         app->camera_enabled ? "ENABLED" : "DISABLED");
	action_notifier_push(&app->notifier,
	                     app->camera_enabled ? "Camera: ON" : "Camera: OFF",
	                     NOTIF_DUR_NORMAL);
}

static void handle_fxaa_input(App* app, int mods)
{
	if (check_flag(mods, GLFW_MOD_SHIFT)) {
		postprocess_toggle(&app->postprocess, POSTFX_FXAA_DEBUG);
		int enabled = postprocess_is_enabled(&app->postprocess,
		                                     POSTFX_FXAA_DEBUG);
		LOG_INFO("suckless-ogl.app", "FXAA Debug: %s",
		         enabled ? "ON" : "OFF");
		action_notifier_push(
		    &app->notifier,
		    enabled ? "FXAA Debug: ON" : "FXAA Debug: OFF",
		    NOTIF_DUR_NORMAL);
	} else {
		postprocess_toggle(&app->postprocess, POSTFX_FXAA);
		int enabled =
		    postprocess_is_enabled(&app->postprocess, POSTFX_FXAA);
		LOG_INFO("suckless-ogl.app", "FXAA: %s",
		         enabled ? "ON" : "OFF");
		action_notifier_push(&app->notifier,
		                     enabled ? "FXAA: ON" : "FXAA: OFF",
		                     NOTIF_DUR_NORMAL);
	}
}

static void handle_f_key_input(App* app, int key, int mods)
{
	switch (key) {
		case GLFW_KEY_F1:
			handle_overlay_input(app);
			break;
		case GLFW_KEY_F2:
			app->show_help = !app->show_help;
			action_notifier_push(
			    &app->notifier,
			    app->show_help ? "Help: ON" : "Help: OFF",
			    NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_F3:
			if (check_flag(mods, GLFW_MOD_SHIFT)) {
				/* Toggle Position */
				gpu_profiler_ui_toggle_position(
				    &app->timeline_ui);

				const char* pos_str =
				    (app->timeline_ui.position == 0) ? "TOP"
				                                     : "BOTTOM";
				LOG_INFO("suckless-ogl.app",
				         "Timeline Position: %s", pos_str);

				char msg[NOTIF_BUF_SIZE];
				(void)safe_snprintf(msg, sizeof(msg),
				                    "Timeline: %s", pos_str);
				action_notifier_push(&app->notifier, msg,
				                     NOTIF_DUR_NORMAL);
			} else {
				/* Toggle Visibility */
				gpu_profiler_ui_toggle_visibility(
				    &app->timeline_ui);
				LOG_INFO(
				    "suckless-ogl.app", "GPU Timeline: %s",
				    app->timeline_ui.visible ? "ON" : "OFF");
				action_notifier_push(&app->notifier,
				                     app->timeline_ui.visible
				                         ? "Timeline: ON"
				                         : "Timeline: OFF",
				                     NOTIF_DUR_NORMAL);
			}
			break;
		case GLFW_KEY_F4:
			app->log_gpu_metrics = !app->log_gpu_metrics;
			LOG_INFO("suckless-ogl.app", "Log GPU Metrics: %s",
			         app->log_gpu_metrics ? "ON" : "OFF");
			action_notifier_push(&app->notifier,
			                     app->log_gpu_metrics
			                         ? "Log Metrics: ON"
			                         : "Log Metrics: OFF",
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_F9:
			if (app->perf_mode_active) {
				perf_mode_request_end(&app->perf_context);
				app->perf_mode_active = 0;
				action_notifier_push(&app->notifier,
				                     "Perf Mode: OFF",
				                     NOTIF_DUR_LONG);
			} else {
				app->perf_mode_active =
				    (perf_mode_request_start(
				         &app->perf_context) == 0);
				char buf[NOTIF_BUF_SIZE];
				(void)safe_snprintf(buf, sizeof(buf),
				                    "Perf Mode: ON (%s)",
				                    perf_mode_get_state_string(
				                        &app->perf_context));
				action_notifier_push(&app->notifier, buf,
				                     NOTIF_DUR_LONG);
			}
			LOG_INFO(
			    "suckless-ogl.app", "Performance Mode: %s (%s)",
			    app->perf_mode_active ? "ON" : "OFF",
			    perf_mode_get_state_string(&app->perf_context));
			break;
		default:
			break;
	}
}

static void handle_system_key_input(App* app, int key, int mods)
{
	switch (key) {
		case GLFW_KEY_P:
			app_save_raw_frame(app, "capture_frame.raw");
			action_notifier_push(&app->notifier, "Frame Captured",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_Z:
			app->wireframe = !app->wireframe;
			action_notifier_push(
			    &app->notifier,
			    app->wireframe ? "Wireframe: ON" : "Wireframe: OFF",
			    NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_UP:
		case GLFW_KEY_DOWN:
			handle_subdiv_input(app, key);
			break;
		case GLFW_KEY_C:
			handle_camera_toggle(app);
			break;
		case GLFW_KEY_SPACE:
			camera_init(&app->camera, DEFAULT_CAMERA_DISTANCE,
			            DEFAULT_CAMERA_YAW, DEFAULT_CAMERA_PITCH);
			app->env_lod = DEFAULT_ENV_LOD;
			LOG_INFO("suckless-ogl.app", "Camera and LOD reset");
			action_notifier_push(&app->notifier,
			                     "Camera & LOD Reset",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_PAGE_UP:
		case GLFW_KEY_PAGE_DOWN:
			app_handle_env_input(app, GLFW_PRESS, mods, key);
			break;
		case GLFW_KEY_F:
			app_toggle_fullscreen(app, app->window);
			break;
		default:
			break;
	}
}

void handle_app_input(App* app, int key, int mods)
{
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
		handle_f_key_input(app, key, mods);
		return;
	}

	switch (key) {
		case GLFW_KEY_P:
		case GLFW_KEY_Z:
		case GLFW_KEY_UP:
		case GLFW_KEY_DOWN:
		case GLFW_KEY_C:
		case GLFW_KEY_SPACE:
		case GLFW_KEY_PAGE_UP:
		case GLFW_KEY_PAGE_DOWN:
		case GLFW_KEY_F:
			handle_system_key_input(app, key, mods);
			break;
		case GLFW_KEY_L:
			app->billboard_mode = !app->billboard_mode;
			app_update_instancing_mode(app);
			LOG_INFO("suckless-ogl.app", "Billboard Mode: %s",
			         app->billboard_mode ? "ON" : "OFF");
			action_notifier_push(&app->notifier,
			                     app->billboard_mode
			                         ? "Billboards: ON"
			                         : "Billboards: OFF",
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_K:
			app->show_envmap = !app->show_envmap;
			LOG_INFO("suckless-ogl.app", "Envmap: %s",
			         app->show_envmap ? "ON" : "OFF");
			action_notifier_push(
			    &app->notifier,
			    app->show_envmap ? "Skybox: ON" : "Skybox: OFF",
			    NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_X:
			handle_fxaa_input(app, mods);
			break;
		default:
			handle_postprocess_input(app, key);
			break;
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action,
                  int mods)
{
	(void)scancode;
	App* app = (App*)glfwGetWindowUserPointer(window);
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		} else {
			handle_app_input(app, key, mods);
		}
	}
	camera_process_key_callback(&app->camera, key, action);
}

void camera_process_key_callback(Camera* camera, int key, int action)
{
	int pressed = (action != GLFW_RELEASE);
	if (key == GLFW_KEY_W) {
		camera->move_forward = pressed;
	}
	if (key == GLFW_KEY_S) {
		camera->move_backward = pressed;
	}
	if (key == GLFW_KEY_A) {
		camera->move_left = pressed;
	}
	if (key == GLFW_KEY_D) {
		camera->move_right = pressed;
	}
	if (key == GLFW_KEY_Q) {
		camera->move_up = pressed;
	}
	if (key == GLFW_KEY_E) {
		camera->move_down = pressed;
	}
}

void app_toggle_fullscreen(App* app, GLFWwindow* window)
{
	static const int REFRESH_RATE_WINDOWED = 0;
	if (app->is_fullscreen == 0) {
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		glfwGetWindowPos(window, &app->saved_x, &app->saved_y);
		glfwGetWindowSize(window, &app->saved_width,
		                  &app->saved_height);
		glfwSetWindowMonitor(window, monitor, 0, 0, mode->width,
		                     mode->height, mode->refreshRate);
		app->is_fullscreen = 1;
		LOG_INFO("suckless-ogl.app", "Switched to fullscreen (%dx%d)",
		         mode->width, mode->height);
	} else {
		glfwSetWindowMonitor(window, NULL, app->saved_x, app->saved_y,
		                     app->saved_width, app->saved_height,
		                     REFRESH_RATE_WINDOWED);
		app->is_fullscreen = 0;
		LOG_INFO("suckless-ogl.app", "Switched to windowed");
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	App* app = (App*)glfwGetWindowUserPointer(window);
	if (!app->camera_enabled) {
		return;
	}
	if (app->first_mouse) {
		app->last_mouse_x = xpos;
		app->last_mouse_y = ypos;
		app->first_mouse = 0;
		return;
	}
	double delta_x = xpos - app->last_mouse_x;
	double delta_y = ypos - app->last_mouse_y;
	app->last_mouse_x = xpos;
	app->last_mouse_y = ypos;
	camera_process_mouse(&app->camera, (float)delta_x, (float)delta_y);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void)xoffset;
	App* app = (App*)glfwGetWindowUserPointer(window);
	camera_process_scroll(&app->camera, (float)yoffset);
}

void app_save_raw_frame(App* app, const char* filename)
{
	int width = app->width;
	int height = app->height;
	size_t size = (size_t)width * (size_t)height * 3;
	unsigned char* pixels = malloc(size);
	if (!pixels) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate memory for RAW capture");
		return;
	}
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	FILE* file = fopen(filename, "wb");
	if (file) {
		if (fwrite(pixels, 1, size, file) != size) {
			LOG_ERROR("suckless-ogl.app",
			          "Failed to write RAW frame to file: %s",
			          filename);
		}
		(void)fclose(file);
		LOG_INFO("suckless-ogl.app", "RAW frame captured: %s",
		         filename);
	} else {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to open file for RAW capture");
	}
	free(pixels);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
}
