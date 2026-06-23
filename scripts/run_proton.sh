#!/usr/bin/env bash
# scripts/run_proton.sh
set -e

# Arguments passés par le Justfile
RELEASE_DIR="$1"
RELEASE_NAME="$2"
TEST_DIST_DIR="$3"
PROJECT_ROOT="$4"

EXTRACTED_APP="${TEST_DIST_DIR}/${RELEASE_NAME}/app.exe"
PROTON_PFX="${TEST_DIST_DIR}/proton_pfx"

echo "==> 1. Nettoyage et extraction de l'archive de release..."
rm -rf "${TEST_DIST_DIR}"
mkdir -p "${TEST_DIST_DIR}"
tar -I 'zstd' -xf "${RELEASE_DIR}/../${RELEASE_NAME}.tar.zst" -C "${TEST_DIST_DIR}"

# S'assurer que le dossier du préfixe Proton existe avant de lancer Proton (Flatpak ou Natif)
mkdir -p "${PROTON_PFX}"

echo "==> 2. Détection de l'environnement Steam..."

# Test A : Présence d'un environnement Flatpak
if command -v flatpak &>/dev/null && flatpak info com.valvesoftware.Steam &>/dev/null; then
    echo "    [i] Environnement détecté : Steam Flatpak (Sandbox)"
    STEAM_ROOT="${HOME}/.var/app/com.valvesoftware.Steam/.local/share/Steam"
    PROTON_PATH="${STEAM_ROOT}/steamapps/common/Proton - Experimental/proton"

    echo "==> 3. Lancement du moteur via Flatpak..."
    cd "${TEST_DIST_DIR}/${RELEASE_NAME}"
    flatpak run \
        --filesystem="${PROJECT_ROOT}" \
        --env=STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT}" \
        --env=STEAM_COMPAT_DATA_PATH="${PROTON_PFX}" \
        --command=python3 \
        com.valvesoftware.Steam \
        "${PROTON_PATH}" run "${EXTRACTED_APP}"

# Test B : Présence d'un environnement Natif (Bazzite, Fedora, Arch...)
elif [ -d "${HOME}/.local/share/Steam" ]; then
    echo "    [i] Environnement détecté : Steam Natif"
    STEAM_ROOT="${HOME}/.local/share/Steam"
    PROTON_PATH="${STEAM_ROOT}/steamapps/common/Proton - Experimental/proton"

    echo "==> 3. Lancement du moteur via Proton Natif..."

    cd "${TEST_DIST_DIR}/${RELEASE_NAME}"
    STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT}" \
        STEAM_COMPAT_DATA_PATH="${PROTON_PFX}" \
        "${PROTON_PATH}" run "${EXTRACTED_APP}"

else
    echo "    [x] Erreur : Aucune installation de Steam trouvée (ni Flatpak, ni Native)."
    exit 1
fi
