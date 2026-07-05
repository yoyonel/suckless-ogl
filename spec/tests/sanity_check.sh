echo "=== AUDIT DÉTERMINISTE : CONSTITUTION vs BASE DE CODE ==="

echo -e "\n[Test 1] Isolation de postprocess.h dans les effets (Interdiction formelle) :"
# Vérifie qu'aucun fichier d'effet ne dépend du God Object
if grep -rn "postprocess\.h" src/fx_*.c 2>/dev/null; then
    echo "❌ ÉCHEC : Violation de l'isolation des effets trouvée ci-dessus."
else
    echo "✔ PASS : Zéro inclusion de postprocess.h dans les effets."
fi

echo -e "\n[Test 2] Limite des 500 LOC par module [MVP/TRANSITION] :"
# Affiche les fichiers .c qui dépassent la barre des 500 lignes
OVER_500=$(find src/ -name "*.c" -exec wc -l {} + | awk '$1 > 500 && $2 != "total" {print $2 " (" $1 " LOC)"}')
if [ -n "$OVER_500" ]; then
    echo "⚠️  WARNING (Candidats au refactoring) :"
    echo "$OVER_500"
else
    echo "✔ PASS : Tous les modules sont < 500 LOC."
fi

echo -e "\n[Test 3] Fuites d'en-têtes système (Isolation PAL) :"
# Cherche des inclusions OS sauvages hors du module PAL / platform
LEAKS=$(grep -rnE "#include <(windows|dirent|unistd|pthread|sys/).*>" src/ include/ | grep -vE "(pal|platform|xvfb)")
if [ -n "$LEAKS" ]; then
    echo "❌ ÉCHEC : Appels OS non abstraits détectés :"
    echo "$LEAKS"
else
    echo "✔ PASS : Étanchéité du PAL respectée."
fi

echo -e "\n[Test 4] Suffixage des descripteurs de sous-systèmes (_subsys_init) :"
# Vérifie que les fonctions du cycle de vie respectent la convention
COUNT_INIT=$(grep -rn "_subsys_init(" src/ --include="*.c" | wc -l)
echo "ℹ️  Nombre de modules implémentant _subsys_init : $COUNT_INIT"

echo -e "\n[Test 5] Vérification de l'alignement SIMD sur App :"
# Vérifie si l'allocation d'App utilise bien platform_aligned_alloc ou similaire dans main.c
if grep -q "aligned_alloc.*sizeof(App)" src/main.c; then
    echo "✔ PASS : App est alloué avec alignement en mémoire."
else
    echo "❌ ÉCHEC : L'allocation d'App dans main.c ne semble pas expliciter l'alignement SIMD."
fi
