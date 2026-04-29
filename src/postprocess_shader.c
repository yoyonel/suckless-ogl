#include "log.h"
#include "postprocess_internal.h"
#include "profiler.h"
#include "utils.h"

bool pp_is_shader_in_cache(PostProcess* post_processing, Shader* shader)
{
	if (!shader) {
		return false;
	}
	for (int i = 0; i < post_processing->shaders.shader_cache_count; i++) {
		if (post_processing->shaders.shader_cache[i].shader == shader) {
			return true;
		}
	}
	return false;
}

void pp_setup_sampler_uniforms(PostProcess* post_processing)
{
	Shader* shader = post_processing->shaders.postprocess_shader;
	if (!shader) {
		return;
	}

	shader_use(shader);
	shader_set_int(shader, "screenTexture", POSTPROCESS_TEX_UNIT_SCENE);
	shader_set_int(shader, "bloomTexture", POSTPROCESS_TEX_UNIT_BLOOM);
	shader_set_int(shader, "depthTexture", POSTPROCESS_TEX_UNIT_DEPTH);
	shader_set_int(shader, "autoExposureTexture",
	               POSTPROCESS_TEX_UNIT_EXPOSURE);
	shader_set_int(shader, "velocityTexture",
	               POSTPROCESS_TEX_UNIT_VELOCITY);
	shader_set_int(shader, "neighborMaxTexture",
	               POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);
	shader_set_int(shader, "dofBlurTexture", POSTPROCESS_TEX_UNIT_DOF_BLUR);
	shader_set_int(shader, "stencilTexture", POSTPROCESS_TEX_UNIT_STENCIL);
	shader_set_int(shader, "u_lut_tex", POSTPROCESS_TEX_UNIT_LUT3D);
}

void pp_update_current_shader(PostProcess* post_processing, Shader* new_shader,
                              bool is_optimized)
{
	/* Destroy previous shader only if it was dynamic (not in cache) */
	if (post_processing->shaders.postprocess_shader &&
	    !pp_is_shader_in_cache(
	        post_processing, post_processing->shaders.postprocess_shader)) {
		shader_destroy(post_processing->shaders.postprocess_shader);
	}

	post_processing->shaders.postprocess_shader = new_shader;
	post_processing->shaders.is_optimized = is_optimized;

	/* Sampler→unit bindings are per-program state.
	 * Set them once when the active shader changes. */
	pp_setup_sampler_uniforms(post_processing);
}

enum { MAX_SHADER_DEFINES = 32, MAX_DEFINE_LENGTH = 64 };

typedef struct {
	PostProcessEffect flag;
	const char* name;
	const char* define_name;
} EffectMetadata;

static const EffectMetadata ALL_EFFECTS[] = {
    {POSTFX_VIGNETTE, "Vignette", "OPT_ENABLE_VIGNETTE"},
    {POSTFX_GRAIN, "Film Grain", "OPT_ENABLE_GRAIN"},
    {POSTFX_EXPOSURE, "Manual Exposure", "OPT_ENABLE_EXPOSURE"},
    {POSTFX_CHROM_ABBR, "Chromatic Aberration", "OPT_ENABLE_CHROM_ABBR"},
    {POSTFX_BLOOM, "Bloom", "OPT_ENABLE_BLOOM"},
    {POSTFX_COLOR_GRADING, "Color Grading", "OPT_ENABLE_COLOR_GRADING"},
    {POSTFX_DOF, "Depth of Field", "OPT_ENABLE_DOF"},
    {POSTFX_DOF_DEBUG, "DoF Debug View", "OPT_ENABLE_DOF_DEBUG"},
    {POSTFX_AUTO_EXPOSURE, "Auto-Exposure", "OPT_ENABLE_AUTO_EXPOSURE"},
    {POSTFX_EXPOSURE_DEBUG, "Exposure Debug View", "OPT_ENABLE_EXPOSURE_DEBUG"},
    {POSTFX_MOTION_BLUR, "Motion Blur", "OPT_ENABLE_MOTION_BLUR"},
    {POSTFX_MOTION_BLUR_DEBUG, "Motion Blur Debug View",
     "OPT_ENABLE_MOTION_BLUR_DEBUG"},
    {POSTFX_FXAA, "FXAA", "OPT_ENABLE_FXAA"},
    {POSTFX_FXAA_DEBUG, "FXAA Debug View", "OPT_ENABLE_FXAA_DEBUG"},
    {POSTFX_BANDING, "Banding", "OPT_ENABLE_BANDING"},
    {POSTFX_VECTOR_FIELD_DEBUG, "Vector Field Debug",
     "OPT_ENABLE_VECTOR_FIELD_DEBUG"},
    {POSTFX_STENCIL_DEBUG, "Stencil Debug View", "OPT_ENABLE_STENCIL_DEBUG"},
    {POSTFX_BLOOM_DEBUG, "Bloom Debug View", "OPT_ENABLE_BLOOM_DEBUG"},
    {POSTFX_FOG, "Atmospheric Fog", "OPT_ENABLE_FOG"},
    {POSTFX_FOG_DEBUG, "Fog Debug View", "OPT_ENABLE_FOG_DEBUG"},
    {POSTFX_LUT3D, "3D LUT Gamut Mapping", "OPT_ENABLE_LUT3D"},
};

