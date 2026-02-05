uniform sampler2D velocityTexture;
uniform sampler2D neighborMaxTexture;

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

	vec3 acc = centerColor;
	float totalWeight = 1.0;

	int samples = mb_samples;

	for (int i = 0; i < samples; ++i) {
		if (i == samples / 2)
			continue;  // Skip center

		float t = mix(-0.5, 0.5, (float(i) + noise) / float(samples));
		vec2 sampleUV = uv + velocity * t;

		/* Always sample RAW screen texture here.
		   (CA is applied *after* this function returns) */
		vec3 sampleColor = texture(screenTexture, sampleUV).rgb;

		/* Depth Weighting */
		float sampleDepth =
		    linearizeDepth(texture(depthTexture, sampleUV).r);
		float depthDiff = sampleDepth - centerDepth;
		float weight = 1.0;

		if (depthDiff > 1.0) {
			weight = 0.1;
		} else if (depthDiff < -1.0) {
			weight = 1.0;
		} else {
			weight = 1.0;
		}

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
