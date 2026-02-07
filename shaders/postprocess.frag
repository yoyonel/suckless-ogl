#version 440 core
@header "common.glsl";

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform usampler2D stencilTexture;

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
		/* FXAA applied on top of the MB+CA result */
		color = applyFXAA(color, TexCoords);
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

	/* 5. Exposure */
	float finalExposure = getCombinedExposure();
	color *= finalExposure;

	/* 6. Color Grading & White Balance */
	if (enableColorGrading) {
		/* apply_color_grading handles WB internally in our new module
		 */
		color = apply_color_grading(color);
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
