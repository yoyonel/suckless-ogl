#!/usr/bin/env bash
# Quitte immédiatement en cas d'erreur, de variable non définie, ou d'erreur dans un pipe
set -euo pipefail

# Paramètres d'entrée avec valeurs par défaut
VERSION="${1:-v0.1.0}"
BUILD_DIR="${2:-build-win}"
ZSTD_LEVEL="${3:-3}"

# Variables internes
RELEASE_BASE_DIR="build-release"
RELEASE_NAME="suckless-ogl-windows-${VERSION}"
RELEASE_DIR="${RELEASE_BASE_DIR}/${RELEASE_NAME}"
ARCHIVE_NAME="${RELEASE_NAME}.tar.zst"
ARCHIVE_PATH="${RELEASE_BASE_DIR}/${ARCHIVE_NAME}"

echo "==> Synchronisation de l'arborescence de release (${RELEASE_NAME})..."

# 1. Création de l'arborescence
mkdir -p "${RELEASE_DIR}/shaders" "${RELEASE_DIR}/assets"

# 2. Synchronisation intelligente (rsync)
rsync -a --update "${BUILD_DIR}/app.exe" "${RELEASE_DIR}/"
[ -d "shaders" ] && rsync -a --update shaders/ "${RELEASE_DIR}/shaders/"
[ -d "assets" ] && rsync -a --update assets/ "${RELEASE_DIR}/assets/"

# 3. Vérification des modifications
NEEDS_COMPRESSION=1

if [ -f "${ARCHIVE_PATH}" ]; then
    # find cherche un fichier plus récent que l'archive. 'head -n 1' optimise la recherche.
    NEWER_FILES=$(find "${RELEASE_DIR}" -newer "${ARCHIVE_PATH}" -type f 2>/dev/null | head -n 1)
    if [ -z "${NEWER_FILES}" ]; then
        NEEDS_COMPRESSION=0
    fi
fi

# 4. Compression (si nécessaire)
if [ "${NEEDS_COMPRESSION}" -eq 0 ]; then
    echo "==> Aucun fichier modifié. Recompression ignorée."
else
    echo "==> Génération de l'archive tar.zst (Niveau ${ZSTD_LEVEL}, optimisée rsync)..."
    cd "${RELEASE_BASE_DIR}"
    # On compresse vers un fichier temporaire (.tmp)
    tar -I "zstd -T0 -${ZSTD_LEVEL} --rsyncable" -cf "${ARCHIVE_NAME}.tmp" "${RELEASE_NAME}"
    # Si succès, on valide la transaction en le renommant (opération atomique)
    mv "${ARCHIVE_NAME}.tmp" "${ARCHIVE_NAME}"
    cd - > /dev/null
fi

# 5. Bilan
du -sh "${ARCHIVE_PATH}"
