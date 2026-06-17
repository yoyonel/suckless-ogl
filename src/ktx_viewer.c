/**
 * @file ktx_viewer.c
 * @brief Viewer minimal KTX2 — Affiche une texture PBR/HDR compressée avec
 * tone-mapping.
 *
 * Architecture & Pipeline :
 * 1. Chargement (Disque -> RAM) : Lecture du fichier .ktx2 via l'API libktx. Si
 * le fichier utilise une supercompression (ex: Zstandard), il est déflaté à la
 * volée.
 * 2. Décompression (Blocs -> Pixels) : Les GPU Desktop (Intel/Nvidia/AMD) ne
 * supportent pas le format ASTC HDR matériellement. Le CPU transcode donc les
 * blocs (Basis/UASTC/ASTC) vers un format SFLOAT brut (Half-Float 16-bit).
 * 3. Upload (RAM -> VRAM) : Envoi manuel vers GL_TEXTURE_2D. Un soin
 * particulier est apporté à l'alignement mémoire (Row Pitch) car le RGB16F
 * n'est pas un multiple de 4 octets.
 * 4. Rendu (GPU) : Un shader "Fullscreen Triangle" (sans VBO) lit la texture,
 * applique un tone-mapping de Reinhard (pour ramener le HDR sur un écran LDR)
 * et une correction Gamma.
 */
/**
 * =========================================================================================
 * PIPELINE DE GÉNÉRATION DES ASSETS GRAPHIQUES (HDR -> KTX2 UASTC + ZSTD)
 * =========================================================================================
 * * Ce visualiseur est conçu pour exploiter des textures HDR/PBR hautement
 * compressées. Les GPU de bureau (ex: Intel Iris Xe, AMD/Nvidia standards) ne
 * gèrent pas le décodage matériel du format ASTC HDR. La stratégie consiste
 * donc à utiliser le codec UASTC-HDR compressé au format Zstandard sur le
 * disque, puis à transcoder sur le CPU vers du Float 16-bit (SFLOAT) avant
 * l'envoi en VRAM.
 *
 * --- PERFORMANCES ATTENDUES ---
 * Source (.hdr)       : ~ 40 Ko  (Format Radiance classique)
 * Uncompressed (.ktx2): ~ 4.0 Mo (R16G16B16A16_SFLOAT)
 * UASTC-HDR 4x4       : ~ 512 Ko (Format de blocs universel)
 * UASTC-HDR + Zstd 18 : ~ 3 Ko   (Taille finale sur le disque)
 * * --- COMMANDES DE GÉNÉRATION ---
 * Outils requis : OpenImageIO (`oiiotool`) et KTX-Software (`ktx`).
 * * ÉTAPE 1 : Conversion stricte du format Radiance (.hdr) vers OpenEXR
 * linéaire (.exr) (Préserve la plage dynamique et évite le clamping). $
 * oiiotool assets/textures/hdr/axis_test.hdr -o /tmp/axis_test.exr
 * * ÉTAPE 2 : Génération de l'image KTX2 cible (UASTC-HDR-4x4 + Zstd 18)
 * On peut le faire en une seule commande 'create' depuis l'EXR :
 * $ ktx create --format R16G16B16A16_SFLOAT \
 * --encode uastc-hdr-4x4 \
 * --zstd 18 \
 * /tmp/axis_test.exr assets/textures/hdr/axis_test_uastc_hdr4x4_zstd18.ktx2
 * * ALTERNATIVE (Utilisée dans les tests) : Via 'ktx encode' depuis un KTX2
 * non-compressé :
 * $ ktx encode --codec uastc-hdr-4x4 --zstd 18 \
 * assets/textures/hdr/axis_test_uncompressed.ktx2 \
 * assets/textures/hdr/axis_test_uastc_hdr4x4_zstd18.ktx2
 * * =========================================================================================
 */
#include <glad/glad.h>

#include "utils.h"
#include <GLFW/glfw3.h>
#include <ktx.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
 * Configurations et Constantes Globales
 * ========================================================================= */

