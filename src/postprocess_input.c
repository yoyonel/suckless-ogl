#include "postprocess_input.h"

#include "effects/fx_auto_exposure.h"
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

static void handle_preset_input(const PostProcessInputContext* ctx, int key,
                                int mods)
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
		case GLFW_KEY_9: /* Preset: Nordic Noir */
			postprocess_apply_preset(ctx->postprocess,
			                         &PRESET_NORDIC_NOIR);
			LOG_INFO("suckless-ogl.postprocess",
			         "Style: Nordic Noir");
			action_notifier_push(ctx->notifier,
			                     "Style: Nordic Noir",
			                     NOTIF_DUR_LONG);
			break;
		case GLFW_KEY_F8: /* Preset: Sony A7S III / Cycle LUTs */
			if (check_flag(mods, GLFW_MOD_SHIFT)) {
				int* lut_idx_ptr =
				    &ctx->postprocess->lut3d_fx.current_lut_idx;
				const char* luts[] = {
				    "assets/luts/sony_scinetone.cube",
				    "assets/luts/sony_venice.cube",
				    "assets/luts/kodak_vision3.cube",
				    "assets/luts/fuji_eternal.cube",
				    "assets/luts/teal_orange.cube",
				    "assets/luts/vintage.cube",
				    "assets/luts/sony_a7siii_poc.cube"};
				const char* names[] = {
				    "Sony S-Cinetone", "Sony Venice Look",
				    "Kodak Vision3",   "Fujifilm Eternal",
				    "Teal & Orange",   "Vintage Film",
				    "Alpha 7S III POC"};
				int count =
				    (int)(sizeof(luts) / sizeof(luts[0]));
				*lut_idx_ptr = (*lut_idx_ptr + 1) % count;

				if (postprocess_load_lut3d(
				        ctx->postprocess, luts[*lut_idx_ptr]) ==
				    0) {
					postprocess_enable(ctx->postprocess,
					                   POSTFX_LUT3D);
					action_notifier_push(
					    ctx->notifier, names[*lut_idx_ptr],
					    NOTIF_DUR_SHORT);
					LOG_INFO("suckless-ogl.input",
					         "Loaded LUT: %s",
					         names[*lut_idx_ptr]);
				}
			} else {
				postprocess_apply_preset(ctx->postprocess,
				                         &PRESET_SONY_A7SIII);
				postprocess_load_lut3d(
				    ctx->postprocess,
				    "assets/luts/sony_scinetone.cube");
				LOG_INFO("suckless-ogl.input",
				         "Style: Sony Alpha 7S III");
				action_notifier_push(ctx->notifier,
				                     "Style: Sony A7S III",
				                     NOTIF_DUR_LONG);
			}
			break;
		case GLFW_KEY_F10:
			if (check_flag(mods, GLFW_MOD_SHIFT)) {
				const bool is_enabled =
				    (bool)(!ctx->postprocess->lut_viz_fx
				                .is_enabled);
				ctx->postprocess->lut_viz_fx.is_enabled =
				    is_enabled;

				const char* status_str = "OFF";
				if (is_enabled) {
					status_str = "ON";
				}

				LOG_INFO("suckless-ogl.postprocess",
				         "LUT Visualization: %s", status_str);

				char buf[NOTIF_BUF_SIZE];
				(void)safe_snprintf(buf, sizeof(buf),
				                    "LUT Viz: %s", status_str);

				action_notifier_push(ctx->notifier, buf,
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

static void handle_ae_path_toggle(const PostProcessInputContext* ctx)
{
	fx_auto_exposure_toggle_path(ctx->postprocess);
	const char* pname = fx_auto_exposure_path_name(ctx->postprocess);
	char buf[NOTIF_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "AE Path: %s", pname);
	action_notifier_push(ctx->notifier, buf, NOTIF_DUR_NORMAL);
}

static void handle_auto_exposure_key(const PostProcessInputContext* ctx,
                                     int mods)
{
	if (check_flag(mods, GLFW_MOD_CONTROL)) {
		handle_ae_path_toggle(ctx);
	} else {
		toggle_postfx_complex(ctx, mods, POSTFX_AUTO_EXPOSURE,
		                      POSTFX_EXPOSURE_DEBUG, "Auto Exposure",
		                      "Auto Exposure Debug");
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
			if (check_flag(mods, GLFW_MOD_SHIFT)) {
				/* SHIFT+B: Cycle Bloom Debug Mode:
				   Off -> Final -> Prefilter -> Downsample ->
				   Upsample -> Off */
				int debug_enabled = postprocess_is_enabled(
				    ctx->postprocess, POSTFX_BLOOM_DEBUG);
				int current_step =
				    ctx->postprocess->bloom_fx.debug_step;

				const char* mode_name = NULL;
				if (!debug_enabled) {
					/* Off -> Final */
					postprocess_enable(ctx->postprocess,
					                   POSTFX_BLOOM_DEBUG);
					/* Ensure bloom is on for debug */
					postprocess_enable(ctx->postprocess,
					                   POSTFX_BLOOM);
					ctx->postprocess->bloom_fx.debug_step =
					    0;
					mode_name = "Bloom Debug: Final Map";
				} else if (current_step == 0) {
					/* Final -> Prefilter */
					ctx->postprocess->bloom_fx.debug_step =
					    1;
					mode_name = "Bloom Debug: Prefilter";
				} else if (current_step == 1) {
					/* Prefilter -> Downsample */
					ctx->postprocess->bloom_fx.debug_step =
					    2;
					mode_name = "Bloom Debug: Downsample";
				} else if (current_step == 2) {
					/* Downsample -> Upsample (is actually
					 * final map in our impl, let's keep it
					 * for completeness or intermediate if
					 * we want) */
					/* For now let's just use it as
					 * "Upsample/Final" */
					ctx->postprocess->bloom_fx.debug_step =
					    3;
					mode_name = "Bloom Debug: Upsample";
				} else {
					/* Upsample -> Off */
					postprocess_disable(ctx->postprocess,
					                    POSTFX_BLOOM_DEBUG);
					mode_name = "Bloom Debug: OFF";
				}
				LOG_INFO("suckless-ogl.postprocess", "%s",
				         mode_name);
				action_notifier_push(ctx->notifier, mode_name,
				                     NOTIF_DUR_NORMAL);
			} else if (check_flag(mods, GLFW_MOD_ALT)) {
				/* ALT+B: Cycle Bloom Debug Mip (0-4) */
				if (postprocess_is_enabled(
				        ctx->postprocess, POSTFX_BLOOM_DEBUG)) {
					ctx->postprocess->bloom_fx.debug_mip =
					    (ctx->postprocess->bloom_fx
					         .debug_mip +
					     1) %
					    BLOOM_MIP_LEVELS;

					char buf[NOTIF_BUF_SIZE];
					(void)safe_snprintf(
					    buf, sizeof(buf),
					    "Bloom Debug Mip: %d",
					    ctx->postprocess->bloom_fx
					        .debug_mip);
					LOG_INFO("suckless-ogl.postprocess",
					         "%s", buf);
					action_notifier_push(ctx->notifier, buf,
					                     NOTIF_DUR_SHORT);
				}
			} else {
				toggle_postfx(ctx, POSTFX_BLOOM, "Bloom");
			}
			break;
		case GLFW_KEY_H:
			toggle_postfx_complex(ctx, mods, POSTFX_DOF,
			                      POSTFX_DOF_DEBUG, "DOF",
			                      "DOF DEBUG");
			break;
		case GLFW_KEY_U:
			toggle_postfx(ctx, POSTFX_CHROM_ABBR,
			              "Chromatic Aberration");
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
			handle_auto_exposure_key(ctx, mods);
			break;
		case GLFW_KEY_F6:
			toggle_postfx(ctx, POSTFX_STENCIL_DEBUG,
			              "Stencil Debug");
			break;
		case GLFW_KEY_F7:
			toggle_postfx_complex(
			    ctx, mods, POSTFX_FOG, POSTFX_FOG_DEBUG,
			    "Atmospheric Fog", "Fog Debug View");
			break;
		default:
			handle_preset_input(ctx, key, mods);
			break;
	}
}
