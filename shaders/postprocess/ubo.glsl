/* Centralized UBO for Post-Processing Settings */
layout(std140, binding = 0) uniform PostProcessBlock
{
	uint activeEffects;
	float time;
	vec2 screenTexelSize;

	/* Vignette (16 bytes) */
	float v_intensity;
	float v_smoothness;
	float v_roundness;
	float _pad1;

	/* Grain (32 bytes) */
	float g_intensity;
	float g_intensityShadows;
	float g_intensityMidtones;
	float g_intensityHighlights;
	float g_shadowsMax;
	float g_highlightsMin;
	float g_texelSize;
	float _pad2;

	/* Exposure (16 bytes) */
	float e_exposure;
	float _pad3_0;
	float _pad3_1;
	float _pad3_2;

	/* ChromAbbr (16 bytes) */
	float ca_strength;
	float _pad4_0;
	float _pad4_1;
	float _pad4_2;

	/* WhiteBalance (16 bytes) */
	float wb_temperature;
	float wb_tint;
	float _pad5_0;
	float _pad5_1;

	/* ColorGrading (32 bytes) */
	float cg_saturation;
	float cg_contrast;
	float cg_gamma;
	float cg_gain;
	float cg_offset;
	float cg_lift;
	float _pad6_0;
	float _pad6_1;

	/* Tonemap (32 bytes) */
	float tm_slope;
	float tm_toe;
	float tm_shoulder;
	float tm_blackClip;
	float tm_whiteClip;
	float _pad7_0;
	float _pad7_1;
	float _pad7_2;

	/* Bloom (16 bytes) */
	float b_intensity;
	float b_threshold;
	float b_softThreshold;
	float b_radius;

	/* DoF (16 bytes) */
	float d_focalDistance;
	float d_focalRange;
	float d_bokehScale;
	float d_anamorphicRatio;

	/* MotionBlur (16 bytes) */
	float mb_intensity;
	float mb_maxVelocity;
	int mb_samples;
	float _pad9;

	/* FXAA (16 bytes) */
	float fxaaQualitySubpix;
	float fxaaQualityEdgeThreshold;
	float fxaaQualityEdgeThresholdMin;
	float _pad10;

	/* Banding (32 bytes) */
	int bandingMode;
	float bandingLevels;
	float bandingDitherStrength;
	float bandingPerceptualGamma;
	vec3 bandingChannelLevels;
	float _pad11;

	/* Fog (32 bytes) */
	float fog_density;
	float fog_start;
	float fog_heightFalloff;
	float _pad12;
	vec3 fog_color;
	float _pad13;

	/* 3D LUT (16 bytes) */
	float lut3d_intensity;
	float _pad14[3];
};

/* Compatibility Helper Macros */
#ifdef OPT_ENABLE_VIGNETTE
const bool enableVignette = bool(OPT_ENABLE_VIGNETTE);
#else
#define enableVignette ((activeEffects & (1u << 0u)) != 0u)
#endif

#ifdef OPT_ENABLE_GRAIN
const bool enableGrain = bool(OPT_ENABLE_GRAIN);
#else
#define enableGrain ((activeEffects & (1u << 1u)) != 0u)
#endif

#ifdef OPT_ENABLE_EXPOSURE
const bool enableExposure = bool(OPT_ENABLE_EXPOSURE);
#else
#define enableExposure ((activeEffects & (1u << 2u)) != 0u)
#endif

#ifdef OPT_ENABLE_CHROM_ABBR
const bool enableChromAbbr = bool(OPT_ENABLE_CHROM_ABBR);
#else
#define enableChromAbbr ((activeEffects & (1u << 3u)) != 0u)
#endif

#ifdef OPT_ENABLE_BLOOM
const bool enableBloom = bool(OPT_ENABLE_BLOOM);
#else
#define enableBloom ((activeEffects & (1u << 4u)) != 0u)
#endif

