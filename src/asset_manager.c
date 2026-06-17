#include "asset_manager.h"

#include "utils.h"
#include <string.h>

/* --- LE REGISTRE (Data-Oriented Pattern) --- */
typedef struct {
	const char* extension;
	AssetType type;
	uint32_t flags;
} AssetRegistryEntry;

/* Pour ajouter un format, il suffit d'ajouter une ligne ici.
 * Aucune autre logique (if/else) n'est requise ailleurs. */
static const AssetRegistryEntry ASSET_REGISTRY[] = {
    {".hdr", ASSET_TYPE_TEXTURE_STB, ASSET_FLAG_HDR_CAPABLE},
    {".ktx", ASSET_TYPE_TEXTURE_KTX,
     (uint32_t)ASSET_FLAG_HDR_CAPABLE | (uint32_t)ASSET_FLAG_COMPRESSED},
    {".ktx2", ASSET_TYPE_TEXTURE_KTX,
     (uint32_t)ASSET_FLAG_HDR_CAPABLE | (uint32_t)ASSET_FLAG_COMPRESSED},
    {".png", ASSET_TYPE_TEXTURE_STB, ASSET_FLAG_NONE},
    {".jpg", ASSET_TYPE_TEXTURE_STB, ASSET_FLAG_NONE}};

static const size_t REGISTRY_COUNT =
    sizeof(ASSET_REGISTRY) / sizeof(ASSET_REGISTRY[0]);

static const AssetRegistryEntry* find_registry_entry(const char* filename)
{
	const char* ext = strrchr(filename, '.');
	if (!ext) {
		return NULL;
	}

	for (size_t i = 0; i < REGISTRY_COUNT; i++) {
		/* Comparaison insensible à la casse pour les extensions (.KTX2
		 * == .ktx2) */
		if (strcasecmp(ext, ASSET_REGISTRY[i].extension) == 0) {
			return &ASSET_REGISTRY[i];
		}
	}
	return NULL;
}

bool asset_resolve_path(const char* base_dir, const char* filename,
                        AssetHandle* out_handle)
{
	if (!base_dir || !filename || !out_handle) {
		return false;
	}

	const AssetRegistryEntry* entry = find_registry_entry(filename);
	if (!entry) {
		out_handle->type = ASSET_TYPE_UNKNOWN;
		out_handle->flags = ASSET_FLAG_NONE;
		return false;
	}

	out_handle->type = entry->type;
	out_handle->flags = entry->flags;
	(void)safe_snprintf(out_handle->filename, sizeof(out_handle->filename),
	                    "%s", filename);
	(void)safe_snprintf(out_handle->full_path,
	                    sizeof(out_handle->full_path), "%s/%s", base_dir,
	                    filename);

	return true;
}

bool asset_has_flag(const char* filename, uint32_t flag)
{
	const AssetRegistryEntry* entry = find_registry_entry(filename);
	if (!entry) {
		return false;
	}
	return (entry->flags & flag) == flag;
}
