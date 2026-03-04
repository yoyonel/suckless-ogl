# Rapport d'Audit Qualité - yoyonel/suckless-ogl

## Résumé
L'analyse de la base de code C/C++ révèle la présence de **3** familles principales de patterns de boilerplate et de bruit visuel qui augmentent significativement la charge cognitive.

## Top des Répétitions

### 1. Duplication de la configuration des textures OpenGL (Boilerplate)
**Description du problème :**
La création et la configuration de textures 2D via l'API OpenGL (`glGenTextures`, `glBindTexture`, `glTexImage2D`, et les multiples appels à `glTexParameteri` pour le filtrage et le wrapping) sont dupliquées de manière quasi-identique dans de nombreux fichiers, en particulier dans les modules d'effets post-processing (`fx_bloom.c`, `fx_auto_exposure.c`, `fx_dof.c`, `fx_motion_blur.c`, etc.).
Bien qu'une fonction `render_utils_create_texture_2d` existe, de nombreux fichiers continuent d'utiliser la séquence d'appels manuels de bas niveau.

**Extrait de code (Le "Avant") :**
```c
	glGenTextures(1, &auto_exp->downsample_tex);
	glBindTexture(GL_TEXTURE_2D, auto_exp->downsample_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, LUM_HISTOGRAM_MAP_SIZE,
	             LUM_HISTOGRAM_MAP_SIZE, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

**Occurrences estimées et fichiers concernés :**
Environ **15 à 20 occurrences**. Fichiers : `src/effects/fx_bloom.c`, `src/effects/fx_auto_exposure.c`, `src/effects/fx_dof.c`, `src/effects/fx_motion_blur.c`, `src/env_manager.c`, `src/ui.c`.

**Suggestion d'abstraction théorique :**
Généraliser et forcer l'utilisation d'une fonction utilitaire centralisée comme `render_utils_create_texture_2d` (déjà existante mais sous-utilisée) ou créer une variante simplifiée `render_utils_create_simple_texture(width, height, internalFormat, filter)` pour encapsuler l'allocation et le setup des paramètres standards (GL_LINEAR, GL_CLAMP_TO_EDGE).

---

### 2. Duplication de la création des Framebuffers (Couplage/Boilerplate)
**Description du problème :**
La séquence de création d'un Framebuffer (FBO) associant une texture en tant qu'attachement de couleur (`glGenFramebuffers`, `glBindFramebuffer`, `glFramebufferTexture2D`, `glCheckFramebufferStatus`) est un pattern qui se répète à chaque fois qu'un effet de post-traitement a besoin d'une cible de rendu.

**Extrait de code (Le "Avant") :**
```c
	glGenFramebuffers(1, &bloom->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, bloom->fbo);
        // ... (création texture) ...
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, bloom->mips[0].texture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // ...
```

**Occurrences estimées et fichiers concernés :**
Environ **8 à 10 occurrences**. Fichiers : `src/effects/fx_bloom.c`, `src/effects/fx_auto_exposure.c`, `src/effects/fx_dof.c`, `src/postprocess.c`, `src/tracy_manager.c`.

**Suggestion d'abstraction théorique :**
Créer une fonction utilitaire `render_utils_create_fbo(GLuint* fbo, GLuint texture, GLenum attachment)` qui s'occupe de la génération, de l'attachement et de la vérification du statut en une seule ligne.

---

### 3. Formatage manuel redondant de chaînes de caractères (Bruit Visuel)
**Description du problème :**
L'utilisation répétitive de `safe_snprintf` (introduit pour des raisons de sécurité liées à `clang-tidy`) pollue visuellement la logique de haut niveau, particulièrement dans les modules d'interface utilisateur (UI) et d'input pour formater de petites chaînes d'information ou de debug.

**Extrait de code (Le "Avant") :**
```c
	(void)safe_snprintf(buf, sizeof(buf), "HDR: %s",
	                    app->settings.hdr_enabled ? "ON" : "OFF");
```

**Occurrences estimées et fichiers concernés :**
Plus de **30 occurrences**. Fichiers : `src/app_input.c`, `src/app_ui.c`, `src/app_metrics.c`, `src/postprocess_input.c`.

**Suggestion d'abstraction théorique :**
Même si `safe_snprintf` est requis, dans le contexte spécifique du rendu UI (comme ImGui ou un système custom), des fonctions "helpers" de plus haut niveau pourraient être créées, ex: `ui_text_format("HDR: %s", value)` qui masque le buffer temporaire et l'appel à `safe_snprintf`.
