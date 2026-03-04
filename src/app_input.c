#include "app_input.h"

#include "action_notifier.h"
#include "app.h"
#include "app_settings.h"
#include "camera.h"
#include "camera_input.h"
#include "env_manager.h"
#include "glad/glad.h"
#include "log.h"
#include "perf_mode.h"
#include "postprocess.h" /* Explicit include for types */
#include "postprocess_input.h"
#include "profiler.h"
#include "scene.h"
#include "utils.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PBR_DEBUG_MODE_COUNT = 10 };

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

static void handle_pbr_debug_mode(App* app)
{
	app->scene.pbr_debug_mode =
	    (app->scene.pbr_debug_mode + 1) % PBR_DEBUG_MODE_COUNT;
	const char* modeNames[] = {"Final PBR",
	                           "Albedo",
	                           "Normal",
	                           "Metallic",
	                           "Roughness",
	                           "AO",
	                           "Irradiance (Diff)",
	                           "Prefilter (Spec)",
	                           "BRDF LUT",
	                           "1-Bounce GI (Probes)"};
	LOG_INFO("suckless-ogl.app", "PBR Debug Mode: %s",
	         modeNames[app->scene.pbr_debug_mode]);

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "Debug: %s",
	                    modeNames[app->scene.pbr_debug_mode]);
	action_notifier_push(&app->notifier, buf, NOTIF_DUR_LONG);
}

static void handle_aa_mode_input(App* app)
{
	app->scene.aa_mode = (app->scene.aa_mode + 1) % AA_MODE_COUNT;
	const char* mode_name = aa_mode_to_string(app->scene.aa_mode);
	LOG_INFO("suckless-ogl.app", "Specular AA Mode: %s", mode_name);

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "AA Mode: %s", mode_name);
	action_notifier_push(&app->notifier, buf, NOTIF_DUR_NORMAL);
}

