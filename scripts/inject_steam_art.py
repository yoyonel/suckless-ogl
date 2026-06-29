#!/usr/bin/env python3
"""
Steam Artwork Injector.
A robust, type-safe utility for managing Steam Non-Steam game assets.
"""

import argparse
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Final


@dataclass
class SteamShortcut:
    """Represents a Steam Non-Steam Game shortcut."""

    app_name: str
    app_id: str


@dataclass
class SteamConfig:
    """Models the Steam environment with configurable paths."""

    root_paths: list[Path]
    grid_dir: Path
    shortcuts: list[SteamShortcut] = field(default_factory=list)


class SteamIntegrationError(Exception):
    """Custom exception for Steam integration failures."""


def get_steam_root(search_paths: list[Path]) -> Path:
    """Detects the Steam installation root from provided search paths."""
    for path in search_paths:
        if path.exists():
            return path
    raise SteamIntegrationError(f"Steam installation not found in: {search_paths}")


def load_config(search_paths: list[Path]) -> SteamConfig:
    """Initializes Steam configuration."""
    root = get_steam_root(search_paths)
    userdata = root / "userdata"
    # Utilisation de next(..., None) pour éviter l'indexation unsafe
    shortcut_file = next(userdata.glob("*/config/shortcuts.vdf"), None)

    if not shortcut_file:
        raise SteamIntegrationError("No shortcuts.vdf found.")

    return SteamConfig(
        root_paths=search_paths,
        grid_dir=shortcut_file.parent.parent / "grid",
        shortcuts=[SteamShortcut(app_name="app.exe", app_id="-1715085355")],
    )


def inject_assets(config: SteamConfig, target_name: str, icon_source: Path) -> None:
    """Copies artwork assets to the Steam grid folder."""
    shortcut = next((s for s in config.shortcuts if s.app_name == target_name), None)
    if not shortcut:
        raise SteamIntegrationError(f"Shortcut '{target_name}' not found.")

    config.grid_dir.mkdir(parents=True, exist_ok=True)

    assets: Final = {
        "banner.png": f"{shortcut.app_id}.png",
        "cover.png": f"{shortcut.app_id}p.png",
        "hero.png": f"{shortcut.app_id}_hero.png",
        "logo.png": f"{shortcut.app_id}_logo.png",
    }

    src_dir = Path("assets/steam_grid")
    for src, dst in assets.items():
        src_path = src_dir / src
        if src_path.exists():
            # Assignation à _ pour éviter l'avertissement de retour inutilisé
            _ = shutil.copy2(src_path, config.grid_dir / dst)
            print(f"    ✓ {src} -> {dst}")

    dest_icon = config.grid_dir / f"{target_name}_icon.ico"
    _ = shutil.copy2(icon_source, dest_icon)
    print(f"    ✓ Icon deployed to {dest_icon}")


def main() -> None:
    """Main execution entry point with CLI argument handling."""
    parser = argparse.ArgumentParser(
        description="Inject artwork and icons into Steam for Non-Steam games."
    )

    parser.add_argument(
        "target",
        help="The exact name of the application as it appears in the Steam library.",
    )
    parser.add_argument(
        "icon",
        type=Path,
        help="Path to the .ico file to be used as the application icon.",
    )
    parser.add_argument(
        "--paths",
        nargs="+",
        type=Path,
        default=[
            Path.home() / ".var/app/com.valvesoftware.Steam/.local/share/Steam",
            Path.home() / ".local/share/Steam",
        ],
        help="List of potential paths where Steam is installed.",
    )

    args = parser.parse_args()

    try:
        config = load_config(args.paths)
        inject_assets(config, args.target, args.icon)
        print("✓ Process completed successfully.")
    except SteamIntegrationError as e:
        print(f"❌ {e}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Fatal error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
