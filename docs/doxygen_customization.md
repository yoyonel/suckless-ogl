# Doxygen Customization & Modernization

This document details the custom modifications made to the Doxygen setup to achieve a modern, high-performance visual style consistent with the **Suckless-OGL** philosophy.

---

## 🎨 Theme: Doxygen-Awesome

We use the [Doxygen-Awesome](https://github.com/jothepro/doxygen-awesome-css) library to replace the legacy Doxygen look with a modern, responsive interface.

### Integrated Components
- **Dark Mode**: Toggle situated in the sidebar.
- **Interactive TOC**: Floating table of contents for long pages.
- **Copy Button**: One-click code extraction from fragments.
- **Tabs**: Support for tabbed content in documentation.

---

## 💻 Advanced Syntax Highlighting

Doxygen's default highlighting is limited for modern graphics programming. We've integrated [highlight.js](https://highlightjs.org/) with specialized logic to handle our shaders and C code.

### 1. Shader Support
The `Doxyfile` maps graphics-specific extensions to C++ for basic indexing, while `highlight.js` handles the visual coloring:
```properties
EXTENSION_MAPPING = glsl=C++ vert=C++ frag=C++ comp=C++
```

### 2. Indentation Preservation Fix
Doxygen encodes code blocks as a series of `<div class="line">` elements, which often breaks standard syntax highlighters. We've added a custom JavaScript orchestrator in `header.html` that:
1.  **Extracts** the raw text from all `.line` divs.
2.  **Reconstructs** a standard `pre` > `code` block.
3.  **Preserves** original indentation and whitespace.
4.  **Triggers** `highlight.js` on the newly created block.

**Theme**: `Tokyo Night Dark` (consistent with the wider project ecosystem).

---

## 📊 "Suckless-Modern Ghost" Diagrams

Our Graphviz (DOT) diagrams have been overhauled to blend seamlessly with the dark mode documentation.

### 1. Engine Configuration
Defined in `Doxyfile`:
- **Format**: `svg` (for crisp zoom and interactivity).
- **Transparency**: `DOT_TRANSPARENT = YES` (removes harsh white/gray backgrounds).

### 2. Design Tokens
Each diagram follows a strict "Ghost" style to ensure it feels like a native part of the page:
- **Nodes**: `shape=rect, style="rounded"`. No fill color (`fillcolor="none"`) for a "see-through" aesthetic.
- **Borders**: Vivid **Tokyo Night** colors with enhanced `penwidth=2`.
- **Edges**: Refined grays (`#565f89`) with subtle arrowheads.

**Example Token usage:**
```properties
node [style="rounded", fillcolor="none", color="#414868", fontcolor="#c0caf5", penwidth=2];
```

---

## 🔧 Core Configuration (Doxyfile)

| Option | Value | Purpose |
| :--- | :--- | :--- |
| `OPTIMIZE_OUTPUT_FOR_C` | `YES` | Adjusts terminology for C (e.g., "Files" vs "Classes"). |
| `GENERATE_TREEVIEW` | `YES` | Enables the interactive sidebar navigation. |
| `CALL_GRAPH` | `YES` | Automatically generates function call dependency diagrams. |
| `INTERACTIVE_SVG` | `YES` | Allows panning and zooming in generated diagrams. |

---

## 🛠️ Maintenance

If you need to update the theme:
1.  Styles are located in `docs/doxygen-awesome-css/`.
2.  Global JS logic (including the highlighting fix) is in `docs/header.html`.
3.  Rebuild the documentation with `make docs`.
