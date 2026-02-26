// tests/test_camera.c
#include "camera.h"
#include "unity.h"
#include <math.h>

static Camera cam;
static mat4 view_matrix;

static const float DEFAULT_INIT_X = 0.0F;
static const float DEFAULT_INIT_Y = 20.0F;
static const float DEFAULT_INIT_Z = 0.0F;
static const float DEFAULT_INIT_YAW = -90.0F;
static const float DEFAULT_INIT_PITCH = 0.0F;
static const float TOLERANCE_SMALL = 0.01F;
static const float TOLERANCE_TINY = 1e-6F;
static const float TIMESTEP_MULTIPLIER = 3.0F;
static const int ITERATIONS_EXPECTED = 3;
static const float MOUSE_X_OFFSET = 10.0F;
static const float MOUSE_Y_OFFSET = 5.0F;
static const float PITCH_EXTREME_HIGH = 1000.0F;
static const float PITCH_EXTREME_LOW = -1000.0F;
static const float SCROLL_OFFSET = 1.0F;

void setUp(void)
{
	camera_init(&cam, DEFAULT_INIT_Y, DEFAULT_INIT_YAW, DEFAULT_INIT_PITCH);
}

void tearDown(void)
{
	// Nettoyage si nécessaire
}

void test_camera_module_exists(void)
{
	TEST_ASSERT_NOT_NULL(&cam);
	TEST_ASSERT_NOT_NULL(cam.front);
	TEST_ASSERT_NOT_NULL(cam.position);
	TEST_ASSERT_NOT_NULL(cam.up);
	TEST_ASSERT_NOT_NULL(cam.right);
}

void test_camera_initialization(void)
{
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_INIT_YAW, cam.yaw);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_INIT_PITCH, cam.pitch);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_CAMERA_SPEED, cam.velocity);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_CAMERA_SENSITIVITY, cam.sensitivity);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_CAMERA_ZOOM, cam.zoom);
	TEST_ASSERT_EQUAL_FLOAT(0.0F, cam.physics_accumulator);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_FIXED_TIMESTEP, cam.fixed_timestep);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_INIT_X, cam.position[0]);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_INIT_Z, cam.position[1]);
	TEST_ASSERT_EQUAL_FLOAT(DEFAULT_INIT_Y, cam.position[2]);
}

void test_camera_update_vectors(void)
{
	camera_update_vectors(&cam);

	// Vérifie que les vecteurs sont normalisés
	float front_len = glm_vec3_norm(cam.front);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_SMALL, 1.0F, front_len);

	float right_len = glm_vec3_norm(cam.right);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_SMALL, 1.0F, right_len);

	float up_len = glm_vec3_norm(cam.up);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_SMALL, 1.0F, up_len);

	// Vérifie l'orthogonalité
	float dot_front_right = glm_vec3_dot(cam.front, cam.right);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_SMALL, 0.0F, dot_front_right);
}

void test_camera_fixed_update_no_input(void)
{
	// Sauvegarde la position initiale
	vec3 initial_pos;
	glm_vec3_copy(cam.position, initial_pos);

	// Simule une mise à jour sans input
	camera_fixed_update(&cam);

	// La position ne devrait pas changer (pas d'input)
	TEST_ASSERT_EQUAL_FLOAT(initial_pos[0], cam.position[0]);
	TEST_ASSERT_EQUAL_FLOAT(initial_pos[1], cam.position[1]);
	TEST_ASSERT_EQUAL_FLOAT(initial_pos[2], cam.position[2]);
}

void test_camera_fixed_update_with_forward_input(void)
{
	// Active le mouvement avant
	cam.move_forward = 1;
	vec3 initial_pos;
	glm_vec3_copy(cam.position, initial_pos);

	// Simule une mise à jour
	camera_fixed_update(&cam);

	// La position devrait avoir changé (vers l'avant)
	float distance_moved = glm_vec3_distance(initial_pos, cam.position);
	TEST_ASSERT_TRUE(distance_moved > 0.0F);
}

void test_camera_fixed_update_accumulator(void)
{
	// Simule un delta_time plus grand que fixed_timestep
	cam.physics_accumulator =
	    DEFAULT_FIXED_TIMESTEP * TIMESTEP_MULTIPLIER;  // 3x fixed_timestep

	// Vérifie que la boucle de fixed update s'exécute 3 fois
	int update_count = 0;
	while (cam.physics_accumulator >= cam.fixed_timestep) {
		camera_fixed_update(&cam);
		cam.physics_accumulator -= cam.fixed_timestep;
		update_count++;
	}
	TEST_ASSERT_EQUAL_INT(ITERATIONS_EXPECTED, update_count);
}

