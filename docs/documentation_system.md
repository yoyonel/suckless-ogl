# Documentation System Architecture

This project uses a **Hybrid Documentation Model** that combines the best of both worlds:

1. **MkDocs**: For narrative documentation, guides, and tutorials.

2. **Doxygen**: For auto-generated API reference, dependency graphs, and call hierarchies.

## 🏗️ Architecture

The build system (`make docs`) generates both sites and merges them into a single static `site/` directory.

| Component | Tool | Config File | Output Path |
| :-------- | :--- | :---------- | :---------- |

| **Guides & UI** | [MkDocs](https://www.mkdocs.org/) | `mkdocs.yml` | `site/` |

| **API Reference** | [Doxygen](https://www.doxygen.nl/) | `Doxyfile` | `site/doxygen/` |

### Integration

- **MkDocs** creates the main website structure and navigation.

- **Doxygen** runs as a subprocess and outputs its HTML to a subdirectory.

- A **Custom Layout** (`docs/DoxygenLayout.xml`) adds a "Back to Docs" link in the Doxygen sidebar to ensure seamless navigation between the two.

## 🛠️ Tools & Libraries

### 1. MkDocs (The Core)

We use the **[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)** theme for its modern aesthetics and mobile responsiveness.

- **Configuration**: `mkdocs.yml`

- **Key Plugins**:
  - `mkdocs-kroki-plugin`: Renders diagrams (GraphViz, Mermaid, etc.).

  - `pymdown-extensions`: Adds advanced Markdown features (Admonitions, Tabbed code blocks).

  - [**MkDocs Hooks**](https://www.mkdocs.org/user-guide/configuration/#hooks): Custom Python scripts to extend MkDocs behavior.

### 2. Doxygen (The API)

We use the **[Doxygen Awesome CSS](https://github.com/jothepro/doxygen-awesome-css)** theme to make standard Doxygen output look modern and match the MkDocs style.

- **Configuration**: `Doxyfile`

- **Customization**:
  - `HTML_EXTRA_STYLESHEET` points to the `doxygen-awesome-css` files in `docs/doxygen-awesome-css/`.

  - `USE_MDFILE_AS_MAINPAGE` is set to `docs/api_index.md` to provide a clean landing page.

  - **Math Compatibility**: A Python input filter (`math_filter.py`) automatically translates standard Markdown math for Doxygen. See [Doxygen Customization](doxygen_customization.md) for details.

## 🚀 Workflow

### Local Development

To preview the full hybrid site locally:

```sh
make docs-serve

# Opens http://localhost:8000

```

> **Note**: `make docs-dev` (or `mkdocs serve`) only previews the MkDocs part and will NOT show the API reference.

### Deployment (CI/CD)

The GitHub Actions workflow (`.github/workflows/docs.yml`) automates the deployment:

1. Installs system deps (`doxygen`, `graphviz`).

2. Builds MkDocs (`mkdocs build`).

3. Builds Doxygen (`doxygen Doxyfile`).

4. Deploys the combined `site/` folder to the `gh-pages` branch.

## 🎨 Customization & Themes

### Updating the MkDocs Theme

To change the MkDocs look (e.g., colors, fonts), edit `mkdocs.yml`:

```yaml
theme:
  name: material
  palette:
    primary: indigo # Change this to 'red', 'blue', 'teal', etc.

    accent: deep purple
```

To switch to a completely different MkDocs theme (e.g., `readthedocs`):

1. Install the theme (e.g., `pip install mkdocs-readthedocs-theme`).

2. Change `name: material` to `name: readthedocs` in `mkdocs.yml`.

### Updating the Doxygen Theme

To change the Doxygen look, you generally need to replace the CSS files.

1. **Doxygen Awesome**: We include this as a git submodule (or vendored files) in `docs/doxygen-awesome-css`. To update it, `git pull` in that directory.

2. **Switching Themes**:
   - Comment out the `HTML_EXTRA_STYLESHEET` lines in `Doxyfile` to revert to default Doxygen style.

   - Or download a new theme CSS, place it in `docs/`, and update `HTML_EXTRA_STYLESHEET`.

## 🤖 Automated Diagram Indexing

To maintain a high-level overview of the engine's architecture, we use a custom **MkDocs Hook** to automatically index all [Mermaid](https://mermaid.js.org/) diagrams.

### `generate_diagram_index.py`

This script ([hooks/generate_diagram_index.py](file:///home/latty/Prog/__PERSO__/suckless-ogl/hooks/generate_diagram_index.py)) runs during the `on_config` event of the MkDocs build process.

- **Scanning**: It parses all Markdown files in the `docs/` directory.

- **Extraction**: It extracts `mermaid` code blocks along with their preceding paragraph as a description.

- **Generation**: It creates [diagram_index.md](file:///home/latty/Prog/__PERSO__/suckless-ogl/docs/diagram_index.md).

- **Interactive UI**: The index features a "Tokyo Night" themed CSS hover effect that reveals a live preview of the diagram without leaving the page.

### Configuration

The hook is registered in `mkdocs.yml`:

```yaml
hooks:
  - hooks/generate_diagram_index.py

  - hooks/render_graphviz.py
```

## 📊 Diagram Rendering (Suckless & Offline-First)

Nous suivons une philosophie **zéro-dépendance externe**. La documentation doit pouvoir être générée et consultée sans connexion internet.

### Graphviz (Local-Only)

Plutôt que d'utiliser des services en ligne, nous utilisons un hook Python personnalisé ([hooks/render_graphviz.py](file:///home/latty/Prog/__PERSO__/suckless-ogl/hooks/render_graphviz.py)) qui invoque directement le binaire `dot` local :

1. **Rendu Local** : Le hook intercepte les blocs `graphviz` et appelle `/usr/bin/dot`.

2. **Mise en Cache** : Les SVGs générés sont stockés dans `docs/assets/diagrams/` pour éviter des rendus inutiles.

3. **Zéro Latence** : Pas de requêtes réseau, rendu instantané et privé.

> [!TIP]
> Pour vérifier que le build est réellement 100% offline, utilisez la commande :
>
> ```bash
> make docs-offline  # ou 'just docs-offline'
> ```
>
> Cette commande utilise `unshare -rn` pour isoler totalement le processus du réseau. Ces cibles enchaînent automatiquement `docs-clean` et `docs` pour garantir un rendu à partir de zéro sans internet.
>
> [!IMPORTANT]
> L'installation de `graphviz` est requise sur la machine de build pour générer ces diagrammes. Si `dot` est absent, le build MkDocs échouera explicitement via une `RuntimeError`.

### Mermaid (Browser-Side)

Les diagrammes Mermaid sont rendus directement par le navigateur via `mermaid.js`. Cela garantit que le rendu est toujours fidèle à la version du script embarquée, sans dépendre d'un serveur tiers ni de pré-génération complexe.

## 🧪 Documentation Integrity

We enforce documentation health through automated tests via `scripts/verify_docs.py`:

- **Phase 1: Doxygen Diagrams**: Ensures `\dot` blocks are rendered as SVGs.

- **Phase 2: HTML Sanitization**: Prevents raw HTML tags from appearing in navigation or escaped content.

- **Phase 3: MkDocs Rendering**: Scans for unrendered Mermaid/Graphviz blocks in the static `site/` output.

- **Link Validation**: [test_docs_links.py](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/test_docs_links.py) ensures no broken internal links.

## 🔗 Official Documentation Links

- [MkDocs User Guide](https://www.mkdocs.org/user-guide/)

- [MkDocs Hooks API](https://www.mkdocs.org/dev-guide/plugins/#events)

- [Material for MkDocs Reference](https://squidfunk.github.io/mkdocs-material/reference/)

- [Mermaid.js Documentation](https://mermaid.js.org/intro/)

- [Doxygen Manual](https://www.doxygen.nl/manual/index.html)
