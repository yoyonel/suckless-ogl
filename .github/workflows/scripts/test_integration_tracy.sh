#!/usr/bin/env bash

# Arrête le script à la moindre erreur, variable non définie, ou erreur dans un pipe
set -euo pipefail

echo "⚙️  [1/3] Compilation du moteur (Mode Tracy)..."
cmake -B build-tracy -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TRACY=ON -DENABLE_NATIVE_ARCH=OFF >/dev/null
cmake --build build-tracy --parallel "$(nproc)" >/dev/null

export TRACY_PORT=8086
TRACE_OUTPUT="ci-trace.tracy"

echo "🚀 [2/3] Lancement de l'application en arrière-plan (Xvfb)..."
xvfb-run -a -s "-screen 0 1920x1080x24" ./build-tracy/app &
APP_PID=$!

# Nettoyage garanti : Tue l'application quand le script se termine (succès ou échec)
trap 'echo "🧹 Nettoyage du processus moteur ($APP_PID)..."; kill -9 $APP_PID 2>/dev/null || true' EXIT

# On laisse le temps au thread Tracy de démarrer et au moteur d'afficher quelques frames
sleep 2

echo "📡 [3/3] Démarrage de la capture réseau sur le port $TRACY_PORT (3 secondes)..."
# Appel direct à l'exécutable global présent dans le conteneur CI
tracy-capture -a 127.0.0.1 -p $TRACY_PORT -o "$TRACE_OUTPUT" -s 3

echo "📊 Validation basique de l'artefact..."
if [ ! -s "$TRACE_OUTPUT" ]; then
    echo "❌ Échec : Le fichier $TRACE_OUTPUT est introuvable ou vide."
    exit 1
fi

echo "🖨️ Extraction des données avec tracy-csvexport..."
# C'est cette ligne critique qui manquait !
tracy-csvexport "$TRACE_OUTPUT" >trace_stats.csv

echo "🔍 Analyse sémantique de la trace..."
# Appel de ton nouveau script Python externe
python3 .github/workflows/scripts/analyze_tracy_traces.py
