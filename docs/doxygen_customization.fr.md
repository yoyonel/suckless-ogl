# Personnalisation Doxygen

Ce document décrit les personnalisations apportées à la configuration Doxygen du projet.

## Thème Doxygen Awesome

Le projet utilise le thème [doxygen-awesome-css](https://jothepro.github.io/doxygen-awesome-css/) pour une apparence moderne et lisible.

### Configuration dans `Doxyfile`

```
HTML_EXTRA_STYLESHEET = doxygen_resources/doxygen-awesome.css
HTML_EXTRA_FILES       = docs/doxygen-awesome-darkmode-toggle.js
HTML_HEADER            = docs/header.html
```

### Mode sombre

Le toggle de mode sombre est injecté via `header.html` et le script JavaScript `doxygen-awesome-darkmode-toggle.js`. La préférence est conservée dans `localStorage`.

## Support highlight.js pour les shaders

Doxygen ne supporte pas nativement la coloration syntaxique GLSL. Nous injectons **highlight.js** pour combler cette lacune :

```html
<!-- Dans header.html -->
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.x/highlight.min.js"></script>
<script>
document.addEventListener('DOMContentLoaded', () => {
    document.querySelectorAll('pre code').forEach(block => {
        hljs.highlightElement(block);
    });
});
</script>
```

Les langages GLSL, HLSL et C sont supportés.

## Filtre mathématique « Ghost Filter »

Les équations LaTeX dans les commentaires Doxygen sont rendues via MathJax, mais nécessitent un pré-traitement pour convertir la syntaxe `$$...$$` en syntaxe Doxygen `\f[...\f]`.

Le filtre `scripts/doxygen_math_filter.py` effectue cette conversion :

```python
# Convertir $$...$$ → \f[...\f] pour Doxygen
content = re.sub(r'\$\$(.*?)\$\$', r'\\f[\1\\f]', content, flags=re.DOTALL)
content = re.sub(r'\$(.*?)\$', r'\\f$\1\\f$', content)
```

Dans `Doxyfile` :

```
INPUT_FILTER = "python3 scripts/doxygen_math_filter.py"
```

## Configuration principale

Les paramètres clés dans `Doxyfile` :

| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| `EXTRACT_ALL` | `YES` | Documenter tous les symboles |
| `RECURSIVE` | `YES` | Parcourir les sous-répertoires |
| `GENERATE_HTML` | `YES` | Sortie HTML |
| `GENERATE_LATEX` | `NO` | Pas de PDF LaTeX |
| `USE_MATHJAX` | `YES` | Équations mathématiques |
| `MATHJAX_VERSION` | `MathJax_3` | Version MathJax 3.x |

## Intégration MkDocs

La documentation Doxygen est générée dans `site/doxygen/` et intégrée à la documentation MkDocs principale via un iframe ou un lien direct.

```bash
# Générer la documentation complète (MkDocs + Doxygen)
just docs
```

## Voir aussi

- [documentation_system.md](./documentation_system.md) — Vue d'ensemble du système de documentation
