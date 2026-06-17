# 📄 Documentation d'Architecture : Intégration des Textures HDR au format KTX2

**Horodatage :** 19 Juin 2026, 17:35 CEST
**Contexte :** Refonte du sous-système de chargement asynchrone (`AsyncLoader`) et optimisation VRAM des Environment Maps (Skybox / IBL).

---

## 1. Introduction au Format KTX2

Historiquement, le moteur chargeait des images HDR au format Radiance (`.hdr`) via la librairie `stb_image`. Bien que standard, cette méthode forçait le CPU à parser l'encodage RLE puis à convertir les pixels en tableaux bruts de Half-Floats (16-bit), provoquant une explosion de l'empreinte mémoire vidéo (VRAM) lors de l'envoi au GPU.

Pour une texture 4K (4096x2048) avec ses mipmaps, l'empreinte VRAM atteignait **~89,5 Mo** (Format `GL_RGBA16F`).

La migration vers **KTX2** (Khronos Texture format v2) permet de résoudre ce problème. KTX2 est un conteneur robuste conçu spécifiquement pour les API modernes (OpenGL/Vulkan) supportant :
* Le stockage direct des formats matériels compressés (BC6H, ASTC, etc.).
* Le format universel **UASTC** couplé à la supercompression **Zstandard (Zstd)**.
* Le transcodage logiciel à la volée vers les formats natifs de la carte graphique.

---

## 2. Pipeline de Conversion d'Assets (.hdr -> .ktx2)

La génération des assets KTX2 nécessite les outils `oiiotool` (OpenImageIO) et `ktx create` (KTX-Software). La conversion se fait en deux étapes :
1. Conversion du `.hdr` (Radiance) en `.exr` (OpenEXR) pour stabiliser la plage dynamique linéaire.
2. Encodage en `.ktx2`.

### A. Format Non-Compressé (Raw 16-bit Float)
Idéal pour des temps de chargement CPU ultra-rapides, au détriment de l'espace disque et de la VRAM.

    # Étape 1 : HDR -> EXR
    oiiotool assets/textures/hdr/source.hdr -o /tmp/temp.exr

    # Étape 2 : EXR -> KTX2 (Non-compressé)
    ktx create --format R16G16B16A16_SFLOAT /tmp/temp.exr assets/textures/hdr/source_uncompressed.ktx2

### B. Format Compressé (UASTC-HDR + Zstd)
Format de production par défaut. L'asset est encodé de manière "Universelle".
*Important :* Ne **pas** utiliser l'argument `--generate-mipmap` à la création. Générer les mipmaps sur le disque force le CPU à les transcoder inutilement au chargement. La génération des mipmaps est déléguée au GPU (`glGenerateMipmap`).

    # Étape 1 : HDR -> EXR
    oiiotool assets/textures/hdr/source.hdr -o /tmp/temp.exr

    # Étape 2 : EXR -> KTX2 (UASTC + Zstd niveau 18)
    ktx create \
        --format R16G16B16A16_SFLOAT \
        --encode uastc-hdr-4x4 \
        --zstd 18 \
        /tmp/temp.exr assets/textures/hdr/source_uastc_zstd.ktx2

---

## 3. Intégration dans le Moteur Graphique

L'intégration repose sur le patron *Strategy* au sein du système `AsyncLoader`, via le module métier `async_backend_ktx.c` et le module OpenGL `texture.c`.

### Phase 1 : Décodage et Transcodage (CPU / Worker Thread)
Lors de l'analyse du fichier KTX2 par la `libktx` :
1. **Pass-through matériel :** Si l'iGPU cible supporte la compression HDR matérielle (ex: puces Desktop gérant `GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT`), le thread asynchrone transcode l'image UASTC vers des blocs **BC6H** natifs (`KTX_TTF_BC6HU`).
2. **Repli logiciel (Fallback) :** Si la compression n'est pas gérée, on transcode vers de la RAM brute en Half-Float (`KTX_TTF_RGBA_HALF`).

### Phase 2 : Upload GPU (Main Thread)
Le transfert des données via les PBO (Pixel Buffer Objects) s'adapte dynamiquement au format :
* Si les données ont été transcodées en BC6H, on utilise **`glCompressedTexSubImage2D`**.
* Sinon, on utilise la méthode classique **`glTexSubImage2D`**.

---

## 4. Le Grand Compromis : Bilan et Métriques

Le tableau suivant présente les profils de performance (mesurés sur une build optimisée `Ultra Release`) pour le chargement d'une image HDR 4K.

| Métrique (Image 4K)    | Ancien (.hdr via STB)  | KTX2 Non-Compressé | KTX2 (UASTC + Zstd18) |
| :--------------------- | :--------------------- | :----------------- | :-------------------- |
| **Poids sur le Disque**| ~ 26 Mo                | ~ 64 Mo            | **~ 9,3 Mo** |
| **Empreinte VRAM** | ~ 89,5 Mo              | ~ 89,5 Mo          | **~ 11,1 Mo** (BC6H)  |
| **Temps de Charge CPU**| ~ 910 ms               | **~ 200 ms** | ~ 700 ms              |
| **Goulot** | Parsing RLE + Conv. SIMD| I/O (Disque)       | Transcodage BasisU    |

### Analyse des Performances
* **La charge CPU :** Contrairement aux idées reçues, le transcodage UASTC matériel (700 ms) est en réalité **plus rapide** que le parsing traditionnel d'un fichier `.hdr` (910 ms), car il évite de manipuler des buffers temporaires gigantesques de 134 Mo en RAM (Floats 32-bit) nécessaires à `stb_image`.
* **Le gain Mémoire :** L'empreinte VRAM est divisée par 8 (soit 800% d'économie). L'espace disque est divisé par 3 par rapport au `.hdr`.
* **La mitigation (L'asynchronisme) :** Les ~700 ms de calcul sont invisibles pour l'utilisateur car ils sont confinés dans l'**`AsyncLoader`**. La boucle de rendu OpenGL maintient ses performances (144 Hz) pendant que le worker thread prépare la ressource.

### Conclusion
La chaîne d'assets `HDR -> KTX2 (UASTC)` validée avec upload matériel `BC6H` est désormais le standard absolu du moteur. Elle bat l'ancien pipeline `.hdr` sur **toutes les métriques** (Poids Disque, Empreinte VRAM, et Temps CPU), tout en prévenant les erreurs "Out Of Memory" (OOM) lors des phases de *crossfade* entre de multiples *Environment Maps*.
