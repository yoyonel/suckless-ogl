# Documentation : Intégration Artwork & Icônes Steam (Non-Steam)
Dernière mise à jour : 2026-06-28

Ce document détaille le pipeline automatisé permettant d'injecter des assets graphiques pour *suckless-ogl* au sein de *Steam* (version *Flatpak*).

## 1. Contexte Technique
*Steam* identifie les jeux "Non-Steam" via un fichier binaire `shortcuts.vdf`.

Pour associer des visuels, le client utilise :
* un *ID* calculé (*CRC32* du chemin du binaire),
* un dossier `grid/` spécifique au répertoire `userdata` de l'utilisateur,
* et un cache interne (`librarycache`) nécessitant une purge manuelle pour forcer le rafraîchissement.

## 2. Pipeline d'automatisation
Le pipeline repose sur trois composants :

1. Génération (`scripts/generate_steam_assets.sh`) : Utilise *ImageMagick* pour convertir `reference_image.png` en formats standards.
2. Injection (`scripts/inject_steam_art.py`) : Script *Python* manipulant le raccourci et copiant les assets dans le sandbox *Flatpak*.
3. Interface (`Justfile`) : Orchestre l'ensemble via `just steam-art`.

## 3. Workflow Opérationnel
Pour mettre à jour tes visuels après une modification de `reference_image.png`, exécute la commande : `just steam-art`

### Étapes automatisées par `just steam-art` :
1. Normalisation : Génération des 4 assets requis (*Banner*, *Cover*, *Hero*, *Logo*) et de l'icône `.ico`.
2. Détection d'*ID* : Extraction de l'*ID CRC32* depuis `shortcuts.vdf` via le script *Python*.
3. Bypass Sandbox : Copie forcée des assets dans le dossier `grid` interne au sandbox *Flatpak*.
4. Patch *VDF* : Mise à jour du chemin de l'icône dans le binaire `shortcuts.vdf`.

## 4. Résolution de problèmes
* L'asset reste noir/gris : Steam conserve un *cache agressif*.
  * Solution : Fermer Steam -> `rm -rf ~/.var/app/com.valvesoftware.Steam/.local/share/Steam/appcache/librarycache/*` -> Redémarrer Steam.
* L'icône ne s'affiche pas : Assure-toi que le fichier est bien au format `.ico` (les PNG peuvent échouer selon la version de Proton).
* `Permission Denied` : Si la copie échoue malgré le script, vérifie les droits d'accès via : `flatpak override --user --filesystem=/ton/chemin/projet com.valvesoftware.Steam`

## 5. Références
* SteamGridDB Wiki : https://www.reddit.com/r/steamgrid/wiki/overlays
* Valve VDF Spec : https://developer.valvesoftware.com/wiki/VDF
* ImageMagick CLI : https://imagemagick.org/script/convert.php
