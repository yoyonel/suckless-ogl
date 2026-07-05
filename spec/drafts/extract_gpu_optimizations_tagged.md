## 1. INVARIANTS D'OPTIMISATION GPU ET MULTI-DRAW INDIRECT

* [LOCAL:sorting] [ANCRÉ] [RENDERDOC] Exécuter le tri bitonic GPU uniquement sur des entrées proxies de 8 octets (4-byte float depth, 4-byte int index) pour économiser la bande passante VRAM.
* [LOCAL:sorting] [ANCRÉ] [ARCHI-REVIEW] Utiliser une passe de permutation GPU pour réorganiser les structures larges de 128 octets après le tri des proxies.
* [LOCAL:sorting] [ANCRÉ] [ARCHI-REVIEW] Éliminer toute divergence de branchement (instruction 'if') dans les shaders de tri réseau pour maximiser l'occupation des threads GPU.
* [LOCAL:sorting] [ANCRÉ] [ARCHI-REVIEW] Employer le tri LSD Radix (4 passes de 8 bits) sur le CPU à la place de qsort pour trier entre 1 000 et 100 000 éléments.
* [LOCAL:sorting] [ANCRÉ] [ARCHI-REVIEW] Convertir les floats via le trick IEEE-754 (inverser le bit de signe si positif, tous les bits si négatif) pour trier les nombres flottants comme des entiers non signés.
* [LOCAL:mdi] [ANCRÉ] [ARCHI-REVIEW] Restreindre l'usage du MDI aux scènes comportant plusieurs types de maillages distincts partageant un Geometry Atlas commun.
* [LOCAL:mdi] [ANCRÉ] [GREP] Regrouper toutes les géométries rendues par MDI dans un VBO/EBO unique et indexer via baseVertex/firstIndex.
* [LOCAL:mdi] [ANCRÉ] [GREP] Aligner le Draw Indirect Buffer (DIB) sur la structure C standard DrawElementsIndirectCommand (count, instanceCount, firstIndex, baseVertex, baseInstance).
* [LOCAL:mdi] [ANCRÉ] [COMPILATEUR] Récupérer l'index d'instance globale dans les shaders MDI avec 'gl_BaseInstanceARB + gl_InstanceID' en exigeant l'extension GL_ARB_shader_draw_parameters ou GLSL 4.6.
* [LOCAL:mdi] [ANCRÉ] [RENDERDOC] Prévenir les stalls CPU-GPU sur le DIB dynamique via double/triple buffering combiné à l'orphelinage de buffer (glBufferData avec pointeur NULL et GL_STREAM_DRAW).
* [LOCAL:gpu] [ANCRÉ] [ARCHI-REVIEW] Conserver les instances de SSBO persistantes dans la structure d'application pour éviter les allocations dynamiques de buffers à chaque frame.
* [LOCAL:gpu] [ANCRÉ] [GREP] Allouer les buffers de lecture hôte avec glBufferStorage et les flags GL_MAP_READ_BIT | GL_CLIENT_STORAGE_BIT pour forcer la résidence en mémoire visible par l'hôte.
* [LOCAL:gpu] [ANCRÉ] [RENDERDOC] Rapatrier les petits volumes de données du GPU (ex. float unique de luminance) avec glGetBufferSubData pour court-circuiter les verrous de synchronisation de mapping.

## 2. RÈGLES DE LIAISON DE SOMMETS (VERTEX ATTRIB BINDING GL 4.3+) ET SIMD

