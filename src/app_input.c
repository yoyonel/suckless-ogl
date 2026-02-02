#include "app_input.h"

#include "app.h"
#include "app_env.h"
#include "app_scene.h"
#include "app_settings.h"
#include "camera.h"
#include "glad/glad.h"
#include "log.h"
#include "postprocess.h" /* Explicit include for types */
#include "postprocess_presets.h"
#include "utils.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

enum { PBR_DEBUG_MODE_COUNT = 9 };

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
			break;
		case GLFW_KEY_2: /* Preset: Subtle */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_SUBTLE);
			LOG_INFO("suckless-ogl.app", "Style: Subtle");
			break;
		case GLFW_KEY_3: /* Preset: Cinématique */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_CINEMATIC);
			LOG_INFO("suckless-ogl.app", "Style: Cinématique");
			break;
		case GLFW_KEY_4: /* Preset: Vintage */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_VINTAGE);
			LOG_INFO("suckless-ogl.app", "Style: Vintage");
			break;
		case GLFW_KEY_5: /* Style: "Matrix" */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_MATRIX);
			LOG_INFO("suckless-ogl.app", "Style: Matrix Grading");
			break;
		case GLFW_KEY_6: /* Style: "Noir et Blanc Contrasté" */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_BW_CONTRAST);
			LOG_INFO("suckless-ogl.app", "Style: Noir & Blanc");
			break;
		case GLFW_KEY_7: { /* Style Cycle: All Banding Styles */
			static int banding_style_idx = 0;
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
				banding_style_idx = 0;
			} else {
				banding_style_idx =
				    (banding_style_idx + 1) % num_styles;
			}

			postprocess_apply_preset(
			    &app->postprocess,
			    banding_presets[banding_style_idx]);
			LOG_INFO("suckless-ogl.app",
			         "Banding Style [%d/%d]: %s",
			         banding_style_idx + 1, num_styles,
			         banding_names[banding_style_idx]);
			break;
		}
		case GLFW_KEY_8:
			/* Key 8 is now free or can be used for something else,
			   but let's keep it for another preset if needed. */
			break;
		case GLFW_KEY_0:
		case GLFW_KEY_KP_0:
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(&app->postprocess,
			                         app->auto_threshold);
			LOG_INFO("suckless-ogl.app",
			         "Color Grading: Reset to Defaults");
			break;
		default:
			break;
	}
}

static void toggle_postfx(App* app, PostProcessEffect feature, const char* name)
{
	postprocess_toggle(&app->postprocess, feature);
	LOG_INFO(
	    "suckless-ogl.app", "%s: %s", name,
	    postprocess_is_enabled(&app->postprocess, feature) ? "ON" : "OFF");
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
			toggle_postfx_complex(
			    app, POSTFX_MOTION_BLUR, POSTFX_MOTION_BLUR_DEBUG,
			    "Motion Blur", "Motion Blur DEBUG");
			break;
		case GLFW_KEY_R:
			LOG_INFO("suckless-ogl.app",
			         "Shader reloading not implemented yet");
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
		} else if (app->hdr_count > 1) {
			app->current_hdr_index =
			    (app->current_hdr_index + 1) % app->hdr_count;
			app_load_env_map(
			    app, app->hdr_files[app->current_hdr_index]);
		}
	} else if (key == GLFW_KEY_PAGE_DOWN) {
		if (check_flag(mods, GLFW_MOD_SHIFT)) {
			app->env_lod -= LOD_STEP;
			if (app->env_lod < MIN_ENV_LOD) {
				app->env_lod = MIN_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->env_lod);
		} else if (app->hdr_count > 1) {
			app->current_hdr_index--;
			if (app->current_hdr_index < 0) {
				app->current_hdr_index = app->hdr_count - 1;
			}
			app_load_env_map(
			    app, app->hdr_files[app->current_hdr_index]);
		}
	}
}

void handle_app_input(App* app, int key, int mods)
{
	switch (key) {
		case GLFW_KEY_F1: {
			static const char* mode_names[] = {
			    "Off", "FPS + Position", "FPS + Position + Envmap",
			    "FPS + Position + Envmap + Exposure"};
			static const int mode_count =
			    sizeof(mode_names) / sizeof(mode_names[0]);
			app->text_overlay_mode =
			    (app->text_overlay_mode + 1) % mode_count;
			LOG_INFO("suckless-ogl.app", "Text Overlay: %s",
			         mode_names[app->text_overlay_mode]);
		} break;
		case GLFW_KEY_F2:
			app->show_help = !app->show_help;
			break;
		case GLFW_KEY_P:
			app_save_raw_frame(app, "capture_frame.raw");
			break;
		case GLFW_KEY_Z:
			app->wireframe = !app->wireframe;
			break;
		case GLFW_KEY_UP:
			if (app->subdivisions < MAX_SUBDIV) {
				app->subdivisions++;
			}
			break;
		case GLFW_KEY_DOWN:
			if (app->subdivisions > MIN_SUBDIV) {
				app->subdivisions--;
			}
			break;
		case GLFW_KEY_C:
			app->camera_enabled = !app->camera_enabled;
			if (app->camera_enabled) {
				glfwSetInputMode(app->window, GLFW_CURSOR,
				                 GLFW_CURSOR_DISABLED);
				app->first_mouse = 1;
			} else {
				glfwSetInputMode(app->window, GLFW_CURSOR,
				                 GLFW_CURSOR_NORMAL);
			}
			LOG_INFO("suckless-ogl.app", "Camera control: %s",
			         app->camera_enabled ? "ENABLED" : "DISABLED");
			break;
		case GLFW_KEY_SPACE:
			camera_init(&app->camera, DEFAULT_CAMERA_DISTANCE,
			            DEFAULT_CAMERA_YAW, DEFAULT_CAMERA_PITCH);
			app->env_lod = DEFAULT_ENV_LOD;
			LOG_INFO("suckless-ogl.app", "Camera and LOD reset");
			break;
		case GLFW_KEY_PAGE_UP:
		case GLFW_KEY_PAGE_DOWN:
			app_handle_env_input(app, GLFW_PRESS, mods, key);
			break;
		case GLFW_KEY_F:
			app_toggle_fullscreen(app, app->window);
			break;
		case GLFW_KEY_L:
			app->billboard_mode = !app->billboard_mode;
			/* Forward declaration or move
			 * app_update_instancing_mode to scene */
			app_update_instancing_mode(app);
			LOG_INFO("suckless-ogl.app", "Billboard Mode: %s",
			         app->billboard_mode ? "ON" : "OFF");
			break;
		case GLFW_KEY_K:
			app->show_envmap = !app->show_envmap;
			LOG_INFO("suckless-ogl.app", "Envmap: %s",
			         app->show_envmap ? "ON" : "OFF");
			break;
		case GLFW_KEY_X:
			if (check_flag(mods, GLFW_MOD_SHIFT)) {
				postprocess_toggle(&app->postprocess,
				                   POSTFX_FXAA_DEBUG);
				LOG_INFO(
				    "suckless-ogl.app", "FXAA Debug: %s",
				    postprocess_is_enabled(&app->postprocess,
				                           POSTFX_FXAA_DEBUG)
				        ? "ON"
				        : "OFF");
			} else {
				postprocess_toggle(&app->postprocess,
				                   POSTFX_FXAA);
				LOG_INFO("suckless-ogl.app", "FXAA: %s",
				         postprocess_is_enabled(
				             &app->postprocess, POSTFX_FXAA)
				             ? "ON"
				             : "OFF");
			}
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
