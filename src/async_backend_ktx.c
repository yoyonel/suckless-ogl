#include "async_backend.h"
#include "async_loader.h"
#include "gl_common.h"
#include "log.h"
#include "utils.h"
#include <ktx.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT 0x8E8F
#endif

const double static OCTET_TO_MEGA = 1.0 / (1024.0 * 1024.0);

/* --- Helper : Gestion de la décompression --- */
static bool process_ktx_compression(ktxTexture2* ktx_tex, bool* out_is_bc6h)
{
	*out_is_bc6h = false;

	if (!ktx_tex->isCompressed) {
		return true; /* Déjà décompressé, rien à faire */
	}

	KTX_error_code res = KTX_SUCCESS;

	if (ktxTexture2_NeedsTranscoding(ktx_tex) ||
	    ktxTexture2_IsTranscodable(ktx_tex)) {
		if (ktxTexture2_IsHDR(ktx_tex)) {
			res = ktxTexture2_TranscodeBasis(ktx_tex, KTX_TTF_BC6HU,
			                                 0);
			if (res == KTX_SUCCESS) {
				*out_is_bc6h = true;
			}
		} else {
			res = ktxTexture2_TranscodeBasis(ktx_tex,
			                                 KTX_TTF_RGBA_HALF, 0);
		}
	} else {
		res = ktxTexture2_DecodeAstc(ktx_tex);
	}

	return (res == KTX_SUCCESS);
}

/* --- Helper : Audit --- */
static void log_ktx_audit(const char* path, ktxTexture2* ktx_tex, bool is_bc6h)
{
#ifndef NDEBUG
	LOG_INFO(
	    "suckless-ogl.async",
	    "[KTX Audit] %s | Dim: %ux%u | Zstd: %s | Hardware: %s | VRAM: "
	    "%.2f MB",
	    path, ktx_tex->baseWidth, ktx_tex->baseHeight,
	    ktxSupercompressionSchemeString(ktx_tex->supercompressionScheme),
	    is_bc6h ? "Oui (BC6H Native)" : "Non (Decompressed SFLOAT)",
	    (double)ktx_tex->dataSize * OCTET_TO_MEGA);
#else
	(void)path;
	(void)ktx_tex;
	(void)is_bc6h;
#endif
}

/* --- Helper : Résolution des Formats OpenGL --- */
static void configure_request_formats(AsyncRequest* req, ktxTexture2* ktx_tex,
                                      bool is_bc6h)
{
	if (is_bc6h) {
		req->is_compressed = true;
		req->channels = 3;
		req->gl_internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		req->gl_format = GL_RGB;
		req->gl_type = 0;
		return;
	}

	ktx_uint32_t num_comp = 0;
	ktx_uint32_t comp_bytes = 0;
	ktxTexture2_GetComponentInfo(ktx_tex, &num_comp, &comp_bytes);

	req->is_compressed = false;
	req->channels = (int)num_comp;
	req->gl_type = (comp_bytes == 2U) ? GL_HALF_FLOAT : GL_FLOAT;

	if (num_comp == 3U) {
		req->gl_format = GL_RGB;
		req->gl_internal_format =
		    (comp_bytes == 2U) ? GL_RGB16F : GL_RGB32F;
	} else {
		req->gl_format = GL_RGBA;
		req->gl_internal_format =
		    (comp_bytes == 2U) ? GL_RGBA16F : GL_RGBA32F;
	}
}

/* =========================================================================
 * Implémentation de l'Interface
 * ========================================================================= */

static bool ktx_backend_load(const char* path, AsyncRequest* req)
{
	ktxTexture2* ktx_tex = NULL;
	KTX_error_code res = ktxTexture2_CreateFromNamedFile(
	    path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_tex);

	if (res != KTX_SUCCESS) {
		return false;
	}

	bool transcoded_to_bc6h = false;
	if (!process_ktx_compression(ktx_tex, &transcoded_to_bc6h)) {
		ktxTexture_Destroy((ktxTexture*)ktx_tex);
		return false;
	}

	log_ktx_audit(path, ktx_tex, transcoded_to_bc6h);

	req->backend_data = ktx_tex;
	req->required_pbo_size =
	    ktxTexture_GetImageSize((ktxTexture*)ktx_tex, 0);
	req->width = (int)ktx_tex->baseWidth;
	req->height = (int)ktx_tex->baseHeight;

	configure_request_formats(req, ktx_tex, transcoded_to_bc6h);

	return true;
}

static void ktx_backend_convert(void* dst_ptr, AsyncRequest* req)
{
	ktxTexture2* ktx_tex = (ktxTexture2*)req->backend_data;
	size_t pbo_size = req->required_pbo_size;
	size_t offset = 0;

	ktxTexture_GetImageOffset((ktxTexture*)ktx_tex, 0, 0, 0, &offset);

	if (dst_ptr && ktx_tex) {
		uint8_t* ktx_data = ktxTexture_GetData((ktxTexture*)ktx_tex);
		if (ktx_data) {
			(void)safe_memcpy(dst_ptr, pbo_size, ktx_data + offset,
			                  pbo_size);
		} else {
			LOG_ERROR("suckless-ogl.async",
			          "Failed to get KTX data pointer.");
		}
	}

	if (ktx_tex) {
		ktxTexture_Destroy((ktxTexture*)ktx_tex);
	}
}

static void ktx_backend_cleanup(AsyncRequest* req)
{
	if (req->backend_data) {
		ktxTexture_Destroy((ktxTexture*)req->backend_data);
		req->backend_data = NULL;
	}
}

static const AsyncBackendInterface g_backend_ktx = {
    ktx_backend_load, ktx_backend_convert, ktx_backend_cleanup};

const AsyncBackendInterface* async_backend_ktx_get(void)
{
	return &g_backend_ktx;
}
