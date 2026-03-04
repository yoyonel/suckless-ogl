#include "postprocess_input.h"

#include "log.h"
#include "postprocess_presets.h"
#include "utils.h"
#include <string.h>

enum { NOTIF_BUF_SIZE = 128 };

static void toggle_postfx(const PostProcessInputContext* ctx,
                          PostProcessEffect feature, const char* name)
{
	postprocess_toggle(ctx->postprocess, feature);
	int enabled = postprocess_is_enabled(ctx->postprocess, feature);
	LOG_INFO("suckless-ogl.postprocess", "%s: %s", name,
	         enabled ? "ON" : "OFF");

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "%s: %s", name,
	                    enabled ? "ON" : "OFF");
	action_notifier_push(ctx->notifier, buf, NOTIF_DUR_NORMAL);
}

static void toggle_postfx_complex(const PostProcessInputContext* ctx, int mods,
                                  PostProcessEffect feature,
                                  PostProcessEffect debug_feature,
                                  const char* name, const char* debug_name)
{
	if (check_flag(mods, GLFW_MOD_SHIFT)) {
		toggle_postfx(ctx, debug_feature, debug_name);
	} else {
		toggle_postfx(ctx, feature, name);
	}
}

static void handle_exposure_input(const PostProcessInputContext* ctx, int key)
{
	float current = ctx->postprocess->exposure.exposure;
	float next_val = current;

	if (key == GLFW_KEY_KP_ADD) {
		next_val += DEFAULT_EXPOSURE_STEP;
	} else if (key == GLFW_KEY_KP_SUBTRACT) {
		next_val = (current > DEFAULT_MIN_EXPOSURE)
		               ? current - DEFAULT_EXPOSURE_STEP
		               : DEFAULT_MIN_EXPOSURE;
	}

	postprocess_set_exposure(ctx->postprocess, next_val);
	LOG_INFO("suckless-ogl.postprocess", "Exposure: %.2f",
	         ctx->postprocess->exposure.exposure);

	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "Exposure: %.2f",
	                    ctx->postprocess->exposure.exposure);
	action_notifier_push(ctx->notifier, buf, NOTIF_DUR_SHORT);
}

static void handle_preset_input(const PostProcessInputContext* ctx, int key)
{
	switch (key) {
		case GLFW_KEY_1: /* Preset: Aucun */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(
			    ctx->postprocess, ctx->postprocess->auto_threshold);
			LOG_INFO("suckless-ogl.postprocess",
			         "Style: Aucun (rendu pur) - Exposure: %.2f",
			         ctx->postprocess->auto_threshold);
			action_notifier_push(ctx->notifier,
			                     "Style: Pure Render",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_2: /* Preset: Subtle */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_SUBTLE);
			LOG_INFO("suckless-ogl.postprocess", "Style: Subtle");
			action_notifier_push(ctx->notifier, "Style: Subtle",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_3: /* Preset: Cinématique */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_CINEMATIC);
			LOG_INFO("suckless-ogl.postprocess",
			         "Style: Cinématique");
			action_notifier_push(ctx->notifier, "Style: Cinematic",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_4: /* Preset: Vintage */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_VINTAGE);
			LOG_INFO("suckless-ogl.postprocess", "Style: Vintage");
			action_notifier_push(ctx->notifier, "Style: Vintage",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_5: /* Style: "Matrix" */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_MATRIX);
			LOG_INFO("suckless-ogl.postprocess",
			         "Style: Matrix Grading");
			action_notifier_push(ctx->notifier, "Style: Matrix",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_6: /* Style: "Noir et Blanc Contrasté" */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_BW_CONTRAST);
			LOG_INFO("suckless-ogl.postprocess",
			         "Style: Noir & Blanc");
			action_notifier_push(ctx->notifier,
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
			if (!postprocess_is_enabled(ctx->postprocess,
			                            POSTFX_BANDING)) {
				ctx->postprocess->banding_preset_idx = 0;
			} else {
				ctx->postprocess->banding_preset_idx =
				    (ctx->postprocess->banding_preset_idx + 1) %
				    num_styles;
			}

			postprocess_apply_preset(
			    ctx->postprocess,
			    banding_presets[ctx->postprocess
			                        ->banding_preset_idx]);
			LOG_INFO("suckless-ogl.postprocess",
			         "Banding Style [%d/%d]: %s",
			         ctx->postprocess->banding_preset_idx + 1,
			         num_styles,
			         banding_names[ctx->postprocess
			                           ->banding_preset_idx]);

			char buf[NOTIF_BUF_SIZE];
			(void)safe_snprintf(
			    buf, sizeof(buf), "Banding: %s",
			    banding_names[ctx->postprocess
			                      ->banding_preset_idx]);
			action_notifier_push(ctx->notifier, buf,
			                     NOTIF_DUR_LONG);
			break;
		}
		case GLFW_KEY_8:
			if (ctx->effect_bench &&
			    !effect_benchmark_is_running(ctx->effect_bench)) {
				effect_benchmark_start(ctx->effect_bench);
				action_notifier_push(ctx->notifier,
				                     "FX Benchmark: Started",
				                     NOTIF_DUR_LONG);
			} else {
				action_notifier_push(
				    ctx->notifier,
				    "FX Benchmark: Already running",
				    NOTIF_DUR_NORMAL);
			}
			break;
		case GLFW_KEY_0:
		case GLFW_KEY_KP_0:
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(
			    ctx->postprocess, ctx->postprocess->auto_threshold);
			LOG_INFO("suckless-ogl.postprocess",
			         "Color Grading: Reset to Defaults");
			action_notifier_push(ctx->notifier,
			                     "FX: Reset to Defaults",
			                     NOTIF_DUR_LONG);
			break;
		default:
			break;
	}
}

