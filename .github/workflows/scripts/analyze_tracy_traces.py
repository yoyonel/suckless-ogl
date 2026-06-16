import csv
import sys

# Les zones critiques que l'on veut garantir
zones_to_check = [
    "App Update",
    "App Render",
    "Frame Timing",
    "GLFW SwapBuffers",
    "Tracy Screenshot Update",
]

found_zones = {}

try:
    # Le module csv gère nativement les guillemets et les virgules internes
    with open("trace_stats.csv", mode="r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row.get("name", "")
            if name in zones_to_check:
                # On récupère la vraie colonne call_count
                found_zones[name] = row.get("counts", "N/A")
except Exception as e:
    print(f"❌ Erreur critique lors de la lecture du CSV : {e}")
    sys.exit(1)

# Validation stricte
success = True
for zone in zones_to_check:
    if zone not in found_zones:
        print(f"❌ Échec : La zone '{zone}' est absente de la trace !")
        success = False
    else:
        print(f"  ✓ Zone '{zone}' validée ({found_zones[zone]} appels enregistrés).")

if not success:
    sys.exit(1)

print("✅ Succès : La trace est structurellement cohérente et exploitable !")
