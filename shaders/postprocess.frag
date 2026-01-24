#version 440 core
@header "common.glsl";

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;

/* Includes for Post-Process Effects */
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

/* ============================================================================
   MAIN PIPELINE
   ============================================================================
 */

void main()
{
	/* 1. Priority Debug Check for Motion Blur */
	if (enableMotionBlurDebug != 0) {
		vec3 debugColor = applyMotionBlur(TexCoords);
		FragColor = vec4(debugColor, 1.0);
		return;
	}

	vec3 color;

	/* Skybox Hack: Depth ~ 1.0 */
	float depth = texture(depthTexture, TexCoords).r;
	bool isSkybox = depth >= 0.99999;

	/* 2. Pipeline: Motion Blur -> Chromatic Aberration */
	if (enableChromAbbr != 0 && !isSkybox) {
		/* CA samples "SceneSource" (which calls MB) */
		color = applyChromAbbr(TexCoords);
	} else {
		/* Direct fetch (or MB only) */
		color = getSceneSource(TexCoords);
	}

	/* 3. Depth of Field */
	if (enableDoF != 0) {
		/* applyDoF handles skybox check internally */
		vec3 dofColor = applyDoF(color, TexCoords);

		/* Check if it returned a debug visualization (assumed if
		   drastically different, but applyDoF returns valid color or
		   debug color. We just assign it.) */
		color = dofColor;
	}

	/* 4. Bloom */
	if (enableBloom != 0) {
		color = applyBloom(color);
	}

	/* 5. Exposure */
	float finalExposure = getCombinedExposure();
	color *= finalExposure;

	/* 6. Color Grading & White Balance */
	if (enableColorGrading != 0) {
		/* apply_color_grading handles WB internally in our new module
		 */
		color = apply_color_grading(color);
	}
	/* Note: if color grading is disabled, we might skip WB?
	   Original code had separate check for WB but apply_color_grading
	   combined them. Let's check original logic: Original: if
	   (enableColorGrading != 0) applyWhiteBalance(color) if
	   (enableColorGrading != 0) apply_color_grading(color) So effectively
	   WB is only applied if ColorGrading is ON. My new module combines
	   them, so calling apply_color_grading is correct.
	*/

	/* 7. Tonemapping */
	color = unrealTonemap(color);

	/* 8. Vignette */
	if (enableVignette != 0) {
		color = applyVignette(color, TexCoords);
	}

	/* 9. Gamma Correction */
	color = pow(color, vec3(1.0 / 2.2));

	/* 10. Grain */
	if (enableGrain != 0) {
		color = applyGrain(color, TexCoords);
	}

	FragColor = vec4(color, 1.0);
}
