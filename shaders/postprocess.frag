#version 440 core
@header "common.glsl";

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(binding = 0) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D depthTexture;
layout(binding = 7) uniform usampler2D stencilTexture;

/* Includes for Post-Process Effects */
@header "postprocess/ubo.glsl";
@header "postprocess/defines.glsl";
@header "postprocess/bloom.glsl";
@header "postprocess/motion_blur.glsl";
@header "postprocess/chromatic_aberration.glsl";
@header "postprocess/dof.glsl";
@header "postprocess/exposure.glsl";
@header "postprocess/color_grading.glsl";
@header "postprocess/tonemap.glsl";
@header "postprocess/vignette.glsl";
@header "postprocess/grain.glsl";
@header "postprocess/fxaa.glsl";
@header "postprocess/banding.glsl";
@header "postprocess/fog.glsl";
@header "postprocess/lut3d.glsl";

/* ============================================================================
   MAIN PIPELINE
   ============================================================================
 */

void main()
{
	/* 1. Priority Debug Check for Motion Blur */
	if (enableMotionBlurDebug) {
		vec3 debugColor = applyMotionBlur(TexCoords);
		FragColor = vec4(debugColor, 1.0);
		return;
	}

	/* 1b. Priority Debug Check for Vector Field Overlay */
	if (enableVectorFieldDebug) {
		vec3 debugColor = applyMotionBlur(TexCoords);
		FragColor = vec4(debugColor, 1.0);
		return;
	}

	/* 1c. Priority Debug Check for Stencil Buffer */
	if (enableStencilDebug) {
		uint sVal = texture(stencilTexture, TexCoords).r;
		FragColor = vec4(vec3(sVal > 0u ? 1.0 : 0.0), 1.0);
		return;
	}

	/* 1d. Priority Debug Check for Bloom Stages */
	if (enableBloomDebug) {
		vec3 bloomCol = texture(bloomTexture, TexCoords).rgb;
		FragColor = vec4(bloomCol, 1.0);
		return;
	}

	vec3 color;
	/* Stencil Check: 0 = Skybox/Background, 1 = Object */
	uint stencil = texture(stencilTexture, TexCoords).r;

	bool isSkybox = (stencil == 0u);

	/* 2. Pipeline: Motion Blur -> Chromatic Aberration -> FXAA
	   Motion Blur is applied first, then CA, then FXAA on top.
	   This ensures MB is never bypassed by FXAA. */
	if (enableChromAbbr && !isSkybox) {
		/* CA samples "SceneSource" (which calls MB internally) */
		color = applyChromAbbr(TexCoords);
	} else {
		/* Direct fetch (or MB only) */
		color = getSceneSource(TexCoords);
	}

	if (enableFXAA) {
		if (isSkybox) {
			/* Skip FXAA on skybox to preserve star sharpness */
		} else {
			/* FXAA applied on top of the MB+CA result */
			color = applyFXAA(color, TexCoords);
		}
	}

	/* 3. Depth of Field */
	if (enableDoF) {
		/* applyDoF handles skybox check internally */
		vec3 dofColor = applyDoF(color, TexCoords);

		/* Check if it returned a debug visualization (assumed if
		   drastically different, but applyDoF returns valid color or
		   debug color. We just assign it.) */
		color = dofColor;
	}

	/* 4. Bloom */
	if (enableBloom) {
		color = applyBloom(color);
	}

	/* 4b. Atmospheric Fog (HDR space, before exposure/tonemapping) */
	if (enableFog) {
		color = applyFog(color, TexCoords);
	}

	if (enableFogDebug) {
		/* Show only the fog component (fog color * fog factor) on black
		 */
		color = applyFog(vec3(0.0), TexCoords);
	}

	/* 5. Exposure */
	float finalExposure = getCombinedExposure();
	color *= finalExposure;

	/* 6. Color Grading & White Balance */
	if (enableColorGrading) {
		color = apply_color_grading(color);
	}

	/* 6b. 3D LUT Gamut Mapping */
	if (enableLUT3D) {
		color = apply_lut3d(color);
	}

	/* 7. Tonemapping */
	color = unrealTonemap(color);

	/* 8. Vignette */
	if (enableVignette) {
		color = applyVignette(color, TexCoords);
	}

	/* 9. Gamma Correction */
	color = pow(color, vec3(1.0 / 2.2));

	/* 10. Banding/Quantization (applied in LDR space) */
	if (enableBanding) {
		color = applyBanding(color);
	}

	/* 11. Grain */
	if (enableGrain) {
		color = applyGrain(color, TexCoords);
	}

	FragColor = vec4(color, 1.0);
}