#ifdef OPT_ENABLE_COLOR_GRADING
const bool enableColorGrading = bool(OPT_ENABLE_COLOR_GRADING);
#else
#define enableColorGrading ((activeEffects & (1u << 5u)) != 0u)
#endif

#ifdef OPT_ENABLE_DOF
const bool enableDoF = bool(OPT_ENABLE_DOF);
#else
#define enableDoF ((activeEffects & (1u << 6u)) != 0u)
#endif

#ifdef OPT_ENABLE_DOF_DEBUG
const bool enableDoFDebug = bool(OPT_ENABLE_DOF_DEBUG);
#else
#define enableDoFDebug ((activeEffects & (1u << 7u)) != 0u)
#endif

#ifdef OPT_ENABLE_AUTO_EXPOSURE
const bool enableAutoExposure = bool(OPT_ENABLE_AUTO_EXPOSURE);
#else
#define enableAutoExposure ((activeEffects & (1u << 8u)) != 0u)
#endif

#ifdef OPT_ENABLE_EXPOSURE_DEBUG
const bool enableExposureDebug = bool(OPT_ENABLE_EXPOSURE_DEBUG);
#else
#define enableExposureDebug ((activeEffects & (1u << 9u)) != 0u)
#endif

#ifdef OPT_ENABLE_MOTION_BLUR
const bool enableMotionBlur = bool(OPT_ENABLE_MOTION_BLUR);
#else
#define enableMotionBlur ((activeEffects & (1u << 10u)) != 0u)
#endif

#ifdef OPT_ENABLE_MOTION_BLUR_DEBUG
const bool enableMotionBlurDebug = bool(OPT_ENABLE_MOTION_BLUR_DEBUG);
#else
#define enableMotionBlurDebug ((activeEffects & (1u << 11u)) != 0u)
#endif

#ifdef OPT_ENABLE_FXAA
const bool enableFXAA = bool(OPT_ENABLE_FXAA);
#else
#define enableFXAA ((activeEffects & (1u << 12u)) != 0u)
#endif

#ifdef OPT_ENABLE_FXAA_DEBUG
const bool enableFXAADebug = bool(OPT_ENABLE_FXAA_DEBUG);
#else
#define enableFXAADebug ((activeEffects & (1u << 13u)) != 0u)
#endif

#ifdef OPT_ENABLE_BANDING
const bool enableBanding = bool(OPT_ENABLE_BANDING);
#else
#define enableBanding ((activeEffects & (1u << 14u)) != 0u)
#endif

#ifdef OPT_ENABLE_VECTOR_FIELD_DEBUG
const bool enableVectorFieldDebug = bool(OPT_ENABLE_VECTOR_FIELD_DEBUG);
#else
#define enableVectorFieldDebug ((activeEffects & (1u << 15u)) != 0u)
#endif

#ifdef OPT_ENABLE_STENCIL_DEBUG
const bool enableStencilDebug = bool(OPT_ENABLE_STENCIL_DEBUG);
#else
#define enableStencilDebug ((activeEffects & (1u << 16u)) != 0u)
#endif

#ifdef OPT_ENABLE_BLOOM_DEBUG
const bool enableBloomDebug = bool(OPT_ENABLE_BLOOM_DEBUG);
#else
#define enableBloomDebug ((activeEffects & (1u << 17u)) != 0u)
#endif

#ifdef OPT_ENABLE_FOG
const bool enableFog = bool(OPT_ENABLE_FOG);
#else
#define enableFog ((activeEffects & (1u << 18u)) != 0u)
#endif

#ifdef OPT_ENABLE_FOG_DEBUG
const bool enableFogDebug = bool(OPT_ENABLE_FOG_DEBUG);
#else
#define enableFogDebug ((activeEffects & (1u << 19u)) != 0u)
#endif

#ifdef OPT_ENABLE_LUT3D
const bool enableLUT3D = bool(OPT_ENABLE_LUT3D);
#else
#define enableLUT3D ((activeEffects & (1u << 20u)) != 0u)
#endif
