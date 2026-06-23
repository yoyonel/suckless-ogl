#!/usr/bin/env bash

# Arrête le script à la moindre erreur, variable non définie, ou erreur dans un pipe
set -euo pipefail

if command -v tracy-capture >/dev/null 2>&1 && command -v tracy-csvexport >/dev/null 2>&1; then
    echo "✅ Outils Tracy CLI trouvés dans le PATH."
    TRACY_CAPTURE="tracy-capture"
    TRACY_CSVEXPORT="tracy-csvexport"
else
    echo "⚙️  [0/3] Compilation locale des outils Tracy CLI (Capture & CSV Export)..."
    cmake -B deps/tracy/capture/build -S deps/tracy/capture -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build deps/tracy/capture/build --parallel "$(nproc)" >/dev/null
    TRACY_CAPTURE="./deps/tracy/capture/build/tracy-capture"

    cmake -B deps/tracy/csvexport/build -S deps/tracy/csvexport -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build deps/tracy/csvexport/build --parallel "$(nproc)" >/dev/null
    TRACY_CSVEXPORT="./deps/tracy/csvexport/build/tracy-csvexport"
fi

echo "⚙️  [1/3] Compilation du moteur (Mode Tracy)..."
cmake -B build-tracy -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TRACY=ON -DENABLE_NATIVE_ARCH=OFF >/dev/null
cmake --build build-tracy --parallel "$(nproc)" >/dev/null

export TRACY_PORT=8086
TRACE_OUTPUT="ci-trace.tracy"

echo "🚀 [2/3] Lancement de l'application en arrière-plan (Xvfb)..."
xvfb-run -a -s "-screen 0 1920x1080x24" ./build-tracy/app &
APP_PID=$!

# Nettoyage garanti : Tue l'application et les processus enfants de xvfb-run (comme Xvfb et l'app) quand le script se termine
trap 'echo "🧹 Nettoyage du processus moteur ($APP_PID)..."; pkill -9 -P $APP_PID 2>/dev/null || true; kill -9 $APP_PID 2>/dev/null || true' EXIT

# On laisse le temps au thread Tracy de démarrer et au moteur d'afficher quelques frames.
# Sous Mesa llvmpipe (headless), l'initialisation et la compilation des shaders
# peuvent prendre plus de 2 secondes. On augmente donc l'attente.
sleep 7

echo "📡 [3/3] Démarrage de la capture réseau sur le port $TRACY_PORT (5 secondes)..."
# Appel à l'exécutable local ou global, avec -f pour écraser la trace existante si nécessaire
$TRACY_CAPTURE -a 127.0.0.1 -p $TRACY_PORT -o "$TRACE_OUTPUT" -s 5 -f

echo "📊 Validation basique de l'artefact..."
if [ ! -s "$TRACE_OUTPUT" ]; then
    echo "❌ Échec : Le fichier $TRACE_OUTPUT est introuvable ou vide."
    exit 1
fi

echo "🖨️ Extraction des données avec tracy-csvexport..."
# C'est cette ligne critique qui manquait !
$TRACY_CSVEXPORT "$TRACE_OUTPUT" >trace_stats.csv

echo "🔍 Analyse sémantique de la trace..."
# Appel de ton nouveau script Python externe
python3 .github/workflows/scripts/analyze_tracy_traces.py
