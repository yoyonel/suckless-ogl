# Documentation System Architecture

This project uses a **Hybrid Documentation Model** that combines the best of both worlds:

1. **MkDocs**: For narrative documentation, guides, and tutorials.
2. **Doxygen**: For auto-generated API reference, dependency graphs, and call hierarchies.

## 🏗️ Architecture

The build system (`make docs`) generates both sites and merges them into a single static `site/` directory.

| Component | Tool | Config File | Output Path |
| :--- | :--- | :--- | :--- |
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

We follow a **zero external dependency** philosophy. Documentation must be buildable and viewable without an internet connection.

### Graphviz (Local-Only)

Rather than using online services, we use a custom Python hook (`hooks/render_graphviz.py`) that invokes the local `dot` binary directly:

1. **Local Rendering**: The hook intercepts `graphviz` blocks and calls `/usr/bin/dot`.
2. **Caching**: Generated SVGs are stored in `docs/assets/diagrams/` to avoid redundant renders.
3. **Zero Latency**: No network requests, instant and private rendering.

> [!TIP]
> To verify the build is truly 100% offline, use:
>
> ```bash
> make docs-offline  # or 'just docs-offline'
> ```
>
> This command uses `unshare -rn` to fully isolate the process from the network. These targets automatically chain `docs-clean` and `docs` to guarantee a fresh render from scratch without internet.

> [!IMPORTANT]
> `graphviz` must be installed on the build machine to generate these diagrams. If `dot` is absent, the MkDocs build will explicitly fail via a `RuntimeError`.

### Mermaid (Browser-Side)

Mermaid diagrams are rendered directly in the browser via `mermaid.js`. This guarantees that rendering always matches the bundled script version, without depending on a third-party server or complex pre-generation.

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
