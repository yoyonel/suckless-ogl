#include "texture.h"

#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLuint texture_load_hdr(const char* path, int* width, int* height)
{
	int channels;
	float* data = stbi_loadf(path, width, height, &channels, 0);
	if (!data) {
		fprintf(stderr, "Failed to load HDR image: %s\n", path);
		return 0;
	}

	printf("HDR image: %dx%d, channels=%d\n", *width, *height, channels);

	/* Create OpenGL texture */
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, *width, *height, 0, GL_RGB,
	             GL_FLOAT, data);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "GL error after texture upload: 0x%x\n", err);
		stbi_image_free(data);
		glDeleteTextures(1, &tex);
		return 0;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(data);

	return tex;
}

GLuint texture_create_env_cubemap(int size)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

	/* Allocate storage for all 6 faces */
	for (int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F,
		             size, size, 0, GL_RGBA, GL_FLOAT, NULL);
	}

	/* Better filtering to reduce seams */
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
	                GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
	                GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
	                GL_CLAMP_TO_EDGE);

	/* Enable seamless cubemap filtering (OpenGL 3.2+) */
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	return tex;
}

GLuint texture_build_env_cubemap(GLuint hdr_texture, int size,
                                 GLuint compute_program)
{
	GLuint cubemap = texture_create_env_cubemap(size);
	if (!cubemap) {
		return 0;
	}

	/* Use compute shader to convert equirectangular to cubemap */
	glUseProgram(compute_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdr_texture);
	glUniform1i(glGetUniformLocation(compute_program, "equirectangularMap"),
	            0);

	glBindImageTexture(1, cubemap, 0, GL_TRUE, 0, GL_WRITE_ONLY,
	                   GL_RGBA16F);

	/* Dispatch compute shader */
	glDispatchCompute(size / 32, size / 32, 6);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "Compute shader error: 0x%x\n", err);
	}

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	/* Generate mipmaps */
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
	                GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
	                GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
	                GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
	                GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "Mipmap generation error: 0x%x\n", err);
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	return cubemap;
}
