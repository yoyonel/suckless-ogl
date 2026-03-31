layout(binding = 4) uniform sampler2D velocityTexture;
layout(binding = 5) uniform sampler2D neighborMaxTexture;

vec3 applyVectorFieldDebug(vec2 uv);

/* ============================================================================
   EFFECT: MOTION BLUR
   ============================================================================
 */

/* Advanced Reconstruction using NeighborMax and Depth Weighting */
vec3 applyMotionBlur(vec2 uv)
{
	/* 1. Get Velocity at center pixel */
	vec2 velocity = texture(velocityTexture, uv).rg;

	/* Debug Visualization (Early Exit) */
	if (enableMotionBlurDebug) {
		/* Mode 2: RG Color Visualization */
		return vec3(abs(velocity.x) * 20.0, abs(velocity.y) * 20.0,
		            0.0);
	}

	/* Mode 3: Vector Field Overlay (toggleable via enableVectorFieldDebug)
	 */
	if (enableVectorFieldDebug) {
		return applyVectorFieldDebug(uv);
	}

	velocity *= mb_intensity;

	/* Clamp main velocity */
	float speed = length(velocity);
	if (speed > mb_maxVelocity) {
		velocity = normalize(velocity) * mb_maxVelocity;
		speed = mb_maxVelocity;
	}

	/* 2. Get Neighbor Max Velocity */
	vec2 maxNeighborVelocity =
	    texture(neighborMaxTexture, uv).rg * mb_intensity;
	float maxNeighborSpeed = length(maxNeighborVelocity);

	/* Fetch Center Color (Raw) */
	vec3 centerColor = texture(screenTexture, uv).rgb;

	/* Early exit if negligible motion */
	if (speed < 0.0001 && maxNeighborSpeed < 0.0001) {
		return centerColor;
	}

	/* Jitter */
	float noise = InterleavedGradientNoise(gl_FragCoord.xy);

	/* Center Depth */
	float centerDepth = linearizeDepth(texture(depthTexture, uv).r);

	/* Échantillonnage Adaptatif (Dynamic Sample Count)
	 * On limite le nombre de boucles en fonction de la vitesse (UV speed).
	 * speed_ratio = speed / mb_maxVelocity (0.0 à 1.0)
	 */
	float speed_ratio =
	    (mb_maxVelocity > 0.0) ? (speed / mb_maxVelocity) : 0.0;
	int actual_samples =
	    clamp(int(speed_ratio * float(mb_samples)), 2, mb_samples);

	vec3 acc = centerColor;
	float totalWeight = 1.0;

	for (int i = 0; i < actual_samples; ++i) {
		if (i == actual_samples / 2)
			continue;  // Skip center

		float t =
		    mix(-0.5, 0.5, (float(i) + noise) / float(actual_samples));
		vec2 sampleUV = uv + velocity * t;

		/* Always sample RAW screen texture here.
		   (CA is applied *after* this function returns) */
		vec3 sampleColor = texture(screenTexture, sampleUV).rgb;

		/* Soft Depth-Testing */
		float sampleDepth =
		    linearizeDepth(texture(depthTexture, sampleUV).r);
		float depthDiff = sampleDepth - centerDepth;

		/* Remplacement du if brut par un smoothstep.
		 * Si l'échantillon est derrière le pixel central (depthDiff >
		 * 0), le poids diminue progressivement de 1.0 vers 0.1 entre
		 * 0.5 et 2.0 unités.
		 */
		float weight = mix(1.0, 0.1, smoothstep(0.5, 2.0, depthDiff));

		acc += sampleColor * weight;
		totalWeight += weight;
	}

	return acc / totalWeight;
}

/* Wrapper to get "Scene Color" (Blurred or Raw) for CA to sample */
vec3 getSceneSource(vec2 uv)
{
	if (enableMotionBlur) {
		return applyMotionBlur(uv);
	}
	return texture(screenTexture, uv).rgb;
}

vec3 applyVectorFieldDebug(vec2 uv)
{
	vec2 screenSize = vec2(textureSize(velocityTexture, 0));
	vec2 pixelPos = uv * screenSize;

	/* Grid cell size (one arrow every N pixels) - LARGER for visibility */
	float gridSize = 48.0;
	vec2 cellCenter =
	    (floor(pixelPos / gridSize) * gridSize) + (gridSize * 0.5);
	vec2 uvCenter = cellCenter / screenSize;

	/* Sample velocity at the CENTER of the cell (not at current pixel) */
	vec2 velCenter = texture(velocityTexture, uvCenter).xy;

	/* Draw arrow if velocity is significant */
	if (length(velCenter) > 1e-4) {
		/* Direction and visual length */
		vec2 dir = normalize(velCenter);
		float len =
		    length(velCenter) * 800.0;   /* Increased visual scale */
		len = min(len, gridSize * 0.45); /* Clamp to stay in cell */

		/* Local position relative to cell center */
		vec2 localPos = pixelPos - cellCenter;

		/* SDF Point-to-Segment distance for symmetric line (-dir to
		 * +dir) */
		float h = clamp(dot(localPos, dir) / len, -1.0, 1.0);
		float d = length(localPos - dir * len * h);

		/* Line thickness (2.0 pixels for better visibility) */
		if (d < 2.0) {
			/* Color based on direction angle (HSV -> RGB) */
			float angle = atan(dir.y, dir.x); /* -PI to PI */
			float hue =
			    (angle + 3.14159) / 6.28318; /* Normalize to 0-1 */

			/* HSV to RGB (S=1, V=1) */
			vec3 rgb = clamp(
			    abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) -
			        3.0) -
			        1.0,
			    0.0, 1.0);
			return rgb;
		}
	}

	/* Darken the base scene to make arrows visible */
	vec3 baseColor = texture(screenTexture, uv).rgb;
	return baseColor * 0.3;
}
