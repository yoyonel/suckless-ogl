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