#define EFFECT_COUNT (sizeof(ALL_EFFECTS) / sizeof(ALL_EFFECTS[0]))

static void log_optimized_effects(unsigned int flags)
{
	LOG_INFO("suckless-ogl.postprocess",
	         "Compiled OPTIMIZED shader with effects:");

	for (size_t i = 0; i < EFFECT_COUNT; i++) {
		if ((flags & (unsigned int)ALL_EFFECTS[i].flag) != 0) {
			LOG_INFO("suckless-ogl.postprocess", "  ✓ %s",
			         ALL_EFFECTS[i].name);
		}
	}

	LOG_INFO("suckless-ogl.postprocess",
	         "Shader optimization complete (Flags: 0x%08X)", flags);
}

static Shader* find_shader_in_cache(PostProcess* post_processing,
                                    unsigned int static_flags)
{
	for (int i = 0; i < post_processing->shaders.shader_cache_count; i++) {
		if (post_processing->shaders.shader_cache[i].flags ==
		    static_flags) {
			/* Found it! Move to front (LRU policy) */
			if (i > 0) {
				ShaderCacheEntry entry =
				    post_processing->shaders.shader_cache[i];

				/* Manual shift to avoid insecure memmove
				 * warnings */
				for (int j = i; j > 0; j--) {
					post_processing->shaders
					    .shader_cache[j] =
					    post_processing->shaders
					        .shader_cache[j - 1];
				}

				post_processing->shaders.shader_cache[0] =
				    entry;
			}
			return post_processing->shaders.shader_cache[0].shader;
		}
	}
	return NULL;
}

void postprocess_compile_optimized(PostProcess* post_processing,
                                   unsigned int static_flags)
{
	PROFILE_ZONE(ctx, "PostProcess Compile Optimized");
	/* Check cache first */
	Shader* cached = find_shader_in_cache(post_processing, static_flags);
	if (cached) {
		if (post_processing->shaders.postprocess_shader != cached) {
			pp_update_current_shader(post_processing, cached, true);
			LOG_INFO("suckless-ogl.postprocess",
			         "Using CACHED shader for flags 0x%08X",
			         static_flags);
		}
		post_processing->shaders.compiled_flags = static_flags;
		PROFILE_ZONE_END(ctx);
		return;
	}

	const char* defines[MAX_SHADER_DEFINES];
	int count = 0;
	char buffer[MAX_SHADER_DEFINES][MAX_DEFINE_LENGTH];

	for (size_t i = 0; i < EFFECT_COUNT; i++) {
		if (safe_snprintf(
		        buffer[count], sizeof(buffer[count]), "%s %d",
		        ALL_EFFECTS[i].define_name,
		        ((static_flags & (unsigned int)ALL_EFFECTS[i].flag) !=
		         0)) < 0) {
			LOG_ERROR("suckless-ogl.postprocess",
			          "Failed to format shader define");
			return;
		}
		defines[count] = buffer[count];
		count++;
	}

	Shader* new_shader = shader_load_with_defines(
	    "shaders/postprocess.vert", "shaders/postprocess.frag", defines,
	    count);

	if (new_shader) {
		new_shader->silent_warnings = true;
		pp_update_current_shader(post_processing, new_shader, true);
		post_processing->shaders.compiled_flags = static_flags;

		/* Add to cache */
		/* Move existing entries down to make room at index 0 */
		int move_count = post_processing->shaders.shader_cache_count;
		if (move_count == SHADER_CACHE_SIZE) {
			/* Cache full: Evict LRU (last entry) */
			shader_destroy(post_processing->shaders
			                   .shader_cache[SHADER_CACHE_SIZE - 1]
			                   .shader);
			move_count--; /* Only move first 31 items */
		} else {
			post_processing->shaders.shader_cache_count++;
		}

		if (move_count > 0) {
			/* Manual shift to avoid insecure memmove warnings */
			for (int j = move_count; j > 0; j--) {
				post_processing->shaders.shader_cache[j] =
				    post_processing->shaders
				        .shader_cache[j - 1];
			}
		}

		post_processing->shaders.shader_cache[0].flags = static_flags;
		post_processing->shaders.shader_cache[0].shader = new_shader;

		log_optimized_effects(static_flags);
	} else {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to compile optimized shader");
	}
	PROFILE_ZONE_END(ctx);
}

void postprocess_use_dynamic(PostProcess* post_processing)
{
	Shader* new_shader =
	    shader_load("shaders/postprocess.vert", "shaders/postprocess.frag");

	if (new_shader) {
		pp_update_current_shader(post_processing, new_shader, false);
		LOG_INFO("suckless-ogl.postprocess",
		         "Switched to DYNAMIC shader");
	} else {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to compile dynamic shader");
	}
}
