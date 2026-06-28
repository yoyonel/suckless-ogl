#!/usr/bin/env python3
import os
import shutil
import struct
import sys
import zlib
from pathlib import Path

import vdf


def get_steam_root():
    paths = [
        Path.home() / ".var/app/com.valvesoftware.Steam/.local/share/Steam",
        Path.home() / ".local/share/Steam",
    ]
    for p in paths:
        if p.exists():
            return p
    return None


# Calcule l'ID Steam pour les jeux Non-Steam (CRC32 du chemin)
def get_steam_id(target_name):
    # L'ID est basé sur une string spécifique, mais le plus simple est de le
    # récupérer directement depuis le shortcuts.vdf pour éviter les erreurs de hash
    return None


def inject(target_name):
    steam_root = get_steam_root()
    shortcut_path = list(steam_root.glob("userdata/*/config/shortcuts.vdf"))[0]
    # Pointage direct sur le dossier grid du bon utilisateur
    grid_dir = shortcut_path.parent.parent / "grid"

    # --- AJOUT DE CETTE LIGNE ---
    grid_dir.mkdir(parents=True, exist_ok=True)
    # ----------------------------

    with open(shortcut_path, "rb") as f:
        data = vdf.binary_load(f)

    found_id = None
    for key, entry in data["shortcuts"].items():
        if entry.get("AppName") == target_name:
            found_id = entry.get("appid")
            break

    if not found_id:
        print(f"❌ Erreur : '{target_name}' non trouvé dans shortcuts.vdf")
        sys.exit(1)

    print(f"    ✓ ID détecté : {found_id}")

    mapping = {
        "banner.png": f"{found_id}.png",
        "cover.png": f"{found_id}p.png",
        "hero.png": f"{found_id}_hero.png",
        "logo.png": f"{found_id}_logo.png",
    }

    for src_name, dst_name in mapping.items():
        src = Path("assets/steam_grid") / src_name
        if src.exists():
            shutil.copy2(src, grid_dir / dst_name)
            print(f"    ✓ {src_name} -> {dst_name}")


def update_icon(target_name, icon_path):
    steam_root = get_steam_root()
    shortcut_path = list(steam_root.glob("userdata/*/config/shortcuts.vdf"))[0]
    grid_dir = shortcut_path.parent.parent / "grid"

    # 1. Copie interne de l'icône pour bypasser le sandbox Flatpak
    # On utilise un nom d'icône unique basé sur le target_name pour éviter les conflits
    internal_icon_path = grid_dir / f"{target_name}_icon.ico"
    try:
        shutil.copy2(icon_path, internal_icon_path)
    except Exception as e:
        print(f"    ❌ Erreur lors de la copie de l'icône : {e}")
        return

    # 2. Mise à jour sécurisée du fichier VDF
    with open(shortcut_path, "rb") as f:
        data = vdf.binary_load(f)

    found = False
    # La structure de données de shortcuts.vdf est un dict indexé par "0", "1", etc.
    # On parcourt tous les raccourcis pour trouver celui qui correspond au nom
    for key in data["shortcuts"]:
        entry = data["shortcuts"][key]
        if entry.get("AppName") == target_name:
            entry["icon"] = str(internal_icon_path.absolute())
            found = True
            print(f"    ✓ Icône {internal_icon_path} associée à {target_name}")
            break

    if found:
        with open(shortcut_path, "wb") as f:
            vdf.binary_dump(data, f)
    else:
        print(f"    ❌ Raccourci '{target_name}' introuvable dans shortcuts.vdf")


if __name__ == "__main__":
    target = sys.argv[1]
    icon = sys.argv[2]
    inject(target)
    update_icon(target, icon)
