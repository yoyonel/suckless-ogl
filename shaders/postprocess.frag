#version 440 core
@header "common.glsl";

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;

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

	vec3 color;

	/* Skybox Hack: Depth ~ 1.0 */
	float depth = texture(depthTexture, TexCoords).r;
	bool isSkybox = depth >= 0.99999;

	// --------------------------------------------------------------------------------
	// FIXME: ça semble casser les performances de rajouter un if else pour
	// le FXAA 	c'est l'effet/impact direct des branchements
	// conditionnels dans les shaders !
	// --------------------------------------------------------------------------------
	// if (enableFXAA) {
	// 	// FXAA on the HDR source
	// 	// Fix: Pass the actual center pixel color, otherwise early exit
	// 	// returns black!
	// 	vec3 centerColor = texture(screenTexture, TexCoords).rgb;
	// 	color = applyFXAA(centerColor, TexCoords);
	// }
	// /* 2. Pipeline: Motion Blur -> Chromatic Aberration */
	// else
	if (enableChromAbbr && !isSkybox) {
		/* CA samples "SceneSource" (which calls MB) */
		color = applyChromAbbr(TexCoords);
	} else {
		/* Direct fetch (or MB only) */
		color = getSceneSource(TexCoords);
	}
	// --------------------------------------------------------------------------------
	// FIXME: en effet en mettant le FXAA ici, ça limite les impacts de
	// performance.
	// TODO: faire des shaders dédiés pour chaque combinaison d'effet
	// post-process
	color = applyFXAA(color, TexCoords);

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

	/* 10. Grain */
	if (enableGrain) {
		color = applyGrain(color, TexCoords);
	}

	FragColor = vec4(color, 1.0);
}
