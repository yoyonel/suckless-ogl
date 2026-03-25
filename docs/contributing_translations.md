# Contributing Translations

This guide explains how to contribute a translation of the Suckless OGL documentation, how to submit it for review, and how it gets validated and integrated by the maintainer.

---

## How It Works

This documentation uses [mkdocs-static-i18n](https://ultrabug.github.io/mkdocs-static-i18n/) to support multiple languages. The system is **suffix-based**:

| File | Language |
|------|----------|
| `docs/page.md` | English (default) |
| `docs/page.fr.md` | French |
| `docs/page.de.md` | German |
| `docs/page.es.md` | Spanish |

When a user selects a language in the site, MkDocs automatically serves the translated version if it exists, or falls back to English.

---

## Adding a New Language

> [!NOTE]
> Adding a new language requires a small configuration change in `mkdocs.yml`. This is done by the maintainer after at least **one translated page** has been reviewed and accepted.

To propose a new language, open an issue titled:
```
[i18n] Request: Add <language name> (<ISO 639-1 code>) support
```

Once approved, the maintainer adds the locale to `mkdocs.yml`:

```yaml
plugins:
  - i18n:
      languages:
        - locale: de          # ISO 639-1 code
          name: Deutsch
          build: true
          nav_translations:
            Home: Startseite
            Architecture: Architektur
            # ... other nav items
```

---

## Language Switcher UI

The header language button is driven by two files that require **no changes** when adding a new language:

### How it works

```
document.documentElement.lang  →  "fr"
         ↓ lang_switcher.js
button[data-lang="FR"]          →  CSS content: attr(data-lang)  →  "FR"
```

1. **[`docs/assets/javascripts/lang_switcher.js`](../assets/javascripts/lang_switcher.js)** — runs at `DOMContentLoaded`, reads `document.documentElement.lang` (set automatically by MkDocs for every page), uppercases and truncates it to 2 characters, then sets `data-lang="XX"` on the language button.

2. **[`docs/assets/stylesheets/extra.css`](../assets/stylesheets/extra.css)** — hides the default SVG globe icon and uses a single generic CSS rule:

```css
.md-header__option .md-select > button.md-header__button[data-lang]::before {
  content: attr(data-lang);
}
```

The active language is also highlighted in the dropdown with a `✓` using the CSS `:lang()` pseudo-class.

### Adding a new language

No CSS or JS changes needed. Simply add the locale to `mkdocs.yml`:

```yaml
plugins:
  - i18n:
      languages:
        - locale: de
          name: Deutsch
          build: true
```

The button will automatically display `DE` when browsing German pages.

---

## Submitting a Translation

### Step 1 — Fork and Branch

```bash
git clone https://github.com/yoyonel/suckless-ogl.git
cd suckless-ogl
git checkout -b i18n/fr/banding-effect    # use: i18n/<locale>/<page-slug>
```

### Step 2 — Create the Translation File

Copy the English source and translate it:

```bash
cp docs/banding.md docs/banding.fr.md
# Edit docs/banding.fr.md with your translation
```

**What to translate — prose text only:**

| Content type | Translate? | Notes |
|---|---|---|
| Headings, paragraphs | **Yes** | Core translation work |
| Table cell text (descriptions) | **Yes** | |
| Admonition body text | **Yes** | |
| Code block **comments** | Optional | May help readers |
| **Code block bodies** | **No** | Copy as-is from English |
| **Math equations (`$$...$$`)** | **No** | Language-agnostic — copy unchanged |
| **Diagram labels that are identifiers** | **No** | e.g. `rankdir`, `fillcolor`, `shape` |
| Diagram labels that are **natural language** | **Yes** | e.g. `label="Render Pass"` → `label="Passe de rendu"` |
| Technical terms (function names, API names) | **No** | e.g. `glDispatchCompute`, `PostProcessUBO` |
| File paths | **No** | |

> [!TIP]
> A quick way to work: search for every occurrence of accented or locale-specific characters in the English source — if there are none, there is very little to translate beyond headings and paragraph text.

**Practical example** — this block is copied unchanged:

```markdown
$$
\text{Form Factor (FF)} = \frac{r^2}{d^2}
$$
```

Only this kind of block needs translation:

```markdown
<!-- English: -->
The goal is to capture diffuse color transfer between scene objects.

<!-- French: -->
L'objectif est de capturer le transfert de couleur diffuse entre les objets de la scène.
```

---

## Keeping Translations Up to Date

When the English source (`page.md`) is updated, the corresponding translation file (`page.fr.md`) may become **stale**. A MkDocs hook (`hooks/check_translation_staleness.py`) automatically detects this and injects a warning banner into the translated page:

> ⚠️ **This translation may be outdated.** The English source has been updated more recently than this page.

### What triggers a staleness warning?

The hook compares:
1. The **git commit timestamp** of `page.md` (last time the English source was committed)
2. The **git commit timestamp** of `page.fr.md` (last time the translation was committed)

If `page.md` is more than 60 seconds newer than `page.fr.md`, the warning is shown.

### As a maintainer — when should I update translations?

- **Math equations changed** → update the equation in all `.XX.md` files (copy-paste)
- **Code block changed** → copy-paste the updated block into all translation files
- **Prose text changed** → update the corresponding paragraph in each translation
- **New section added** → add the section translated (or in English as placeholder) to each translation file

> [!NOTE]
> Language-agnostic content (equations, code, diagrams) is **always copy-pasted** from the English source. Translations only add prose text. This means most English-source updates only require small targeted patches in translation files.

### Step 3 — Verify Locally

```bash
# In the project root:
just docs-serve
# or: python3 -m mkdocs serve
```

Navigate to `http://localhost:8000/fr/<page>/` to preview your translation.

### Step 4 — Open a Pull Request

Open a PR with:

- **Title**: `i18n(fr): Translate banding.md to French` *(use [Conventional Commits](https://www.conventionalcommits.org/) format)*
- **Labels**: `translation`, `lang:fr` *(or your locale)*
- **Description**: Use the translation PR template (`.github/PULL_REQUEST_TEMPLATE/translation.md` in the repository)

---

## Review Process

1. **Automated checks** — The CI verifies that `mkdocs build` succeeds without errors.
2. **Maintainer review** — The maintainer (`@yoyonel`) reviews the translation for:
   - Correctness and fidelity to the English source
   - Absence of untranslated paragraphs
   - Proper Markdown structure
   - No introduced broken links
3. **Approval** — Once approved, the PR is merged and the translation is live on the next documentation deployment.

> [!IMPORTANT]
> All translation PRs require **explicit approval from the maintainer** before merge. Automated CI passing alone is not sufficient.

---

## Translation Status

The table below tracks the current translation coverage per language.

> [!NOTE]
> This table is updated manually by the maintainer when translations are merged.

### French (fr)

| Page | Status |
|------|--------|
| `banding.md` | ✅ Translated |
| `glossary.md` | ✅ Translated |
| `memory_management_audit.md` | ✅ Translated |
| `standalone_testing_mocking.md` | ✅ Translated |
| `pr_evaluation_report.md` | ✅ Translated |
| `postprocess_optimizations_2026-02.md` | ✅ Translated |
| `postprocess_optimizations_2026-03.md` | ✅ Translated |
| `motion_blur_analysis.md` | ✅ Translated |
| `global_illumination.md` | ✅ Translated |
| `effect_benchmark.md` | ✅ Translated |
| `documentation_system.md` | ✅ Translated |
| All other pages | ⬜ Not yet translated |

---

## File Naming Convention

| Locale | ISO 639-1 | File suffix |
|--------|-----------|-------------|
| English | `en` | `.md` (default) |
| French | `fr` | `.fr.md` |
| German | `de` | `.de.md` |
| Spanish | `es` | `.es.md` |
| Portuguese | `pt` | `.pt.md` |
| Japanese | `ja` | `.ja.md` |

Use the [ISO 639-1 two-letter code](https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes) for any new language.

---

## Questions?

Open a [GitHub Discussion](https://github.com/yoyonel/suckless-ogl/discussions) or an issue tagged `translation`.
