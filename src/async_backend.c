/**
 * @file async_backend.c
 * @brief Registre statique et aiguillage des backends de décodage.
 */

#include "async_backend.h"

#include <stddef.h>

const AsyncBackendInterface* async_backend_get(AssetType type)
{
	switch (type) {
		case ASSET_TYPE_TEXTURE_STB:
			return async_backend_stb_get();
		case ASSET_TYPE_TEXTURE_KTX:
			return async_backend_ktx_get();
		default:
			return NULL;
	}
}
