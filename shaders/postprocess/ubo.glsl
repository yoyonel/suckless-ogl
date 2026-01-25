/* Centralized UBO for Post-Processing Settings */
layout(std140, binding = 0) uniform PostProcessBlock
{
	uint activeEffects;
	float time;
	float _pad0_0;
	float _pad0_1;

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
	float _pad6_0;
	float _pad6_1;
	float _pad6_2;

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
	float _pad8;

	/* MotionBlur (16 bytes) */
	float mb_intensity;
	float mb_maxVelocity;
	int mb_samples;
	float _pad9;
};

/* Compatibility Helper Macros */
#define enableVignette ((activeEffects & (1u << 0u)) != 0u)
#define enableGrain ((activeEffects & (1u << 1u)) != 0u)
#define enableExposure ((activeEffects & (1u << 2u)) != 0u)
#define enableChromAbbr ((activeEffects & (1u << 3u)) != 0u)
#define enableBloom ((activeEffects & (1u << 4u)) != 0u)
#define enableColorGrading ((activeEffects & (1u << 5u)) != 0u)
#define enableDoF ((activeEffects & (1u << 6u)) != 0u)
#define enableDoFDebug ((activeEffects & (1u << 7u)) != 0u)
#define enableAutoExposure ((activeEffects & (1u << 8u)) != 0u)
#define enableExposureDebug ((activeEffects & (1u << 9u)) != 0u)
#define enableMotionBlur ((activeEffects & (1u << 10u)) != 0u)
#define enableMotionBlurDebug ((activeEffects & (1u << 11u)) != 0u)
