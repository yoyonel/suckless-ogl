/**
 * @file async_backend.h
 * @brief Contrat d'implémentation pour les backends de décodage d'assets.
 */

#ifndef ASYNC_BACKEND_H
#define ASYNC_BACKEND_H

#include "async_loader.h"

enum { MSG_BUF_SIZE = 128 };

/**
 * @struct AsyncBackendInterface
 * @brief Table de pointeurs de fonctions définissant le cycle de vie d'un
 * chargement d'asset.
 */
typedef struct {
	bool (*load)(const char* path, AsyncRequest* req);
	void (*convert)(void* dst_ptr, AsyncRequest* req);
	void (*cleanup)(AsyncRequest* req);
} AsyncBackendInterface;

/** Récupère l'interface d'implémentation correspondant au type d'asset. */
const AsyncBackendInterface* async_backend_get(AssetType type);

/* --- Prototypes des points d'accès des modules métiers --- */
const AsyncBackendInterface* async_backend_stb_get(void);
const AsyncBackendInterface* async_backend_ktx_get(void);

#endif /* ASYNC_BACKEND_H */