* [LOCAL:vertex_attrib] [ANCRÉ] [GREP] Découpler le format des attributs des buffers sources via glVertexAttribFormat, glVertexAttribBinding et glBindVertexBuffer.
* [LOCAL:vertex_attrib] [ANCRÉ] [ARCHI-REVIEW] Appliquer la convention de binding des VAO : points de liaison 0 & 1 pour la géométrie (position, normale, texcoords), point de liaison 2 pour les données d'instances.
* [LOCAL:simd] [ANCRÉ] [COMPILATEUR] Offloader la conversion F32 en F16 vers le CPU en utilisant les intrinsèques AVX2 et F16C (_mm256_cvtps_ph) pour supprimer le goulot d'étranglement de conversion dans glTexSubImage2D.
* [LOCAL:simd] [ANCRÉ] [COMPILATEUR] Activer les optimisations d'architecture native du compilateur via les drapeaux '-march=native -mavx2 -mf16c'.
* [LOCAL:simd] [ANCRÉ] [ARCHI-REVIEW] Allouer un tampon de travail transitoire de taille Width * Height * 8 octets (RGBA16) lors de la conversion SIMD CPU.

## 3. RÉSILIENCE INTER-VENDEURS (CROSS-VENDOR) ET CORRECTIONS DE JOINTURES

* [LOCAL:cross-vendor] [ANCRÉ] [GREP] Remplacer les fonctions de dérivées d'écran dFdx, dFdy et fwidth par des calculs d'anti-aliasing ou de lissage analytiques.
* [LOCAL:cross-vendor] [ANCRÉ] [COMPILATEUR] Proscrire les calculs de dérivées à l'intérieur de branches conditionnelles divergentes; les précalculer en amont du branchement.
* [LOCAL:cross-vendor] [ANCRÉ] [GREP] Clamer systématiquement les arguments intermédiaires des fonctions sensibles comme pow() pour éviter les underflows/overflows.
* [LOCAL:cross-vendor] [ANCRÉ] [COMPILATEUR] Encadrer explicitement par des parenthèses les expressions flottantes complexes pour empêcher les optimisations agressives non déterministes du compilateur.
* [LOCAL:cross-vendor] [ANCRÉ] [GREP] Remplacer les fonctions d'échantillonnage à MIP implicite par textureLod lors du sampling de textures sensibles.
* [LOCAL:cross-vendor] [ANCRÉ] [COMPILATEUR] Déclarer explicitement les types dans les opérations binaires (suffixes littéraux 'u' ou casts) pour empêcher les échecs de conversion implicite.
* [LOCAL:cross-vendor] [ANCRÉ] [ARCHI-REVIEW] Stocker les résultats de calculs lourds (ex. luma) dans les canaux inutilisés des textures (ex. alpha) pour uniformiser l'accès multi-plateforme.
* [LOCAL:skybox] [ANCRÉ] [ARCHI-REVIEW] Préférer le mapping equirectangulaire direct aux cubemaps pour éliminer les jointures de faces et la conversion par compute shader.
* [LOCAL:ibl] [ANCRÉ] [GREP] Configurer les wrap modes des textures IBL en GL_REPEAT pour le paramètre GL_TEXTURE_WRAP_S et GL_CLAMP_TO_EDGE pour GL_TEXTURE_WRAP_T.
* [LOCAL:ibl] [ANCRÉ] [GREP] Forcer l'échantillonnage de la carte d'irradiance au MIP level 0.0 avec textureLod() pour éliminer le saut de MIP au wrap-around UV.
* [LOCAL:billboard] [ANCRÉ] [ARCHI-REVIEW] Projeter les plans tangents à la sphère en 2D pour dériver l'AABB exact et minimiser le fragment overdraw.
* [LOCAL:billboard] [ANCRÉ] [GREP] Clamer les coordonnées NDC à l'infini (+-10000.0) via une condition ternaire (nx >= 0.0 ? 1.0 : -1.0) * 10000.0 quand la tangente traverse le plan Z >= 0 de la caméra.
* [LOCAL:billboard] [ANCRÉ] [GREP] Culler les sphères en arrière de la caméra uniquement lorsque viewPos.z > sphereRadius pour préserver l'affichage des volumes intersectant le plan de la caméra.
* [LOCAL:billboard] [ANCRÉ] [RENDERDOC] Positionner le plan du billboard sur le plan frontal de la sphère (Z_nearest = Z_view + R) et écrire gl_FragDepth dans le Fragment Shader pour garantir une occlusion correcte.
* [LOCAL:billboard] [ANCRÉ] [COMPILATEUR] Extraire le plan zNear dynamiquement dans le shader depuis la matrice de projection via 'projection[3][2] / (projection[2][2] - 1.0)'.
* [LOCAL:billboard] [ANCRÉ] [COMPILATEUR] Utiliser l'interpolation 'flat' pour tous les attributs constants de la sphère pour éviter les bruits de précision float32 à la silhouette.
* [LOCAL:billboard] [ANCRÉ] [GREP] Détecter la caméra à l'intérieur de la sphère avec un epsilon hybride additif/multiplicatif : distSq <= r2 + max(r2 * 0.005, 1e-4).
* [LOCAL:billboard] [ANCRÉ] [COMPILATEUR] Extraire et stocker les facteurs de projection diagonaux (sx, sy) avant l'évaluation des branches conditionnelles.