/** Constantes définissant la géométrie de la fenêtre et les buffers textes. */
enum {
	WINDOW_W = 1280,
	WINDOW_H = 720,
	LOG_BUF = 512,  /**< Taille du buffer pour l'extraction des logs de
	                   shaders GLSL. */
	MSG_BUF = 1024, /**< Taille du buffer pour le formatage des impressions
	                   console. */
};

/** Couleur de fond (Clear Color) très sombre pour faire ressortir les textures
 * HDR. */
static const float CLEAR_R = 0.07F;
static const float CLEAR_G = 0.07F;
static const float CLEAR_B = 0.10F;
static const float CLEAR_A = 1.00F;

/* =========================================================================
 * Code Source des Shaders GLSL (Inline)
 * ========================================================================= */

/**
 * @brief Vertex Shader : "Fullscreen Triangle"
 * * ASTUCE D'OPTIMISATION : Au lieu de créer un VBO/EBO avec 4 sommets pour un
 * quad, on utilise `gl_VertexID` pour générer un unique triangle gigantesque
 * qui couvre tout l'écran. C'est la méthode standard de l'industrie pour les
 * passes de post-processing.
 */
static const char* const VS_SRC =
    "#version 330 core\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

/**
 * @brief Fragment Shader : Échantillonnage HDR et Tone-Mapping
 * * MOTIVATION : Une image HDR contient des valeurs lumineuses supérieures
 * à 1.0 (ex: un soleil à 50.0). Si on affiche ça directement sur un écran (qui
 * plafonne à 1.0), l'image sera "brûlée" (blanche). L'opérateur de Reinhard
 * `hdr / (hdr + 1.0)` compresse la plage [0, +infini] vers [0, 1].
 */
static const char* const FS_SRC =
    "#version 330 core\n"
    "in  vec2      uv;\n"
    "out vec4      frag_color;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "    vec3 hdr    = texture(u_tex, uv).rgb;\n"
    "    vec3 mapped = hdr / (hdr + vec3(1.0));\n" /* Reinhard Tone-mapping */
    "    frag_color  = vec4(pow(mapped, vec3(1.0/2.2)), 1.0);\n" /* Correction
                                                                    Gamma 2.2 */
    "}\n";

/* =========================================================================
 * Structures de Données
 * ========================================================================= */

/** @brief Encapsule la fenêtre système et le contexte de rendu. */
typedef struct {
	GLFWwindow* window;
} GlContext;

/** * @brief Encapsule une ressource KTX2 en RAM.
 * Contient la taille originale du fichier pour les métriques de compression.
 */
typedef struct {
	ktxTexture2* tex;    /**< Pointeur vers l'objet texture de libktx. */
	bool was_transcoded; /**< Indique si un transcodage CPU (Basis->Float) a
	                        eu lieu. */
	size_t file_size_bytes; /**< Poids réel du fichier sur le disque (en
	                           octets). */
} KtxAsset;

/** @brief Encapsule l'objet texture OpenGL résidant en VRAM. */
typedef struct {
	GLuint
	    id; /**< Identifiant (Handle) de la texture générée par OpenGL. */
	GLint internal_fmt; /**< Format interne GL (ex : GL_RGB16F). */
	GLenum base_fmt;    /**< Format de base GL (ex : GL_RGB). */
	GLenum type;        /**< Type de composant (ex : GL_HALF_FLOAT). */
	GLsizei w;          /**< Largeur de l'image (Level 0). */
	GLsizei h;          /**< Hauteur de l'image (Level 0). */
} GlTexture;

/** @brief Encapsule le programme GPU lié. */
typedef struct {
	GLuint program_id;
} ShaderProg;

/* =========================================================================
 * Implémentation : Contexte & Shaders
 * ========================================================================= */

/**
 * @brief Initialise GLFW et GLAD pour fournir un contexte OpenGL 3.3 Core.
 */
