# PLAN TECHNIQUE : RÉPARATION DE L'AUDIT ARCHITECTURAL (LOT V1)

Ce document décrit le plan technique détaillé et l'architecture de remédiation pour corriger les deux anomalies architecturales identifiées lors de l'audit. Ce plan est destiné aux développeurs C11 pour le moteur **suckless-ogl**.

---

## 1. Architecture de la Gestion Résiliente de la Scène (`scene_init.c` & `scene_cleanup.c`)

### A. Analyse de la Problématique et Risques de Régression
La fonction d'initialisation de la scène [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) alloue dynamiquement plusieurs sous-structures opaques sur le tas (`scene->gpu`, `scene->shaders`, `scene->simulation`, `scene->visuals`). Elle crée également de nombreuses ressources OpenGL (VAO, VBO, UBO, textures, programmes de shaders).

En cas d'échec de l'une de ces allocations ou initialisations (ex: échec de compilation d'un shader), le code actuel effectue des retours anticipés (`return 0`) sans nettoyage. L'appelant ([scene_subsys_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L508)) tente alors de récupérer en appelant [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82). Cependant, cette fonction de nettoyage n'a pas été conçue pour tolérer des structures partiellement allouées. Si un sous-composant comme `scene->shaders` ou `scene->gpu` n'est pas alloué ou n'a pas fini son initialisation, `scene_cleanup` provoque un crash par déréférencement de pointeur nul.

### B. Schéma du Point de Sortie Unique (`cleanup:`)
Pour respecter la **Constitution (Domaine I, Règle 10, 11, 12 et 13)**, nous restructurons [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) avec un schéma transactionnel basé sur un unique label `cleanup:` et l'utilisation systématique de `goto cleanup;` sur erreur.

#### Schéma d'implémentation proposé pour `scene_init` :
```c
int scene_init(Scene* scene)
{
	int success = 0;

	/* 1. Allocation des sous-structures opaques */
	scene->gpu = calloc(1, sizeof(SceneGPUResources));
	scene->shaders = calloc(1, sizeof(SceneShaders));
	scene->simulation = calloc(1, sizeof(SceneSimulation));
	scene->visuals = calloc(1, sizeof(SceneVisuals));

	if (!scene->gpu || !scene->shaders || !scene->simulation || !scene->visuals) {
		LOG_ERROR("Scene", "Failed to allocate scene sub-structs");
		goto cleanup;
	}

	/* 2. Initialisation de l'état nominal (pas d'allocations complexes) */
	scene_init_state(scene);

	/* 3. Chargement des shaders principaux */
	if (!scene_init_core_shaders(scene)) {
		goto cleanup;
	}

	/* 4. Création des ressources VAO/VBO système de base */
	render_utils_create_empty_vao(&scene->gpu->empty_vao);

	/* 5. Initialisation du shader de billboard */
	if (!scene_init_billboard_shader(scene)) {
		goto cleanup;
	}

	/* 6. Création des VBOs géométriques */
	render_utils_create_quad_vbo(&scene->gpu->quad_vbo);
	render_utils_create_wire_cube_vbo(&scene->gpu->wire_cube_vbo);
	render_utils_create_wire_quad_vbo(&scene->gpu->wire_quad_vbo);

	/* 7. Initialisation des sous-composants */
	skybox_init(&scene->visuals->skybox, scene->shaders->skybox);
	icosphere_init(&scene->geometry);

	glGenVertexArrays(1, &scene->gpu->icosphere_vao);
	glGenBuffers(1, &scene->gpu->icosphere_vbo);
	glGenBuffers(1, &scene->gpu->icosphere_nbo);
	glGenBuffers(1, &scene->gpu->icosphere_ebo);

	/* 8. Chargement de la bibliothèque de matériaux */
	scene->lighting.material_lib =
	    material_load_presets("assets/materials/pbr_materials.json");
	if (!scene->lighting.material_lib) {
		goto cleanup;
	}

	/* 9. Initialisation des Compute Shaders IBL */
	if (!scene_init_compute_resources(scene)) {
		goto cleanup;
	}

	/* 10. Initialisation de la grille de sondes (Light Probe Grid) */
	{
		int sphere_count = scene->lighting.material_lib->count;
		if (sphere_count > (DEFAULT_COLS * DEFAULT_COLS)) {
			sphere_count = DEFAULT_COLS * DEFAULT_COLS;
		}
		const int pcols = DEFAULT_COLS;
		const int prows = (sphere_count + pcols - 1) / pcols;
		light_probe_grid_init(&scene->lighting.probe_grid,
		                      (2 * pcols) + 1, (2 * prows) + 1, 3);
	}

	/* 11. Initialisation du shader instancié (SSBO ou standard) */
	Shader* inst_shader = NULL;
	if (!scene_init_instanced_shader(scene, &inst_shader)) {
		goto cleanup;
	}

	/* 12. Résolution des localisations d'uniformes */
	scene->shaders->debug_uniforms.projection = shader_get_uniform_location(
	    scene->shaders->debug_line, "projection");
	scene->shaders->debug_uniforms.view =
	    shader_get_uniform_location(scene->shaders->debug_line, "view");
	scene->shaders->debug_uniforms.u_stippled = shader_get_uniform_location(
	    scene->shaders->debug_line, "u_stippled");
	scene->shaders->debug_uniforms.u_billboard_mode =
	    shader_get_uniform_location(scene->shaders->debug_line,
	                                "u_billboardMode");
	scene->shaders->debug_uniforms.u_use_instance_col =
	    shader_get_uniform_location(scene->shaders->debug_line,
	                                "u_useInstanceColor");
	scene->shaders->debug_uniforms.u_color =
	    shader_get_uniform_location(scene->shaders->debug_line, "u_color");

	success = 1;

cleanup:
	if (!success) {
		/* Libération centralisée en cas d'échec */
		scene_cleanup(scene);
	}
	return success;
}
```

### C. Algorithme de Nettoyage Sécurisé & Idempotent
La fonction globale [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82) doit être restructurée pour tolérer les états partiellement initialisés. Les pointeurs de sous-structures opaques (`scene->gpu`, `scene->shaders`, etc.) et les handles OpenGL doivent être vérifiés avant toute opération. Une fois libérées, les variables et handles doivent être systématiquement remis à `NULL` ou `0`.

#### Ordre de libération sécurisé :
Le nettoyage doit s'opérer du plus dépendant au plus indépendant pour éviter de laisser des handles orphelins dans le pilote OpenGL :
1. **Unmap de buffers persistants** : Si `scene->gpu->billboard_ubo` est mappé, il doit être explicitement désengagé (`glUnmapBuffer`) avant destruction.
2. **Vertex Array Objects (VAO)** : `icosphere_vao`, `empty_vao`.
3. **Vertex Buffer Objects / Uniform Buffer Objects (VBO/UBO)** : `icosphere_vbo`, `icosphere_nbo`, `icosphere_ebo`, `quad_vbo`, `wire_cube_vbo`, `wire_quad_vbo`, `billboard_ubo` et `lum_ssbo`.
4. **Programmes OpenGL (Shaders)** : `pbr_instanced`, `pbr_billboard`, `pbr_ssbo`, `skybox`, `debug`, `debug_line`, `spmap_program`, `irmap_program`, `lum_pass1_program`, `lum_pass2_program`.
5. **Textures** : `hdr_texture`, `recycled_hdr_tex`, `brdf_lut_tex`, `spec_prefiltered_tex`, `irradiance_tex`, `dummy_black_tex`, `dummy_white_tex`, `transition_snapshot_tex`.
6. **Sous-composants et Systèmes internes** : `ibl_coordinator_cleanup`, `light_probe_grid_cleanup`, `instanced_group_cleanup`, `billboard_renderer_cleanup`, `skybox_cleanup`, `trail_renderer_cleanup`, `shockwave_renderer_cleanup`.
7. **Pointeurs Opaques** : Libération (`free`) de `scene->gpu`, `scene->shaders`, `scene->simulation`, `scene->visuals` et mise à `NULL` systématique.

#### Signatures sécurisées des routines internes de nettoyage (`scene_cleanup.c`) :
```c
static void scene_cleanup_pbr_shaders(Scene* scene);
static void scene_cleanup_shaders(Scene* scene);
static void scene_cleanup_geometry_buffers(Scene* scene);
static void scene_cleanup_buffers(Scene* scene);
static void scene_cleanup_textures(Scene* scene);
static void scene_cleanup_gpu_resources(Scene* scene);
```

Chacune de ces fonctions doit appliquer un filtre défensif strict :
```c
static void scene_cleanup_pbr_shaders(Scene* scene)
{
	if (!scene) return;

	if (scene->shaders) {
		SHADER_SAFE_DESTROY(scene->shaders->pbr_instanced);
		SHADER_SAFE_DESTROY(scene->shaders->pbr_billboard);
#ifdef USE_SSBO_RENDERING
		SHADER_SAFE_DESTROY(scene->shaders->pbr_ssbo);
#endif
	}

	if (scene->gpu) {
		GL_SAFE_DELETE_PROGRAM(scene->gpu->spmap_program);
		GL_SAFE_DELETE_PROGRAM(scene->gpu->irmap_program);
	}
}
```

---

## 2. Architecture de la Machine d'États IBL (`ibl_coordinator.c`)

### A. Analyse du Dysfonctionnement & Stuttering
Dans l'implémentation d'origine de [ibl_coordinator_update](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L377), une garde en début de fonction court-circuite le traitement si l'état courant est `IBL_STATE_DONE` ou `IBL_STATE_IDLE` :
```c
if (coord->state == IBL_STATE_IDLE || coord->state == IBL_STATE_DONE) {
	return coord->state;
}
```
Par conséquent, la branche `case IBL_STATE_DONE:` à l'intérieur du `switch(coord->state)` n'est jamais évaluée, faisant de l'appel `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)` du code mort.

#### Cas limite critique (Black Screen Transition) :
Dans [env_manager.c:L245](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/env_manager.c#L245), lorsque l'on utilise le mode `ENV_TRANSITION_BLACK_SCREEN`, l'application initie une transition de fondu au noir qui dure plusieurs frames. Pendant tout ce temps, `ibl_coordinator_get_results` n'est pas appelée immédiatement. L'état de l'IBL reste fixé à `IBL_STATE_DONE`, et `ibl_coordinator_update` continue d'être appelée à chaque trame.
* **Risque de Stuttering (Stall)** : Si l'on supprimait simplement la garde `IBL_STATE_DONE` en haut de la fonction, la barrière mémoire `glMemoryBarrier` serait appelée **à chaque frame** tant que la transition de fondu n'est pas complétée. Cette barrière force la synchronisation des caches d'écriture GPU, ce qui provoquerait des blocages sévères du pipeline et violerait le budget de fluidité nominal du thread principal (1 à 2 ms max).

### B. Restructuration du switch/case de `ibl_coordinator_update`
Pour résoudre ce problème de manière performante et propre, nous introduisons un flag booléen `barrier_executed` au sein de la structure `IBLCoordinator` déclarée dans `ibl_coordinator.h`.

1. **Ajout du flag dans la structure (`ibl_coordinator.h`)** :
   ```c
   typedef struct {
       /* ... autres champs ... */
       bool barrier_executed; /**< Indique si la barrière mémoire a été émise pour l'état DONE */
   } IBLCoordinator;
   ```

2. **Réinitialisation du flag dans `ibl_coordinator_reset` et `ibl_coordinator_init`** :
   ```c
   void ibl_coordinator_reset(IBLCoordinator* coord)
   {
       /* ... nettoyage des textures et sync ... */
       coord->barrier_executed = false;
       coord->state = IBL_STATE_IDLE;
   }
   ```

3. **Restructuration de la garde et du switch dans `ibl_coordinator_update`** :
   La garde d'entrée n'interrompt le flux pour l'état `IBL_STATE_DONE` **que si** la barrière a déjà été exécutée.

```c
IBLState ibl_coordinator_update(IBLCoordinator* coord, uint64_t frame_count)
{
	/* Garde d'entrée : n'évite l'exécution que si IDLE, ou si DONE a déjà émis sa barrière mémoire */
	if (coord->state == IBL_STATE_IDLE || (coord->state == IBL_STATE_DONE && coord->barrier_executed)) {
		return coord->state;
	}

	unsigned long long frame = (unsigned long long)frame_count;

	switch (coord->state) {
		case IBL_STATE_LUMINANCE:
			coord->state = process_luminance(coord, frame);
			break;

		case IBL_STATE_LUMINANCE_WAIT:
			coord->state = process_luminance_wait(coord, frame);
			break;

		case IBL_STATE_SPECULAR_INIT:
			coord->state = process_specular_init(coord, frame);
			break;

		case IBL_STATE_SPECULAR_MIPS:
			coord->state = process_specular_mips(coord, frame);
			break;

		case IBL_STATE_IRRADIANCE:
			coord->state = process_irradiance(coord, frame);
			break;

		case IBL_STATE_DONE:
			if (!coord->barrier_executed) {
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				coord->barrier_executed = true;
				LOG_INFO("suckless-ogl.ibl", "[Frame %llu] IBL Memory barrier executed.", frame);
			}
			break;

		default:
			break;
	}

	return coord->state;
}
```

Grâce à cette architecture :
* Lors du passage de `IBL_STATE_IRRADIANCE` à `IBL_STATE_DONE`, le prochain appel à `ibl_coordinator_update` entrera dans `case IBL_STATE_DONE:`, déclenchera la barrière mémoire GPU exactement une fois, et positionnera `barrier_executed` à `true`.
* Aux frames suivantes, la garde d'entrée interceptera l'appel immédiatement, empêchant toute réévaluation de la barrière mémoire GPU et éliminant tout risque de stuttering ou de boucle infinie.

---

## 3. Matrice de Conformité avec la Constitution

| Règle de la Constitution | Proposition Technique | Validation de conformité |
| :--- | :--- | :--- |
| **Domaine I, Règle 10** (Point de sortie unique `cleanup`) | Restructuration de `scene_init` avec une unique étiquette `cleanup:` et des sauts par `goto cleanup;`. | Conforme. Plus aucun `return 0` anticipé après allocation. |
| **Domaine I, Règle 11** (Pas de free en cascade) | Centralisation de la libération dans `scene_cleanup` appelée depuis le bloc `cleanup:` local de `scene_init`. | Conforme. Élimine les cascades de `free()` ad-hoc. |
| **Domaine I, Règle 12** (Initialisation à NULL) | Déclaration avec initialisation à `NULL` de toutes les variables locales de ressources et mise à `NULL` après libération dans `scene_cleanup`. | Conforme. Sécurise les `free(NULL)` successifs et l'idempotence. |
| **Domaine I, Règle 13** (Pas de multiplication de labels) | Un unique label `cleanup:` dans `scene_init`. | Conforme. |
| **Domaine III, Règle 62** (Cycle d'états de l'IblCoordinator) | Préservation de la séquence d'états stricte : `IBL_STATE_IRRADIANCE -> IBL_STATE_DONE -> IBL_STATE_IDLE`. | Conforme. Pas d'introduction d'état de transition illicite. |
| **Domaine III, Règle 67** (Unique barrière OpenGL à DONE) | La barrière mémoire GPU `glMemoryBarrier` est appelée exclusivement à l'état `IBL_STATE_DONE` via le switch restructuré. | Conforme. Interdiction formelle d'insérer des barrières à la fin de chaque tranche. |

---

## 4. Découpage en Tâches Atomiques pour les Développeurs C11

### Tâche 1 : Modification de l'en-tête `ibl_coordinator.h`
* **Action** : Ajouter le membre `bool barrier_executed;` à la fin de la structure `IBLCoordinator`.
* **Vérification** : Pas d'avertissement de compilation, alignement de la structure préservé.

### Tâche 2 : Restructuration de `ibl_coordinator.c`
* **Action** :
  1. Initialiser `coord->barrier_executed = false;` dans `ibl_coordinator_reset` et `ibl_coordinator_init`.
  2. Adapter la garde d'entrée de `ibl_coordinator_update` pour laisser passer l'état `IBL_STATE_DONE` si `barrier_executed` est faux.
  3. Ajouter la logique de barrière et de mise à jour du flag dans `case IBL_STATE_DONE`.
* **Vérification** : La couverture de code de `case IBL_STATE_DONE` doit passer à 100% lors des tests d'environnement.

### Tâche 3 : Sécurisation défensive de `scene_cleanup.c`
* **Action** : Modifier toutes les fonctions internes de nettoyage de `scene_cleanup.c` pour ajouter des filtres de garde vérifiant que `scene`, `scene->gpu` ou `scene->shaders` ne sont pas nuls avant d'accéder à leurs membres. S'assurer de la remise à `NULL`/`0` de tous les handles.
* **Vérification** : Valider avec un test injectant des structures `Scene` vides ou partiellement nulles.

### Tâche 4 : Transition de `scene_init.c` vers le point de sortie unique
* **Action** : Réécrire la structure de contrôle de la fonction `scene_init` pour remplacer chaque cas d'erreur par un `goto cleanup;`. Placer l'appel `scene_cleanup(scene)` dans le bloc final conditionné par `!success`.
* **Vérification** : Exécuter la suite de tests sous Valgrind et ASan en forçant des échecs d'allocation simulés pour vérifier qu'aucune fuite de RAM/VRAM ne subsiste.
