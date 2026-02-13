#include "tracy_ogl_bridge.h"

#include <stack>
#include <vector>

#ifdef TRACY_ENABLE

#include <glad/glad.h>

#include <tracy/TracyOpenGL.hpp>

static std::stack<tracy::GpuCtxScope*> s_gpu_zones;

extern "C" {

void TracyOGL_Init(void)
{
	// Initializes the GPU context.
	TracyGpuContext;
}

void TracyOGL_Destroy(void)
{
	while (!s_gpu_zones.empty()) {
		delete s_gpu_zones.top();
		s_gpu_zones.pop();
	}
}

void TracyOGL_Collect(void)
{
	TracyGpuCollect;
}

void TracyOGL_ZoneBegin(const TracySourceLocationData* srcloc)
{
	const auto* tracy_srcloc =
	    reinterpret_cast<const tracy::SourceLocationData*>(srcloc);
	s_gpu_zones.push(new tracy::GpuCtxScope(tracy_srcloc, true));
}

void TracyOGL_ZoneEnd(void)
{
	if (!s_gpu_zones.empty()) {
		delete s_gpu_zones.top();
		s_gpu_zones.pop();
	}
}

void Profiler_SendScreenCapture(int srcW, int srcH)
{
	// Cible : une image petite (ex: 320px de large max)
	float scale = 320.0f / (float)srcW;
	int dstW = 320;
	int dstH = (int)(srcH * scale);

	// 1. Créer/Récupérer un FBO pour le downscaling
	static GLuint downscaleFBO = 0;
	static GLuint downscaleTex = 0;

	if (downscaleFBO == 0) {
		glGenFramebuffers(1, &downscaleFBO);
		glGenTextures(1, &downscaleTex);
		glBindTexture(GL_TEXTURE_2D, downscaleTex);
		// Allocation de la texture réduite
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dstW, dstH, 0, GL_RGBA,
		             GL_UNSIGNED_BYTE, NULL);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, downscaleFBO);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
		                       GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
		                       downscaleTex, 0);
	}

	// 2. Blit (Redimensionnement GPU rapide)
	// On suppose que le FBO de lecture est déjà bindé (par défaut 0)
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, downscaleFBO);
	glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, dstW, dstH,
	                  GL_COLOR_BUFFER_BIT, GL_LINEAR);

	// 3. Lire les pixels réduits
	glBindFramebuffer(GL_READ_FRAMEBUFFER, downscaleFBO);

	static std::vector<char> screenBuffer;
	size_t bufferSize = dstW * dstH * 4;
	if (screenBuffer.size() != bufferSize) {
		screenBuffer.resize(bufferSize);
	}

	glReadPixels(0, 0, dstW, dstH, GL_RGBA, GL_UNSIGNED_BYTE,
	             screenBuffer.data());

	// 4. Remettre l'état d'origine
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// 5. Envoyer la petite image
	FrameImage(screenBuffer.data(), dstW, dstH, 0, true);
}

}  // extern "C"

#endif  // TRACY_ENABLE
