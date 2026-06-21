#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/* Centralisation stricte des chemins racine */
#define ASSET_DIR_HDR "assets/textures/hdr"
// non utilisée, il y a des traces de `shaders/` un peu partout !
// principalement présents dans les calls de `shader_load(...)`,
// `shader_load_compute_program(...)`, `shader_load_with_defines(...)`
#define ASSET_DIR_SHADERS "shaders"
// n'existe pas
#define ASSET_DIR_MODELS "assets/models"

/* Enumération universelle des types de ressources pris en charge */
typedef enum {
	ASSET_TYPE_UNKNOWN = 0,
	ASSET_TYPE_TEXTURE_STB,
	ASSET_TYPE_TEXTURE_KTX
	/* Futur : ASSET_TYPE_MODEL_OBJ, etc. */
} AssetType;

/* Flags (Bitmask) pour catégoriser les capacités des formats */
typedef enum {
	ASSET_FLAG_NONE = 0,
	ASSET_FLAG_HDR_CAPABLE = (1U << 0U), /* Utilisable pour l'IBL/Skybox */
	ASSET_FLAG_COMPRESSED = (1U << 1U)   /* Supporte la compression GPU */
} AssetFlags;

/* Poignée standardisée circulant dans le moteur */
typedef struct {
	char full_path[512];
	char filename[256];
	AssetType type;
	uint32_t flags;
} AssetHandle;

/* API Publique */

/**
 * @brief Tente de résoudre un fichier en une ressource reconnue.
 * @return true si le format est supporté et le handle rempli, false sinon.
 */
bool asset_resolve_path(const char* base_dir, const char* filename,
                        AssetHandle* out_handle);

/**
 * @brief Vérifie si un fichier correspond à un flag spécifique (ex:
 * ASSET_FLAG_HDR_CAPABLE)
 */
bool asset_has_flag(const char* filename, uint32_t flag);

#endif /* ASSET_MANAGER_H */