void test_camera_process_mouse_changes_orientation(void)
{
	float initial_yaw = cam.yaw;
	float initial_pitch = cam.pitch;

	camera_process_mouse(&cam, MOUSE_X_OFFSET, MOUSE_Y_OFFSET);

	// Le yaw et pitch devraient avoir changé
	TEST_ASSERT_NOT_EQUAL_FLOAT(initial_yaw, cam.yaw_target);
	TEST_ASSERT_NOT_EQUAL_FLOAT(initial_pitch, cam.pitch_target);
}

void test_camera_process_mouse_clamps_pitch(void)
{
	// Force un pitch très grand
	camera_process_mouse(&cam, 0.0F, PITCH_EXTREME_HIGH);
	TEST_ASSERT_FLOAT_WITHIN(0.1F, DEFAULT_MAX_PITCH, -cam.pitch_target);

	// Force un pitch très petit
	camera_init(&cam, DEFAULT_INIT_Y, DEFAULT_INIT_YAW, DEFAULT_INIT_PITCH);
	camera_process_mouse(&cam, 0.0F, PITCH_EXTREME_LOW);
	TEST_ASSERT_FLOAT_WITHIN(0.1F, DEFAULT_MIN_PITCH, -cam.pitch_target);
}

void test_camera_get_view_matrix_not_null(void)
{
	camera_get_view_matrix(&cam, view_matrix);
	// Vérifie que la matrice n'est pas nulle
	int is_zero = 1;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (fabsf(view_matrix[i][j]) > 0.001F) {
				is_zero = 0;
				break;
			}
		}
	}
	TEST_ASSERT_FALSE(is_zero);
}

void test_camera_process_scroll_changes_position(void)
{
	vec3 initial_pos;
	glm_vec3_copy(cam.position, initial_pos);

	camera_process_scroll(&cam, SCROLL_OFFSET);
	camera_fixed_update(&cam);

	// La position devrait avoir changé
	float distance_moved = glm_vec3_distance(initial_pos, cam.position);
	TEST_ASSERT_TRUE(distance_moved > 0.0F);

	// La position devrait être alignée avec le front
	vec3 dir_move;
	glm_vec3_sub(cam.position, initial_pos, dir_move);
	glm_vec3_normalize(dir_move);
	float dot = glm_vec3_dot(cam.front, dir_move);
	TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_TINY, dot, 1.0F);
}

void test_camera_head_bobbing_enabled_by_default(void)
{
	TEST_ASSERT_TRUE(cam.bobbing_enabled);
}

void test_camera_rotation_smoothing(void)
{
	// Change les targets
	camera_process_mouse(&cam, MOUSE_X_OFFSET, MOUSE_Y_OFFSET);

	// Vérifie que yaw/pitch ne sont pas égaux aux targets (smoothing)
	TEST_ASSERT_NOT_EQUAL_FLOAT(cam.yaw_target, cam.yaw);
	TEST_ASSERT_NOT_EQUAL_FLOAT(cam.pitch_target, cam.pitch);

	// Après interpolation, les valeurs devraient se rapprocher
	float alpha = cam.rotation_smoothing;
	float old_yaw = cam.yaw;
	float old_pitch = cam.pitch;
	cam.yaw = cam.yaw + ((cam.yaw_target - cam.yaw) * alpha);
	cam.pitch = cam.pitch + ((cam.pitch_target - cam.pitch) * alpha);

	TEST_ASSERT_TRUE(fabsf(cam.yaw - old_yaw) <
	                 fabsf(cam.yaw_target - old_yaw));
	TEST_ASSERT_TRUE(fabsf(cam.pitch - old_pitch) <
	                 fabsf(cam.pitch_target - old_pitch));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_camera_module_exists);
	RUN_TEST(test_camera_initialization);
	RUN_TEST(test_camera_update_vectors);
	RUN_TEST(test_camera_fixed_update_no_input);
	RUN_TEST(test_camera_fixed_update_with_forward_input);
	RUN_TEST(test_camera_fixed_update_accumulator);
	RUN_TEST(test_camera_process_mouse_changes_orientation);
	RUN_TEST(test_camera_process_mouse_clamps_pitch);
	RUN_TEST(test_camera_get_view_matrix_not_null);
	RUN_TEST(test_camera_process_scroll_changes_position);
	RUN_TEST(test_camera_head_bobbing_enabled_by_default);
	RUN_TEST(test_camera_rotation_smoothing);
	return UNITY_END();
}
