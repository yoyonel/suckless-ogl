# Documentation: Steam Artwork & Icon Integration (Non-Steam)
Last update: 2026-06-28

This document details the automated pipeline for injecting graphical assets for *suckless-ogl* into *Steam* (Flatpak version).

## 1. Technical Context
*Steam* identifies "Non-Steam" games via a binary file named `shortcuts.vdf`.

To associate visuals, the client uses:
* an *ID* calculated (*CRC32* of the binary path),
* a `grid/` folder specific to the user's `userdata` directory,
* and an internal cache (`librarycache`) that requires manual clearing to force a refresh.

## 2. Automation Pipeline
The pipeline relies on three components:

1. Generation (`scripts/generate_steam_assets.sh`): Uses *ImageMagick* to convert `reference_image.png` into standard formats.
2. Injection (`scripts/inject_steam_art.py`): *Python* script that manipulates the shortcut and copies assets into the *Flatpak* sandbox.
3. Interface (`Justfile`): Orchestrates the entire process via `just steam-art`.

## 3. Operational Workflow
To update your visuals after modifying `reference_image.png`, run the command: `just steam-art`

### Steps automated by `just steam-art`:
1. Normalization: Generation of the 4 required assets (*Banner*, *Cover*, *Hero*, *Logo*) and the `.ico` icon.
2. *ID* Detection: Extraction of the *CRC32 ID* from `shortcuts.vdf` via the *Python* script.
3. Bypass Sandbox: Forced copy of assets into the `grid` folder inside the *Flatpak* sandbox.
4. *VDF* Patch: Updating the icon path within the binary `shortcuts.vdf` file.

## 4. Troubleshooting
* Asset remains black/gray: *Steam* maintains an *aggressive cache*.
  * Solution: Close Steam -> `rm -rf ~/.var/app/com.valvesoftware.Steam/.local/share/Steam/appcache/librarycache/*` -> Restart Steam.
* Icon not displaying: Ensure the file is in `.ico` format (PNGs may fail depending on the Proton version).
* `Permission Denied`: If copying fails, verify access rights via: `flatpak override --user --filesystem=/your/project/path com.valvesoftware.Steam`

## 5. References
* SteamGridDB Wiki: https://www.reddit.com/r/steamgrid/wiki/overlays
* Valve VDF Spec: https://developer.valvesoftware.com/wiki/VDF
* ImageMagick CLI: https://imagemagick.org/script/convert.php
