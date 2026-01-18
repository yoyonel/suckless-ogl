#define DEFAULT_SAMPLES 4

enum {
	MIN_SUBDIV = 0,
	MAX_SUBDIV = 6,
	CUBEMAP_SIZE = 1024,
	INITIAL_SUBDIVISIONS = 3
};

static const float DEFAULT_CAMERA_DISTANCE = 20.0f;
static const float DEFAULT_CAMERA_YAW = -90.0f;  // regarder vers -Z
static const float DEFAULT_CAMERA_PITCH = 0.0f;  // regarder l'horizon
static const float DEFAULT_ENV_LOD = 0.0F;
static const float NEAR_PLANE = 0.1F;
static const float FAR_PLANE = 100.0F;
static const float FOV_ANGLE = 60.0F;
static const float MAX_ENV_LOD = 10.0F;
static const float MIN_ENV_LOD = 0.0F;
static const float LOD_STEP = 0.5F;
static const float MIN_CAMERA_DISTANCE = 1.5F;
static const float MAX_CAMERA_DISTANCE = 50.0F;
static const float ZOOM_STEP = 0.2F;
static const float LIGHT_DIR_X = 0.5F;
static const float LIGHT_DIR_Y = 1.0F;
static const float LIGHT_DIR_Z = 0.3F;
// PBR+IBL
static const int PREFILTERED_SPECULAR_MAP_SIZE = 1024;
static const int IRIDIANCE_MAP_SIZE = 64;
static const int BRDF_LUT_MAP_SIZE = 512;
static const float DEFAULT_CLAMP_MULTIPLIER = 6.0F;
static const float DEFAULT_METALLIC = 1.0F;
static const float DEFAULT_ROUGHNESS = 0.0F;
static const float DEFAULT_AO = 1.0F;
//
static const float DEFAULT_FONT_SIZE = 32.0F;
static const float DEFAULT_FPS_SMOOTHING = 0.95F;
static const float DEFAULT_FPS_WINDOW = 5.0F;
//
static const int DEFAULT_COLS = 10;
static const float DEFAULT_SPACING = 2.5F;
static const float HALF_OFFSET_MULTIPLIER = 0.5F;
//
static const float DEFAULT_FONT_SHADOW_OFFSET_X = 2.0F;
static const float DEFAULT_FONT_SHADOW_OFFSET_Y = 2.0F;
static const float DEFAULT_FONT_OFFSET_X = 0.0F;
static const float DEFAULT_FONT_OFFSET_Y = 0.0F;
static const vec3 DEFAULT_FONT_COLOR = {1.0F, 1.0F, 1.0F};
static const vec3 DEFAULT_FONT_SHADOW_COLOR = {0.0F, 0.0F, 0.0F};
static const int MAX_FPS_TEXT_LENGTH = 64;
// Postprocess
static const float DEFAULT_EXPOSURE_STEP = 0.1F;
static const float DEFAULT_MIN_EXPOSURE = 0.1F;