## 4. INTERDICTIONS FORMELLES ET ANTI-PATTERNS (RENDU ET SHADERS)

* [LOCAL:vertex_attrib] [ANTI-PATTERN] [GREP] Interdiction d'utiliser glVertexAttribPointer pour définir la structure et lier les VBO.
* [LOCAL:vertex_attrib] [ANTI-PATTERN] [GREP] Interdiction de convertir des entiers en pointeurs void* (performance-no-int-to-ptr) pour spécifier des offsets.
* [LOCAL:nvidia] [ANTI-PATTERN] [RENDERDOC] Interdiction d'appeler glObjectLabel immédiatement après glGen* sans liaison (glBind*) préalable de l'objet.
* [LOCAL:nvidia] [ANTI-PATTERN] [RENDERDOC] Interdiction de laisser une unité de texture active liée à 0 (toujours lier une texture dummy 1x1 blanche ou noire).
* [LOCAL:nvidia] [ANTI-PATTERN] [GREP] Interdiction de laisser des diviseurs d'attributs indéterminés dans les VAO (forcer glVertexAttribDivisor(index, 0) sur les données non instanciées et désactiver les attributs inutilisés via glDisableVertexAttribArray).
* [LOCAL:gpu] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction d'allouer ou de détruire des buffers (SSBO, VBO) au sein de la boucle de rendu frame-by-frame.
* [LOCAL:gpu] [ANTI-PATTERN] [RENDERDOC] Interdiction d'employer glMapBuffer pour des transferts de petits volumes de données host-to-device.
* [LOCAL:billboard] [ANTI-PATTERN] [GREP] Interdiction d'utiliser la fonction sign() dans le vertex shader de billboard pour déterminer le signe des tangents (collapse en cas de 0.0).
* [LOCAL:billboard] [ANTI-PATTERN] [GREP] Interdiction de culler les sphères arrières sur le seul critère du centre (viewPos.z > 0.0) au lieu de leur enveloppe volumétrique.
* [LOCAL:billboard] [ANTI-PATTERN] [GREP] Interdiction de coupler les shaders à une constante CPU fixe pour le plan zNear.
* [LOCAL:billboard] [ANTI-PATTERN] [GREP] Interdiction d'échantillonner ou de laisser interpoler les paramètres de sphère constants par les fonctions de perspective standards du GPU (utiliser le qualificateur flat).
* [LOCAL:billboard] [ANTI-PATTERN] [GREP] Interdiction d'utiliser un epsilon purement multiplicatif pour détecter la caméra dans la sphère.
* [LOCAL:simd] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction de laisser le driver GPU effectuer la conversion scalaire F32 en F16 lors de glTexSubImage2D pour des textures HDR lourdes.
* [LOCAL:sorting] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction de copier ou d'échanger des structures de données larges (> 8 octets) directement dans la VRAM durant le tri GPU.
* [LOCAL:ibl] [ANTI-PATTERN] [GREP] Interdiction d'activer le wrap mode GL_CLAMP_TO_EDGE horizontalement (GL_TEXTURE_WRAP_S) sur des cartes d'environnement equirectangulaires.
