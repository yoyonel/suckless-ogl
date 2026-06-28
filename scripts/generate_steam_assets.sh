#!/usr/bin/env bash
set -euo pipefail

REF_IMAGE="docs/reference_image.png"
OUT_DIR="assets/steam_grid"

if [ ! -f "$REF_IMAGE" ]; then
    echo "❌ Erreur : L'image source '$REF_IMAGE' est introuvable."
    exit 1
fi

echo "→ Création du dossier cible $OUT_DIR..."
mkdir -p "$OUT_DIR"

# 1. Cover (600x900) : Redimensionne en couvrant toute la zone, puis coupe ce qui dépasse au centre
echo "→ Génération de cover.png (600x900)..."
convert "$REF_IMAGE" -resize 600x900^ -gravity center -extent 600x900 "$OUT_DIR/cover.png"

# 2. Hero (1920x620) : Redimensionnement panoramique
echo "→ Génération de hero.png (1920x620)..."
convert "$REF_IMAGE" -resize 1920x620^ -gravity center -extent 1920x620 "$OUT_DIR/hero.png"

# 3. Banner (460x215) : Format capsule
echo "→ Génération de banner.png (460x215)..."
convert "$REF_IMAGE" -resize 460x215^ -gravity center -extent 460x215 "$OUT_DIR/banner.png"

# 4. Logo : Génère une image transparente avec le texte "Suckless OGL" en blanc
echo "→ Génération de logo.png (Texte transparent)..."
convert -size 800x300 xc:transparent \
    -pointsize 72 -fill white -gravity center \
    -draw "text 0,0 'Suckless OGL'" \
    "$OUT_DIR/logo.png"

# 5. Icone (64x64) : Recadrage carré centré
echo "→ Génération de icon.png (64x64)..."
convert "$REF_IMAGE" -resize 64x64^ -gravity center -extent 64x64 "$OUT_DIR/../icon.png"

echo "✓ Toutes les ressources Steam ont été générées avec succès dans $OUT_DIR/"