static void handle_fxaa_input(const PostProcessInputContext* ctx, int mods)
{
	if (check_flag(mods, GLFW_MOD_SHIFT)) {
		postprocess_toggle(ctx->postprocess, POSTFX_FXAA_DEBUG);
		int enabled =
		    postprocess_is_enabled(ctx->postprocess, POSTFX_FXAA_DEBUG);
		LOG_INFO("suckless-ogl.postprocess", "FXAA Debug: %s",
		         enabled ? "ON" : "OFF");
		action_notifier_push(
		    ctx->notifier,
		    enabled ? "FXAA Debug: ON" : "FXAA Debug: OFF",
		    NOTIF_DUR_NORMAL);
	} else {
		postprocess_toggle(ctx->postprocess, POSTFX_FXAA);
		int enabled =
		    postprocess_is_enabled(ctx->postprocess, POSTFX_FXAA);
		LOG_INFO("suckless-ogl.postprocess", "FXAA: %s",
		         enabled ? "ON" : "OFF");
		action_notifier_push(ctx->notifier,
		                     enabled ? "FXAA: ON" : "FXAA: OFF",
		                     NOTIF_DUR_NORMAL);
	}
}

void postprocess_input_handle_key(const PostProcessInputContext* ctx, int key,
                                  int mods)
{
	if (key == GLFW_KEY_X) {
		handle_fxaa_input(ctx, mods);
		return;
	}

	switch (key) {
		case GLFW_KEY_V:
			toggle_postfx(ctx, POSTFX_VIGNETTE, "Vignette");
			break;
		case GLFW_KEY_G:
			toggle_postfx(ctx, POSTFX_GRAIN, "Grain");
			break;
		case GLFW_KEY_B:
			toggle_postfx(ctx, POSTFX_BLOOM, "Bloom");
			break;
		case GLFW_KEY_H:
			toggle_postfx_complex(ctx, mods, POSTFX_DOF,
			                      POSTFX_DOF_DEBUG, "DOF",
			                      "DOF DEBUG");
			break;
		case GLFW_KEY_M:
			if (!check_flag(mods, GLFW_MOD_SHIFT)) {
				/* M: Toggle Motion Blur */
				toggle_postfx(ctx, POSTFX_MOTION_BLUR,
				              "Motion Blur");
				break;
			}

			/* SHIFT+M: Cycle Debug Views: Off → MB Debug
			   → Vector Field → Off */
			int mb_dbg = postprocess_is_enabled(
			    ctx->postprocess, POSTFX_MOTION_BLUR_DEBUG);
			int vf_dbg = postprocess_is_enabled(
			    ctx->postprocess, POSTFX_VECTOR_FIELD_DEBUG);

			const char* mode_name = NULL;
			if (!mb_dbg && !vf_dbg) {
				/* Off → Motion Blur Debug */
				postprocess_enable(ctx->postprocess,
				                   POSTFX_MOTION_BLUR_DEBUG);
				mode_name = "Motion Blur Debug (RG)";
			} else if (mb_dbg) {
				/* MB Debug → Vector Field */
				postprocess_disable(ctx->postprocess,
				                    POSTFX_MOTION_BLUR_DEBUG);
				postprocess_enable(ctx->postprocess,
				                   POSTFX_VECTOR_FIELD_DEBUG);
				mode_name = "Vector Field Debug";
			} else {
				/* Vector Field → Off */
				postprocess_disable(ctx->postprocess,
				                    POSTFX_VECTOR_FIELD_DEBUG);
				mode_name = "Debug: OFF";
			}
			LOG_INFO("suckless-ogl.postprocess",
			         "Velocity Debug: %s", mode_name);
			action_notifier_push(ctx->notifier, mode_name,
			                     NOTIF_DUR_NORMAL);
			break;

		case GLFW_KEY_R:
			LOG_INFO("suckless-ogl.postprocess",
			         "Shader reloading not implemented yet");
			action_notifier_push(ctx->notifier,
			                     "Hot-Reload: Not Implemented",
			                     NOTIF_DUR_NORMAL);
			break;
		case GLFW_KEY_KP_ADD:
		case GLFW_KEY_KP_SUBTRACT:
			handle_exposure_input(ctx, key);
			break;
		case GLFW_KEY_J:
			toggle_postfx_complex(ctx, mods, POSTFX_AUTO_EXPOSURE,
			                      POSTFX_EXPOSURE_DEBUG,
			                      "Auto Exposure",
			                      "Auto Exposure Debug");
			break;
		case GLFW_KEY_F6:
			toggle_postfx(ctx, POSTFX_STENCIL_DEBUG,
			              "Stencil Debug");
			break;
		default:
			handle_preset_input(ctx, key);
			break;
	}
}
