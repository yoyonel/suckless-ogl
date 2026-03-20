#include "gl_common.h"
#include "log.h"
#include "pbr.h"
#include "shader.h"
#include "texture.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRDF_LUT_SIZE 512
#define IRR_MAP_SIZE 64
#define SP_MAP_SIZE 1024U
#define PATH_BUFFER_SIZE 1024
#define MAX_LUM_GROUPS 65536

static void save_hdr_texture(const char* path, GLuint tex, int width,
                             int height, int src_channels, int mip)
{
	float* src_data = malloc((size_t)width * (size_t)height *
	                         (size_t)src_channels * sizeof(float));
	if (!src_data) {
		return;
	}

	glBindTexture(GL_TEXTURE_2D, tex);
	GLenum format = (src_channels == 4) ? GL_RGBA : GL_RG;
	glGetTexImage(GL_TEXTURE_2D, mip, format, GL_FLOAT, src_data);

	// Always export as 3-channel RGB to match Vulkan expectations
	float* dst_data =
	    malloc((size_t)width * (size_t)height * 3 * sizeof(float));
	if (dst_data) {
		for (int i = 0; i < width * height; ++i) {
			dst_data[i * 3 + 0] = src_data[i * src_channels + 0];
			dst_data[i * 3 + 1] =
			    (src_channels >= 2) ? src_data[i * src_channels + 1]
			                        : 0.0F;
			dst_data[i * 3 + 2] =
			    (src_channels >= 3) ? src_data[i * src_channels + 2]
			                        : 0.0F;
		}
		if (stbi_write_hdr(path, width, height, 3, dst_data) == 0) {
			fprintf(stderr, "Failed to write HDR: %s\n", path);
		}
		free(dst_data);
	}
	free(src_data);
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <input.hdr> <output_dir>\n",
		        argv[0]);
		return EXIT_FAILURE;
	}
	const char* input_path = argv[1];
	const char* output_dir = argv[2];

	log_set_level(LOG_LEVEL_INFO);

	if (glfwInit() == 0) {
		fprintf(stderr, "Failed to init GLFW\n");
		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window =
	    glfwCreateWindow(1, 1, "IBL Extract Tool", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Failed to create GL context\n");
		glfwTerminate();
		return EXIT_FAILURE;
	}
	glfwMakeContextCurrent(window);

	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		fprintf(stderr, "Failed to init GLAD\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	int img_width = 0;
	int img_height = 0;
	int img_ch = 0;
	float* pixels =
	    texture_load_pixels(input_path, &img_width, &img_height, &img_ch);
	if (!pixels) {
		fprintf(stderr, "Failed to load input HDR: %s\n", input_path);
		glfwDestroyWindow(window);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	GLuint env_tex;
	glGenTextures(1, &env_tex);
	glBindTexture(GL_TEXTURE_2D, env_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, img_width, img_height, 0,
	             GL_RGBA, GL_FLOAT, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(pixels);

	// Compute Mean Luminance for thresholding (matching Vulkan logic)
	GLuint lum_p1 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass1.glsl");
	GLuint lum_p2 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass2.glsl");
	if (lum_p1 == 0 || lum_p2 == 0) {
		fprintf(stderr, "Failed to load luminance shaders\n");
		return EXIT_FAILURE;
	}

	PBRLumUniforms lum_uni;
	pbr_get_lum_uniforms(lum_p2, &lum_uni);

	GLuint lum_ssbos[2];
	glGenBuffers(2, lum_ssbos);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lum_ssbos[0]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_LUM_GROUPS * sizeof(float),
	             NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lum_ssbos[1]);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float), NULL,
	             GL_STATIC_DRAW);

	compute_mean_luminance_gpu_start(lum_p1, lum_p2, env_tex, img_width,
	                                 img_height, lum_ssbos, &lum_uni);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
	float mean_lum = compute_mean_luminance_gpu_result(lum_ssbos, 1.0F);
	float threshold = mean_lum * 3.0F;
	printf("Computed mean luminance: %.4f (threshold: %.4f)\n", mean_lum,
	       threshold);

	char path[PATH_BUFFER_SIZE];

	// 1. BRDF LUT
	GLuint brdf_tex = build_brdf_lut_map(BRDF_LUT_SIZE);
	if (snprintf(path, sizeof(path), "%s/brdf_lut.hdr", output_dir) >=
	    (int)sizeof(path)) {
		fprintf(stderr, "Path too long\n");
	}
	save_hdr_texture(path, brdf_tex, BRDF_LUT_SIZE, BRDF_LUT_SIZE, 2, 0);

	// 2. Irradiance Map
	GLuint irr_shader = shader_load_compute("shaders/IBL/irmap.glsl");
	if (irr_shader == 0) {
		fprintf(stderr, "Failed to load irradiance shader\n");
		return EXIT_FAILURE;
	}
	GLuint irr_tex =
	    build_irradiance_map(irr_shader, env_tex, IRR_MAP_SIZE, threshold);
	if (snprintf(path, sizeof(path), "%s/irradiance.hdr", output_dir) >=
	    (int)sizeof(path)) {
		fprintf(stderr, "Path too long\n");
	}
	save_hdr_texture(path, irr_tex, IRR_MAP_SIZE, IRR_MAP_SIZE, 4, 0);

	// 3. Prefiltered Specular Map
	GLuint sp_shader = shader_load_compute("shaders/IBL/spmap.glsl");
	if (sp_shader == 0) {
		fprintf(stderr, "Failed to load specular shader\n");
		return EXIT_FAILURE;
	}

	GLuint sp_tex = build_prefiltered_specular_map(
	    sp_shader, env_tex, (int)SP_MAP_SIZE, (int)SP_MAP_SIZE, threshold);

	int mips[] = {0, 1, 5, 10};
	for (int i = 0; i < 4; i++) {
		int mip = mips[i];
		int mip_width = (int)SP_MAP_SIZE >> mip;
		if (mip_width < 1) {
			mip_width = 1;
		}
		if (snprintf(path, sizeof(path), "%s/prefiltered_mip%d.hdr",
		             output_dir, mip) >= (int)sizeof(path)) {
			fprintf(stderr, "Path too long\n");
		}
		save_hdr_texture(path, sp_tex, mip_width, mip_width, 4, mip);
	}

	printf("IBL Maps exported successfully to %s\n", output_dir);

	glDeleteBuffers(2, lum_ssbos);
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}
