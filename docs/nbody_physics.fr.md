# Simulation N-Corps — Référence Physique

## Vue d'ensemble

Le module N-corps (`nbody.h` / `nbody.c`) implémente une simulation
gravitationnelle temps réel de jusqu'à 16 corps en orbite autour d'une étoile
centrale.  Le moteur physique privilégie la **stabilité long terme**
(conservation de l'énergie sur des milliers de secondes simulées) en utilisant
des techniques éprouvées d'astrophysique numérique :

| Composant | Technique |
|-----------|-----------|
| Intégrateur | **Velocity Verlet** (symplectique, ordre 2) |
| Régularisation | **Adoucissement de Plummer** (par paire, adaptatif) |
| Conditions initiales | Formule de **vitesse orbitale adoucie** |
| Pas de temps | Fixe $\frac{1}{120}\text{s}$, accumulateur borné |

Validé par un test automatisé de 1 200 s avec **0.002 %** de dérive d'énergie.

---

## Intégrateur Velocity Verlet

[Velocity Verlet](https://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet)
(aussi appelé *Leapfrog kick-drift-kick*) est un intégrateur **symplectique** :
il préserve la structure géométrique des systèmes hamiltoniens, ce qui garantit
que l'énergie totale oscille autour de la vraie valeur au lieu de dériver
monotoniquement.

### Algorithme (par pas fixe $\Delta t$)

1. Calculer les accélérations à partir des positions courantes : $\mathbf{a}_\text{old}$
2. Mettre à jour les positions :
$$\mathbf{x} \leftarrow \mathbf{x} + \mathbf{v}\,\Delta t + \tfrac{1}{2}\,\mathbf{a}_\text{old}\,\Delta t^2$$
3. Calculer les accélérations à partir des nouvelles positions : $\mathbf{a}_\text{new}$
4. Mettre à jour les vitesses :
$$\mathbf{v} \leftarrow \mathbf{v} + \tfrac{1}{2}(\mathbf{a}_\text{old} + \mathbf{a}_\text{new})\,\Delta t$$

Implémenté dans `integrate_step()` dans `src/nbody.c`.

### Pourquoi le Symplectique est Important

Les méthodes non-symplectiques (Euler, RK4) accumulent une dérive séculaire
d'énergie proportionnelle au nombre de pas.  Sur 1 200 s à 120 Hz, cela
représente 144 000 pas — suffisant pour que même de petites erreurs par pas
fassent exploser ou effondrer le système.  L'erreur bornée d'un intégrateur
symplectique maintient les orbites qualitativement correctes indéfiniment.

**Références :**

- [Intégrateur symplectique — Wikipédia](https://fr.wikipedia.org/wiki/Int%C3%A9grateur_symplectique)
- [Intégration leapfrog — Wikipedia](https://en.wikipedia.org/wiki/Leapfrog_integration)
- Hairer, Lubich & Wanner, *Geometric Numerical Integration* (Springer, 2006)

---

## Adoucissement de Plummer

Dans un champ gravitationnel pur en $1/r^2$, deux masses ponctuelles passant
très près subissent des forces arbitrairement grandes, menant à des effets de
fronde numérique et des échanges chaotiques d'énergie.  Le remède standard en
astrophysique est l'[adoucissement de Plummer](https://en.wikipedia.org/wiki/Plummer_model) :

$$F = \frac{G\,m_i\,m_j}{(r^2 + \varepsilon^2)^{3/2}}\,\hat{\mathbf{r}}$$

La longueur d'adoucissement $\varepsilon$ régularise la singularité en $r = 0$,
transformant le potentiel newtonien $1/r$ en le potentiel fini de
[Plummer](https://en.wikipedia.org/wiki/Plummer_model)
$\Phi = -G\,M / \sqrt{r^2 + \varepsilon^2}$.

### Adoucissement Adaptatif par Paire

Plutôt qu'un seul $\varepsilon$ global, on adapte l'adoucissement aux rayons
physiques de chaque paire :

$$\varepsilon^2 = \max\!\bigl(\texttt{NBODY\_SOFTENING\_SQ},\;
      (\texttt{NBODY\_SOFTENING\_FACTOR} \cdot (r_i + r_j))^2\bigr)$$

Avec les constantes par défaut :

| Constante | Valeur | Rôle |
|-----------|--------|------|
| `NBODY_SOFTENING_SQ` | 0.25 | Plancher absolu ($\varepsilon_\min = 0.5$) |
| `NBODY_SOFTENING_FACTOR` | 2.0 | Multiplicateur par paire sur les rayons combinés |

Ceci garantit que :

- Les gros corps (l'étoile, $r = 1.5$) bénéficient d'un fort adoucissement ($\varepsilon \geq 3.6$).
- Les petites paires (deux lunes) en reçoivent proportionnellement moins,
  gardant leur dynamique plus képlérienne.
- Aucune paire n'atteint jamais la région singulière.

Implémenté dans `pair_softening_sq()`.

**Références :**

- [Adoucissement gravitationnel — Wikipedia](https://en.wikipedia.org/wiki/Softening)
- Dehnen & Aly, *Improving convergence of N-body methods* (MNRAS 425, 2012)
- Aarseth, *Gravitational N-Body Simulations* (Cambridge, 2003)

---

## Vitesse Orbitale Adoucie

Une erreur courante est d'initialiser les vitesses d'orbite circulaire avec la
formule de Kepler $v = \sqrt{G\,M/r}$.  C'est **faux** quand l'adoucissement
est actif car le potentiel effectif est plus plat que $1/r$.

La condition correcte d'orbite circulaire pour le potentiel de Plummer est :

$$v = r \cdot \sqrt{\frac{G\,M}{(r^2 + \varepsilon^2)^{3/2}}}$$

Elle vient de l'équilibre entre l'accélération centripète $v^2/r$ et
l'accélération gravitationnelle adoucie $G\,M\,r / (r^2 + \varepsilon^2)^{3/2}$.

Utiliser la formule non-adoucie surestime la vitesse orbitale, injectant
un excès d'énergie cinétique et causant la spirale vers l'extérieur des corps.

Implémenté dans `softened_orbital_vel()`.

---

## Annulation de la Quantité de Mouvement

Après l'initialisation de tous les corps, la quantité de mouvement totale
$\mathbf{p} = \sum m_i \mathbf{v}_i$ est mise à zéro en soustrayant la
vitesse du centre de masse de chaque corps :

$$\mathbf{v}_i \leftarrow \mathbf{v}_i - \frac{\sum m_j\,\mathbf{v}_j}{\sum m_j}$$

Cela maintient le centre de masse immobile et empêche le système entier de
dériver à travers la scène.  L'opération est faite **une seule fois** à
l'initialisation, jamais pendant la simulation (ce qui briserait la
symplecticité).

---

## Pas de Temps Fixe avec Accumulateur

Les pas de temps variables détruisent la propriété symplectique.  La simulation
utilise un $\Delta t = 1/120\,\text{s}$ fixe avec un motif d'accumulateur
standard :

```text
accumulateur += wall_dt × time_scale
borner accumulateur à NBODY_MAX_ACCUMULATOR (1/30 s)
tant que accumulateur >= NBODY_FIXED_DT :
    integrate_step(NBODY_FIXED_DT)
    accumulateur -= NBODY_FIXED_DT
```

Le bornage maximum de l'accumulateur empêche un emballement de l'intégration
après des pics de latence ou lors de la première frame.

---

## Approches Testées et Rejetées

Au cours du développement, plusieurs techniques alternatives ont été testées
et **rejetées** car elles brisaient l'invariant symplectique ou introduisaient
une dissipation d'énergie inacceptable.

### 1. Collisions Élastiques

**Idée :** Quand deux corps se chevauchent ($r < r_i + r_j$), refléter leurs
vitesses avec les formules de collision élastique.

**Problème :** La détection discrète de collision à pas de temps fixe introduit
des impulsions non-réversibles dans le temps.  Cela brise la symplecticité et
cause une dérive séculaire d'énergie.  En pratique, les corps gagnaient de
l'énergie à chaque collision, menant à une explosion exponentielle.

**Verdict :** Supprimé entièrement.  L'adoucissement de Plummer empêche le
chevauchement sans nécessiter de gestion des collisions.

### 2. Clamp d'Énergie

**Idée :** Après chaque pas, mesurer l'énergie totale ; si elle dépasse un
seuil, redimensionner toutes les vitesses pour restaurer l'énergie cible.

**Problème :** C'est une étape de projection qui ne dérive pas d'un
hamiltonien.  Elle détruit la structure symplectique et introduit un
amortissement ou un pompage artificiel selon le signe de la correction.

**Verdict :** Supprimé.

### 3. Frein d'Échappement

**Idée :** Si un corps dépasse un seuil de distance, appliquer une force de
traînée pour le ramener.

**Problème :** Force non-conservative → perte séculaire d'énergie → les
orbites décroissent et le système s'effondre.

**Verdict :** Supprimé.

### 4. Ré-annulation de la Quantité de Mouvement par Pas

**Idée :** Soustraire la vitesse du centre de masse de tous les corps à
chaque pas.

**Problème :** C'est une projection non-symplectique appliquée à chaque pas.
Elle interfère avec la structure de l'espace des phases de l'intégrateur et
cause une dérive descendante de l'énergie.

**Verdict :** La quantité de mouvement est annulée **une seule fois** à
l'initialisation, jamais pendant la simulation.

### 5. XPBD (Extended Position-Based Dynamics)

**Idée :** Utiliser le solveur de contraintes XPBD (de la physique de jeux
vidéo) pour imposer une distance minimale entre les corps, remplaçant
l'adoucissement par une répulsion basée sur des contraintes.

**Problème :** XPBD est **fondamentalement dissipatif par conception** — il
résout les contraintes en projetant les positions, ce qui ne conserve pas
l'énergie.  En test, cela a causé **21 % de perte d'énergie** sur 300 s,
les orbites ont décru rapidement et tous les corps se sont effondrés sur
l'étoile.

XPBD est excellent pour la physique de jeux (ragdolls, tissus) où la
dissipation est acceptable voire souhaitable, mais il est **catastrophique**
pour les simulations gravitationnelles qui nécessitent la conservation de
l'énergie.

**Verdict :** Rejeté après test.  Verlet pur + fort adoucissement de Plummer
est l'approche correcte pour la gravité N-corps.

**Références :**

- Müller et al., *Detailed Rigid Body Simulation with Extended Position Based Dynamics* (CGF 2020)
- [Position Based Dynamics — Wikipedia](https://en.wikipedia.org/wiki/Position-based_dynamics)

---

## Validation par Tests

La suite de tests automatisée (`tests/test_nbody_stability.c`) vérifie les
invariants physiques sur de longues simulations :

| Test | Ce qu'il vérifie | Seuil |
|------|-----------------|-------|
| `test_nbody_single_step_sanity` | Un pas n'explose pas | Positions finies |
| `test_nbody_energy_conservation` | Énergie bornée après boost initial | $\Delta E / E_0 < 5\%$ |
| `test_nbody_paused_no_change` | Sim en pause parfaitement gelée | Identique bit à bit |
| `test_nbody_survives_dt_spikes` | Les pics de frame ne déstabilisent pas | Tous les corps $< 50$ unités |
| `test_nbody_long_run_stability` | Invariants sur 1 200 s | Voir ci-dessous |

### Résultats de Stabilité Long Terme (1 200 s à 120 Hz = 144 000 pas)

| Métrique | Mesuré | Seuil |
|----------|--------|-------|
| Dérive d'énergie $\|\Delta E / E_0\|$ | 0.0017 % | < 5 % |
| Dérive du centre de masse | 0.001 | < 0.5 |
| Distance maximale d'un corps | 19.6 | < 50 |
| Nombre de corps | 7 / 7 | inchangé |

---

## Référence des Constantes

Toutes les constantes sont définies dans `include/nbody.h` :

| Constante | Valeur | Description |
|-----------|--------|-------------|
| `NBODY_MAX_BODIES` | 16 | Nombre maximum de corps |
| `NBODY_DEFAULT_G` | 1.0 | Constante gravitationnelle |
| `NBODY_SOFTENING_SQ` | 0.25 | $\varepsilon^2$ minimum |
| `NBODY_SOFTENING_FACTOR` | 2.0 | Multiplicateur d'adoucissement par paire |
| `NBODY_FIXED_DT` | 1/120 s | Pas de temps fixe d'intégration |
| `NBODY_MAX_ACCUMULATOR` | 1/30 s | Temps physique accumulé maximum |

---

## Contrôles en Temps Réel

### Vitesse de Simulation (`time_scale`)

La vitesse de simulation est ajustable au clavier :

| Touche (US) | Touche (AZERTY) | Action |
|-------------|-----------------|--------|
| `.` | `:` | Doubler la vitesse (max 64×) |
| `,` | `;` | Diviser par 2 (min 1/8×) |

Le multiplicateur `time_scale` suit des puissances de 2 pour garantir que
`1.0×` est toujours atteignable :

$$\text{time\_scale} \in \{0.125,\; 0.25,\; 0.5,\; 1.0,\; 2.0,\; 4.0,\; 8.0,\; 16.0,\; 32.0,\; 64.0\}$$

Une notification overlay affiche la vitesse actuelle à chaque changement.

### Stabilité de la Longueur des Traînées

L'accumulateur d'échantillonnage des traînées est modulé par `time_scale` :

```text
effective_dt = wall_dt × time_scale
sample_timer += effective_dt
tant que sample_timer >= TRAIL_SAMPLE_INTERVAL:
    enregistrer les positions
    sample_timer -= TRAIL_SAMPLE_INTERVAL
```

Cela garantit que le ring buffer se remplit et évacue les points au même
rythme *visuel* quel que soit `time_scale`. À 8×, 8 échantillons sont émis
par frame au lieu de 1, gardant la longueur des traînées constante en unités monde.

---

## Carte du Code

| Fichier | Rôle |
|---------|------|
| `include/nbody.h` | API publique, constantes, structures de données |
| `src/nbody.c` | Implémentation physique (Verlet, adoucissement, preset) |
| `tests/test_nbody_stability.c` | Suite de tests de stabilité (5 tests) |
| `include/trail_renderer.h` | Rendu des traînées (rubans visuels derrière les corps) |
| `src/trail_renderer.c` | Implémentation des traînées en rubans billboard |
| `src/app_input.c` | Raccourcis clavier vitesse de simulation (`,` / `.`) |
| `src/app_binding.c` | Enregistrement dans l'overlay d'aide F2 |
| `shaders/trail.vert` | Vertex shader des traînées |
| `shaders/trail.frag` | Fragment shader des traînées |