static bool gl_context_init(GlContext* ctx, const char* title)
{
	if (glfwInit() == GLFW_FALSE) {
		(void)fputs("[GLFW] Échec de l'initialisation\n", stderr);
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	ctx->window = glfwCreateWindow(WINDOW_W, WINDOW_H, title, NULL, NULL);
	if (!ctx->window) {
		(void)fputs("[GLFW] Échec de la création de fenêtre\n", stderr);
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(ctx->window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		(void)fputs("[GLAD] Échec du chargement de l'API OpenGL\n",
		            stderr);
		glfwTerminate();
		return false;
	}
	return true;
}

static void gl_context_destroy(GlContext* ctx)
{
	if (ctx && ctx->window) {
		glfwTerminate();
		ctx->window = NULL;
	}
}

/**
 * @brief Compile un shader GLSL individuel et vérifie les erreurs.
 */
static GLuint shader_compile(GLenum type, const char* src)
{
	GLuint shader_id = glCreateShader(type);
	glShaderSource(shader_id, 1, &src, NULL);
	glCompileShader(shader_id);

	GLint compile_ok = 0;
	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok == GL_FALSE) {
		char log_buf[LOG_BUF];
		glGetShaderInfoLog(shader_id, LOG_BUF, NULL, log_buf);
		(void)fputs("[GLSL] Erreur de compilation :\n", stderr);
		(void)fputs(log_buf, stderr);
	}
	return shader_id;
}

/**
 * @brief Lie le Vertex et le Fragment shader dans un programme GPU.
 */
static bool shader_program_create(ShaderProg* prog, const char* vs_src,
                                  const char* fs_src)
{
	GLuint vert = shader_compile(GL_VERTEX_SHADER, vs_src);
	GLuint frag = shader_compile(GL_FRAGMENT_SHADER, fs_src);

	prog->program_id = glCreateProgram();
	glAttachShader(prog->program_id, vert);
	glAttachShader(prog->program_id, frag);
	glLinkProgram(prog->program_id);

	/* Nettoyage propre : Les shaders sont inutiles une fois liés au
	 * programme principal */
	glDetachShader(prog->program_id, vert);
	glDetachShader(prog->program_id, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	GLint link_ok = 0;
	glGetProgramiv(prog->program_id, GL_LINK_STATUS, &link_ok);
	if (link_ok == GL_FALSE) {
		char log_buf[LOG_BUF];
		glGetProgramInfoLog(prog->program_id, LOG_BUF, NULL, log_buf);
		(void)fputs("[GLSL] Erreur de linkage :\n", stderr);
		(void)fputs(log_buf, stderr);
		return false;
	}
	return true;
}

static void shader_program_destroy(ShaderProg* prog)
{
	if (prog && prog->program_id != 0U) {
		glDeleteProgram(prog->program_id);
		prog->program_id = 0;
	}
}

/* =========================================================================
 * Implémentation : Pipeline KTX2 (Load -> Décode CPU -> Upload GPU)
 * ========================================================================= */

/**
 * @brief Charge le fichier KTX2 depuis le disque et le rend exploitable pour
 * OpenGL.
 * * MOTIVATION DE L'ARCHITECTURE :
 * Le format KTX2 supporte l'encodage universel (BasisLZ / UASTC). Normalement,
 * ces blocs sont envoyés tels quels à la VRAM. CEPENDANT, les drivers OpenGL
 * "Desktop" (notamment les puces Intel Iris Xe ou AMD basiques) ne contiennent
 * pas de décodeur matériel pour l'ASTC HDR. Si on envoie les blocs natifs, on
 * obtient un plantage (Segfault du driver) ou une bouillie violette/magenta.
 * * SOLUTION :
 * On intercepte toute texture qui est "isCompressed". Si elle est transcodable,
 * on force sa décompression sur le CPU vers de la RAM non compressée
 * (RGBA_HALF), que n'importe quelle carte graphique standard sait avaler et
 * afficher.
 */
static bool ktx_asset_load(KtxAsset* asset, const char* filepath)
{
	asset->tex = NULL;
	asset->was_transcoded = false;
	asset->file_size_bytes = 0;

	/* 1. Détermination de la taille réelle sur le disque (pour l'audit des
	 * ratios) */
	FILE* fp_asset = fopen(filepath, "rb");
	if (fp_asset) {
		if (fseek(fp_asset, 0, SEEK_END) == 0) {
			asset->file_size_bytes = (size_t)ftell(fp_asset);
		}
		(void)fclose(fp_asset);
	}

	/* 2. Chargement via libktx. Le flag LOAD_IMAGE_DATA décompresse le Zstd
	 * si présent. */
	KTX_error_code err = ktxTexture2_CreateFromNamedFile(
	    filepath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &asset->tex);

	if (err != KTX_SUCCESS) {
		(void)fputs("[KTX] Chargement échoué : ", stderr);
		(void)fputs(ktxErrorString(err), stderr);
		(void)fputc('\n', stderr);
		return false;
	}

	/* 3. Décodage Logiciel Sécurisé (Anti-Segfault Intel/Desktop) */
	if (asset->tex->isCompressed) {
		if (ktxTexture2_NeedsTranscoding(asset->tex) ||
		    ktxTexture2_IsTranscodable(asset->tex)) {
			(void)fputs(
			    "[KTX] Transcodage CPU (Universal Basis/UASTC -> "
			    "SFLOAT/HALF)...\n",
			    stdout);
			/* KTX_TTF_RGBA_HALF demande explicitement des floats
			 * 16-bit (adapté au PBR) */
			err = ktxTexture2_TranscodeBasis(asset->tex,
			                                 KTX_TTF_RGBA_HALF, 0);
		} else {
			/* Cas spécifique : Le fichier est encodé nativement en
			 * ASTC, libktx refuse de transcoder car il le considère
			 * "prêt pour le GPU". On le force à décoder. */
			(void)fputs(
			    "[KTX] Décodage logiciel forcé (ASTC Natif -> "
			    "SFLOAT brut)...\n",
			    stdout);
			err = ktxTexture2_DecodeAstc(asset->tex);
		}

		if (err != KTX_SUCCESS) {
			(void)fputs("[KTX] Décompression échouée : ", stderr);
			(void)fputs(ktxErrorString(err), stderr);
			(void)fputc('\n', stderr);
			ktxTexture_Destroy((ktxTexture*)asset->tex);
			asset->tex = NULL;
			return false;
		}
		asset->was_transcoded = true;
	}

	return true;
}

/**
 * @brief Affiche un rapport complet sur les métadonnées et l'efficacité de la
 * compression.
 */
static void ktx_asset_print_info(const KtxAsset* asset, const char* path)
{
	const ktxTexture2* tex = asset->tex;
	char buf[MSG_BUF];

	(void)fputs(
	    "\n=======================================================\n",
	    stdout);
	(void)fputs("🔍  AUDIT DES METADONNÉES & COMPRESSION KTX2\n", stdout);
	(void)fputs("=======================================================\n",
	            stdout);

	(void)safe_snprintf(buf, sizeof(buf), "▶ Fichier cible    : %s\n",
	                    path);
	(void)fputs(buf, stdout);

	(void)safe_snprintf(buf, sizeof(buf),
	                    "▶ Dimensions       : %ux%ux%u (L x H x P)\n"
	                    "▶ Niveaux de Mip   : %u\n"
	                    "▶ Array Layers     : %u\n",
	                    tex->baseWidth, tex->baseHeight, tex->baseDepth,
	                    tex->numLevels, tex->numLayers);
	(void)fputs(buf, stdout);

	(void)safe_snprintf(
	    buf, sizeof(buf),
	    "▶ Vulkan Format    : 0x%08X\n"
	    "▶ Supercompression : %s (e.g. Zstandard)\n",
	    tex->vkFormat,
	    ktxSupercompressionSchemeString(tex->supercompressionScheme));
	(void)fputs(buf, stdout);

	ktx_uint32_t num_comp = 0;
	ktx_uint32_t comp_bytes = 0;
	ktxTexture2_GetComponentInfo((ktxTexture2*)tex, &num_comp, &comp_bytes);

	(void)safe_snprintf(
	    buf, sizeof(buf),
	    "▶ Canaux/Précision : %u composant(s), %u octets/canal\n"
	    "▶ Espace Couleur   : Modèle=%d | Contenu HDR: %s\n",
	    num_comp, comp_bytes,
	    (int)ktxTexture2_GetColorModel_e((ktxTexture2*)tex),
	    (int)ktxTexture2_IsHDR((ktxTexture2*)tex) ? "Oui" : "Non");
	(void)fputs(buf, stdout);

	/* Métriques de compression (Taille Disque VS Empreinte VRAM Finale) */
	double ratio = 0.0;
	if (asset->file_size_bytes > 0) {
		ratio = (double)tex->dataSize / (double)asset->file_size_bytes;
	}

	(void)fputs("\n--- Efficacité de la Chaîne d'Assets ---\n", stdout);
	(void)safe_snprintf(
	    buf, sizeof(buf),
	    "▶ Poids Disque     : %zu octets (Compressé / Zstd)\n"
	    "▶ Empreinte VRAM   : %zu octets (Données brutes injectées)\n"
	    "▶ Taux Compression : %.2fx (Le fichier pèse %.1f%% de "
	    "l'original)\n"
	    "▶ Pipeline GPU     : %s\n",
	    asset->file_size_bytes, tex->dataSize, ratio,
	    (ratio > 0.0) ? (100.0 / ratio) : 0.0,
	    (int)(asset->was_transcoded)
	        ? "Décodage Logiciel CPU -> VRAM Native"
	        : "Direct Pass-through Hardware");
	(void)fputs(buf, stdout);
	(void)fputs(
	    "=======================================================\n\n",
	    stdout);
}

static void ktx_asset_destroy(KtxAsset* asset)
{
	if (asset && asset->tex) {
		ktxTexture_Destroy((ktxTexture*)asset->tex);
		asset->tex = NULL;
	}
}

/**
 * @brief Lit le DFD (Data Format Descriptor) pour attribuer les formats OpenGL
 * exacts.
 */
static void ktx_resolve_gl_format(const KtxAsset* asset, GLint* internal_fmt,
                                  GLenum* base_fmt, GLenum* gl_type)
{
	ktx_uint32_t num_comp = 0;
	ktx_uint32_t comp_bytes = 0;
	ktxTexture2_GetComponentInfo((ktxTexture2*)asset->tex, &num_comp,
	                             &comp_bytes);

	/* Adaptabilité dynamique selon la nature du transcodage (ex: RGB = 3,
	 * RGBA = 4). comp_bytes = 2 signifie Float 16-bit (Half). */
	*gl_type = (comp_bytes == 2U) ? GL_HALF_FLOAT : GL_FLOAT;

	if (num_comp == 3U) {
		*base_fmt = GL_RGB;
		*internal_fmt = (comp_bytes == 2U) ? GL_RGB16F : GL_RGB32F;
	} else {
		*base_fmt = GL_RGBA;
		*internal_fmt = (comp_bytes == 2U) ? GL_RGBA16F : GL_RGBA32F;
	}
}

/**
 * @brief Envoie les données CPU (décodées) vers la mémoire de la carte
 * graphique (VRAM).
 */
static bool gl_texture_upload_from_ktx(GlTexture* tex, const KtxAsset* asset)
{
	/* SÉCURITÉ : Bloque tout upload de données compressées vers un appel
	 * d'image non-compressée */
	if (asset->tex->isCompressed) {
		(void)fputs(
		    "[GL] ERREUR FATALE : Données encore compressées. Le "
		    "driver va Segfault !\n",
		    stderr);
		return false;
	}

	ktx_uint8_t* data = ktxTexture_GetData((ktxTexture*)asset->tex);
	if (!data) {
		(void)fputs("[GL] Aucune donnée pixel extraite\n", stderr);
		return false;
	}

	ktx_resolve_gl_format(asset, &tex->internal_fmt, &tex->base_fmt,
	                      &tex->type);

	tex->w = (GLsizei)asset->tex->baseWidth;
	tex->h = (GLsizei)asset->tex->baseHeight;

	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);

	/* EXPLICATION DE L'ALIGNEMENT MÉMOIRE (Le point critique qui fait
	 * crasher les drivers) : Par défaut, OpenGL attend que chaque ligne de
	 * pixel (Row Pitch) se termine sur un multiple de 4 octets. Si on
	 * utilise du GL_RGB16F (3 canaux de 2 octets = 6 octets/pixel), la
	 * ligne ne sera mathématiquement pas toujours alignée sur 4. Si on ne
	 * force pas le GL_UNPACK_ALIGNMENT à 1, le driver OpenGL essaie de lire
	 * du "padding" virtuel en dehors du buffer RAM -> Dépassement de tampon
	 * -> Segfault. */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(GL_TEXTURE_2D, 0, tex->internal_fmt, tex->w, tex->h, 0,
	             tex->base_fmt, tex->type, data);

	glPixelStorei(
	    GL_UNPACK_ALIGNMENT,
	    4); /* Toujours remettre la machine d'état GL par défaut */

	/* Configuration du filtrage standard pour de l'IBL / Affichage basique
	 */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return true;
}

static void gl_texture_destroy(GlTexture* tex)
{
	if (tex && tex->id != 0U) {
		glDeleteTextures(1, &tex->id);
		tex->id = 0;
	}
}

/* =========================================================================
 * Implémentation : Boucle d'Application
 * ========================================================================= */

static void render_loop(GLFWwindow* window, const ShaderProg* prog,
                        const GlTexture* tex)
{
	/* La spécification OpenGL Core Profile oblige à lier un VAO actif,
	 * même si l'on n'utilise aucun buffer de sommets (VBO) ! */
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glUseProgram(prog->program_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex->id);

	GLint loc = glGetUniformLocation(prog->program_id, "u_tex");
	if (loc >= 0) {
		glUniform1i(loc,
		            0); /* Lier le sampler2D à l'unité de texture 0 */
	}

	while (glfwWindowShouldClose(window) == 0) {
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		glClearColor(CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Dessine 3 sommets virtuels. La géométrie est créée
		 * mathématiquement dans le VS. */
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteVertexArrays(1, &vao);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		(void)fputs("Usage : ktx_viewer <chemin_vers_fichier.ktx2>\n",
		            stderr);
		return EXIT_FAILURE;
	}

	const char* filepath = argv[1];

	GlContext ctx = {0};
	if (!gl_context_init(&ctx, "KTX2 Viewer (Zstd/UASTC/ASTC)")) {
		return EXIT_FAILURE;
	}

	KtxAsset asset = {0};
	if (!ktx_asset_load(&asset, filepath)) {
		gl_context_destroy(&ctx);
		return EXIT_FAILURE;
	}

	/* Affichage des métriques après la passe de décodage */
	ktx_asset_print_info(&asset, filepath);

	GlTexture gl_tex = {0};
	if (!gl_texture_upload_from_ktx(&gl_tex, &asset)) {
		ktx_asset_destroy(&asset);
		gl_context_destroy(&ctx);
		return EXIT_FAILURE;
	}

	/* Une fois injectée dans la VRAM de la carte graphique, on peut purger
	 * la RAM système */
	ktx_asset_destroy(&asset);

	ShaderProg prog = {0};
	if (!shader_program_create(&prog, VS_SRC, FS_SRC)) {
		gl_texture_destroy(&gl_tex);
		gl_context_destroy(&ctx);
		return EXIT_FAILURE;
	}

	/* Bloque le thread principal dans la boucle de rendu jusqu'à appui sur
	 * Echap */
	render_loop(ctx.window, &prog, &gl_tex);

	/* Nettoyage scrupuleux avant la sortie */
	shader_program_destroy(&prog);
	gl_texture_destroy(&gl_tex);
	gl_context_destroy(&ctx);

	return EXIT_SUCCESS;
}