void app_handle_env_input(App* app, int action, int mods, int key)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT) {
		return;
	}
	if (key == GLFW_KEY_PAGE_UP) {
		if (check_flag(mods, GLFW_MOD_SHIFT)) {
			app->scene.env_lod += LOD_STEP;
			if (app->scene.env_lod > MAX_ENV_LOD) {
				app->scene.env_lod = MAX_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->scene.env_lod);
			char lod_buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(lod_buf, sizeof(lod_buf),
			                    "Env LOD: %.1F",
			                    app->scene.env_lod);
			action_notifier_push(&app->notifier, lod_buf,
			                     NOTIF_DUR_SHORT);
		} else if (app->scene.hdr_count > 1) {
			app->scene.current_hdr_index =
			    (app->scene.current_hdr_index + 1) %
			    app->scene.hdr_count;
			env_manager_trigger_transition(
			    &app->env_mgr, app->async_loader,
			    app->scene.hdr_files[app->scene.current_hdr_index]);

			char buf[NOTIF_BUF_SIZE];
			const char* filename =
			    app->scene.hdr_files[app->scene.current_hdr_index];
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
			app->scene.env_lod -= LOD_STEP;
			if (app->scene.env_lod < MIN_ENV_LOD) {
				app->scene.env_lod = MIN_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->scene.env_lod);
			char lod_buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(lod_buf, sizeof(lod_buf),
			                    "Env LOD: %.1F",
			                    app->scene.env_lod);
			action_notifier_push(&app->notifier, lod_buf,
			                     NOTIF_DUR_SHORT);
		} else if (app->scene.hdr_count > 1) {
			app->scene.current_hdr_index--;
			if (app->scene.current_hdr_index < 0) {
				app->scene.current_hdr_index =
				    app->scene.hdr_count - 1;
			}
			env_manager_trigger_transition(
			    &app->env_mgr, app->async_loader,
			    app->scene.hdr_files[app->scene.current_hdr_index]);

			char buf[NOTIF_BUF_SIZE];
			const char* filename =
			    app->scene.hdr_files[app->scene.current_hdr_index];
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
	if (key == GLFW_KEY_UP && app->scene.subdivisions < MAX_SUBDIV) {
		app->scene.subdivisions++;
		changed = 1;
	} else if (key == GLFW_KEY_DOWN &&
	           app->scene.subdivisions > MIN_SUBDIV) {
		app->scene.subdivisions--;
		changed = 1;
	}

	if (changed) {
		char buf[NOTIF_BUF_SIZE];
		(void)safe_snprintf(buf, sizeof(buf), "Subdiv: %d",
		                    app->scene.subdivisions);
		action_notifier_push(&app->notifier, buf, NOTIF_DUR_SHORT);
	}
}

static void handle_camera_toggle(App* app)
{
	app->camera_enabled = !app->camera_enabled;
	if (app->camera_enabled) {
		glfwSetInputMode(app->window, GLFW_CURSOR,
		                 GLFW_CURSOR_DISABLED);
		app->camera.first_mouse = 1;
	} else {
		glfwSetInputMode(app->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	LOG_INFO("suckless-ogl.app", "Camera control: %s",
	         app->camera_enabled ? "ENABLED" : "DISABLED");
	action_notifier_push(&app->notifier,
	                     app->camera_enabled ? "Camera: ON" : "Camera: OFF",
	                     NOTIF_DUR_NORMAL);
}

static void handle_f3_input(App* app, int mods)
{
	if (check_flag(mods, GLFW_MOD_SHIFT)) {
		/* Toggle Position */
		gpu_profiler_ui_toggle_position(&app->timeline_ui);

		const char* pos_str =
		    (app->timeline_ui.position == 0) ? "TOP" : "BOTTOM";
		LOG_INFO("suckless-ogl.app", "Timeline Position: %s", pos_str);

		char msg[NOTIF_BUF_SIZE];
		(void)safe_snprintf(msg, sizeof(msg), "Timeline: %s", pos_str);
		action_notifier_push(&app->notifier, msg, NOTIF_DUR_NORMAL);
	} else {
		/* Toggle Visibility */
		gpu_profiler_ui_toggle_visibility(&app->timeline_ui);
		LOG_INFO("suckless-ogl.app", "GPU Timeline: %s",
		         app->timeline_ui.visible ? "ON" : "OFF");
		const char* status = "Timeline: OFF";
		if (app->timeline_ui.visible) {
			status = "Timeline: ON";
		}
		action_notifier_push(&app->notifier, status, NOTIF_DUR_NORMAL);
	}
}

static void handle_f9_input(App* app)
{
	PROFILE_ZONE(f9_zone, "Input: F9 (Performance Mode)");
	if (app->perf_mode_active != 0) {
		perf_mode_request_end(&app->perf_context);
		app->perf_mode_active = 0;
		action_notifier_push(&app->notifier, "Perf Mode: OFF",
		                     NOTIF_DUR_LONG);
	} else {
		app->perf_mode_active =
		    (perf_mode_request_start(&app->perf_context) == 0) ? 1 : 0;
		char buf[NOTIF_BUF_SIZE];
		(void)safe_snprintf(
		    buf, sizeof(buf), "Perf Mode: ON (%s)",
		    perf_mode_get_state_string(&app->perf_context));
		action_notifier_push(&app->notifier, buf, NOTIF_DUR_LONG);
	}
	LOG_INFO("suckless-ogl.app", "Performance Mode: %s (%s)",
	         (app->perf_mode_active != 0) ? "ON" : "OFF",
	         perf_mode_get_state_string(&app->perf_context));
	PROFILE_ZONE_END(f9_zone);
}

static void app_toggle_help(App* app)
{
	app->show_help = !app->show_help;
	action_notifier_push(&app->notifier,
	                     app->show_help ? "Help: ON" : "Help: OFF",
	                     NOTIF_DUR_NORMAL);
}

static bool handle_f_key_input(App* app, int key, int mods)
{
	switch (key) {
		case GLFW_KEY_F1:
			handle_overlay_input(app);
			return true;
		case GLFW_KEY_F2:
			app_toggle_help(app);
			return true;
		case GLFW_KEY_F3:
			handle_f3_input(app, mods);
			return true;
		case GLFW_KEY_F4:
			app->log_gpu_metrics =
			    (app->log_gpu_metrics != 0) ? 0 : 1;
			LOG_INFO("suckless-ogl.app", "Log GPU Metrics: %s",
			         (app->log_gpu_metrics != 0) ? "ON" : "OFF");
			action_notifier_push(&app->notifier,
			                     (app->log_gpu_metrics != 0)
			                         ? "Log Metrics: ON"
			                         : "Log Metrics: OFF",
			                     NOTIF_DUR_NORMAL);
			return true;
		case GLFW_KEY_F5:
			handle_pbr_debug_mode(app);
			return true;
		case GLFW_KEY_F9:
			handle_f9_input(app);
			return true;
		case GLFW_KEY_F12: {
			enum { FILENAME_BUF_SIZE = 64 };
			char filename[FILENAME_BUF_SIZE];
			time_t now = time(NULL);
			struct tm* time_info = localtime(&now);
			(void)strftime(filename, sizeof(filename),
			               "screenshot_%Y%m%d_%H%M%S.png",
			               time_info);
			app_save_png_frame(app, filename);
			char msg[NOTIF_BUF_SIZE];
			(void)safe_snprintf(msg, sizeof(msg),
			                    "Screenshot saved: %s", filename);
			action_notifier_push(&app->notifier, msg,
			                     NOTIF_DUR_LONG);
			return true;
		}
		default:
			return false;
	}
}

static void handle_y_key_input(App* app, int mods)
{
	if ((unsigned int)mods & (unsigned int)GLFW_MOD_SHIFT) {
		app->scene.show_probe_grid = !app->scene.show_probe_grid;
		LOG_INFO("suckless-ogl.app", "Probe Grid Debug: %s",
		         app->scene.show_probe_grid ? "ON" : "OFF");
		action_notifier_push(
		    &app->notifier,
		    app->scene.show_probe_grid ? "Probes: ON" : "Probes: OFF",
		    NOTIF_DUR_NORMAL);
	} else {
		app->scene.gi_mode = (app->scene.gi_mode + 1) % GI_MODE_COUNT;
		const char* mode_names[] = {"OFF", "3D Texture", "SSBO"};
		LOG_INFO("suckless-ogl.app", "GI Mode: %s",
		         mode_names[app->scene.gi_mode]);

		char buf[NOTIF_BUF_SIZE];
		(void)safe_snprintf(buf, sizeof(buf), "GI: %s",
		                    mode_names[app->scene.gi_mode]);
		action_notifier_push(&app->notifier, buf, NOTIF_DUR_NORMAL);
	}
}

static void handle_system_key_input(App* app, int key, int mods)
{
	switch (key) {
		case GLFW_KEY_P:
			app_save_png_frame(app, "capture_frame.png");
			action_notifier_push(&app->notifier, "Frame Captured",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_Z:
			app->scene.wireframe = !app->scene.wireframe;
			action_notifier_push(&app->notifier,
			                     app->scene.wireframe
			                         ? "Wireframe: ON"
			                         : "Wireframe: OFF",
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
			app->scene.env_lod = DEFAULT_ENV_LOD;
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
		if (handle_f_key_input(app, key, mods)) {
			return;
		}
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
		case GLFW_KEY_Y:
			handle_y_key_input(app, mods);
			break;
		case GLFW_KEY_L:
			app->scene.billboard_mode = !app->scene.billboard_mode;
			// app_update_instancing_mode(app) was empty and
			// removed.
			LOG_INFO("suckless-ogl.app", "Billboard Mode: %s",
			         app->scene.billboard_mode ? "ON" : "OFF");
			action_notifier_push(&app->notifier,
			                     app->scene.billboard_mode
			                         ? "Billboards: ON"
			                         : "Billboards: OFF",
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_O:
			app->scene.sorting_mode =
			    (app->scene.sorting_mode + 1) % SORTING_MODE_COUNT;
			const char* mode_name = "Unknown";
			const char* notif_name = "Sort: Unknown";

			switch (app->scene.sorting_mode) {
				case SORTING_MODE_CPU_QSORT:
					mode_name = "CPU (qsort)";
					notif_name = "Sort: CPU (qsort)";
					break;
				case SORTING_MODE_CPU_RADIX:
					mode_name = "CPU (Radix)";
					notif_name = "Sort: CPU (Radix)";
					break;
				case SORTING_MODE_GPU_BITONIC:
					mode_name = "GPU (Bitonic)";
					notif_name = "Sort: GPU (Bitonic)";
					break;
				default:
					break;
			}

			LOG_INFO("suckless-ogl.app", "Sphere Sorting: %s",
			         mode_name);
			action_notifier_push(&app->notifier, notif_name,
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_T:
			app->env_mgr.env_transition_mode =
			    (app->env_mgr.env_transition_mode + 1) % 2;
			LOG_INFO("suckless-ogl.app",
			         "Environment Transition Mode: %s",
			         app->env_mgr.env_transition_mode ==
			                 ENV_TRANSITION_CROSSFADE
			             ? "CROSSFADE"
			             : "BLACK_SCREEN");
			char transition_buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(transition_buf,
			                    sizeof(transition_buf),
			                    "Transition: %s",
			                    app->env_mgr.env_transition_mode ==
			                            ENV_TRANSITION_CROSSFADE
			                        ? "CROSSFADE"
			                        : "BLACK_SCREEN");
			action_notifier_push(&app->notifier, transition_buf,
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_N:
			if (check_flag(mods, GLFW_MOD_SHIFT)) {
				handle_aa_mode_input(app);
			} else {
				app->scene.specular_aa_enabled =
				    !app->scene.specular_aa_enabled;
				action_notifier_push(
				    &app->notifier,
				    app->scene.specular_aa_enabled
				        ? "Specular AA: ON"
				        : "Specular AA: OFF",
				    NOTIF_DUR_SHORT);
			}
			break;
		case GLFW_KEY_K:
			app->scene.show_envmap = !app->scene.show_envmap;
			LOG_INFO("suckless-ogl.app", "Envmap: %s",
			         app->scene.show_envmap ? "ON" : "OFF");
			action_notifier_push(&app->notifier,
			                     app->scene.show_envmap
			                         ? "Skybox: ON"
			                         : "Skybox: OFF",
			                     NOTIF_DUR_NORMAL);
			break;
		default:
			break;
	}

	PostProcessInputContext pp_ctx = {.postprocess = &app->postprocess,
	                                  .notifier = &app->notifier,
	                                  .effect_bench = &app->effect_bench,
	                                  .window = app->window};
	postprocess_input_handle_key(&pp_ctx, key, mods);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action,
                  int mods)
{
	(void)scancode;
	App* app = (App*)glfwGetWindowUserPointer(window);
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE) {
			if (app->show_help) {
				app_toggle_help(app);
			} else {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		} else if (app->show_help) {
			/* Dry-run mode: intercept keys except for Close/Toggle
			 * keys */
			if (key == GLFW_KEY_F2) {
				handle_app_input(app, key, mods);
			} else {
				app->help_pressed_key = key;
				app->help_pressed_mods = mods;
				app->help_press_timer =
				    (double)HELP_PRESS_DURATION; /* Highlight
				                                    duration */
			}
		} else {
			handle_app_input(app, key, mods);
		}
	}
	if (!app->show_help) {
		camera_input_handle_key(&app->camera, key, action);
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
	camera_input_handle_mouse(&app->camera, xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void)xoffset;
	App* app = (App*)glfwGetWindowUserPointer(window);
	camera_input_handle_scroll(&app->camera, yoffset);
}

void app_save_png_frame(App* app, const char* filename)
{
	int width = app->width;
	int height = app->height;
	int channels = 3;
	size_t size = (size_t)width * (size_t)height * (size_t)channels;
	unsigned char* pixels = malloc(size);
	if (!pixels) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate memory for PNG capture");
		return;
	}
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	/* Flip vertically: OpenGL reads bottom-to-top, PNG is top-to-bottom */
	int row_sz = width * channels;
	for (int yr = 0; yr < height / 2; yr++) {
		unsigned char* top = pixels + (yr * row_sz);
		unsigned char* bot = pixels + (((height - yr) - 1) * row_sz);
		for (int xi = 0; xi < row_sz; xi++) {
			unsigned char tmp = top[xi];
			top[xi] = bot[xi];
			bot[xi] = tmp;
		}
	}

	if (stbi_write_png(filename, width, height, channels, pixels,
	                   width * channels) != 0) {
		LOG_INFO("suckless-ogl.app", "PNG frame captured: %s",
		         filename);
	} else {
		LOG_ERROR("suckless-ogl.app", "Failed to write PNG frame: %s",
		          filename);
	}
	free(pixels);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
}
