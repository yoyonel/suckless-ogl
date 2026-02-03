#!/usr/bin/env python3
r"""
Verify Doxygen Generation Quality
---------------------------------
This script performs two checks:
1. Diagram Verification: Ensures \dot or \startuml blocks are rendered as SVGs.
2. HTML Tag Sanitization: Ensures literal HTML tags like <strong> or <tt>
   do not appear in navigation data or as escaped text in content (indicating parsing issues).

Usage:
    python3 scripts/verify_docs.py [docs_dir] [html_dir]
"""

import os
import re
import sys

# Default paths
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS_DIR = os.path.join(PROJECT_ROOT, "docs")
HTML_DIR = os.path.join(DOCS_DIR, "doxygen", "html")

# Literal tags are forbidden in JS (navigation)
FORBIDDEN_RAW_TAGS = [
    r'<strong>', r'</strong>',
    r'<tt>', r'</tt>',
    r'<b>', r'</b>',
    r'<code>', r'</code>',
    r'<blockquote>', r'</blockquote>'
]

# Escaped tags are forbidden in HTML (indicating failure to parse Markdown)
# We use a regex to catch any &lt;/?[a-z]+&gt; pattern
ESCAPED_TAG_PATTERN = re.compile(r'&lt;/?([a-z1-6]+)&gt;', re.IGNORECASE)

# Some escaped sequences are actually intended (e.g. in code blocks or examples)
# but usually Doxygen renders them inside <div class="fragment"> or <code> tags.
# If they appear in plain text or headers, they are likely bugs.
# For now, we flag them all and we can exclude specific false positives if needed.

def get_files_with_diagrams(docs_dir):
    r"""Scans markdown files for \dot or \startuml blocks."""
    files_with_diagrams = {}
    for root, dirs, filenames in os.walk(docs_dir):
        if "doxygen" in root: continue
        for f in filenames:
            if not f.endswith('.md'): continue
            path = os.path.join(root, f)
            try:
                with open(path, 'r', encoding='utf-8') as fd:
                    content = fd.read()
                    dot_count = len(re.findall(r'\\dot', content))
                    uml_count = len(re.findall(r'\\startuml', content))
                    # Support for modern markdown fences (MkDocs/Kroki)
                    graphviz_count = len(re.findall(r'```graphviz', content))
                    mermaid_count = len(re.findall(r'```mermaid', content))
                    total = dot_count + uml_count + graphviz_count + mermaid_count
                    if total > 0:
                        files_with_diagrams[f] = total
            except Exception as e:
                print(f"Error reading {f}: {e}")
    return files_with_diagrams

def find_html_file(md_filename, html_dir):
    """Attempts to find the generated HTML file for a given Markdown file."""
    stem = os.path.splitext(md_filename)[0]
    escaped_stem = stem.replace('_', '__').replace('-', '_')

    candidates = []
    if not os.path.exists(html_dir): return None

    for f in os.listdir(html_dir):
        if f.endswith(".html") and "md_" in f:
            if escaped_stem in f or stem.replace('_','__') in f:
                candidates.append(f)

    best_candidate = None
    for c in candidates:
        if c.endswith(escaped_stem + ".html") or c.endswith(escaped_stem + "_8md.html"):
            best_candidate = c
            break

    if not best_candidate and candidates:
        best_candidate = candidates[0]

    return os.path.join(html_dir, best_candidate) if best_candidate else None

def verify_diagrams(expected_files, html_dir):
    """Ensures diagrams are correctly rendered in HTML files."""
    errors = 0
    print(f"\n--- Phase 1: Diagram Verification ---")
    for md_file, count in expected_files.items():
        html_path = find_html_file(md_file, html_dir)
        if not html_path or not os.path.exists(html_path):
            # Many files are handled by MkDocs and don't have Doxygen HTML counterparts
            continue
        try:
            with open(html_path, 'r', encoding='utf-8') as fd:
                content = fd.read()
            svg_refs = re.findall(r'(dot_inline_dotgraph_\d+\.svg)', content)
            num_found = len(set(svg_refs))
            if num_found < count:
                print(f"⚠️  [ISSUE] {md_file}: Expected {count}, found {num_found} in {os.path.basename(html_path)}")
                errors += 1
            else:
                print(f"✅ [OK] {md_file}: {num_found}/{count} diagrams found.")
        except Exception as e:
            print(f"❌ [ERROR] {md_file}: {e}")
            errors += 1
    return errors

def verify_html_sanitization(html_dir):
    """Ensures no literal/escaped forbidden tags appear in output files."""
    errors = 0
    print(f"\n--- Phase 2: HTML Tag Sanitization ---")

    files_to_check = []
    for root, _, filenames in os.walk(html_dir):
        for f in filenames:
            if f.endswith(('.html', '.js')):
                # Ignore external project documentation
                if 'doxygen-awesome-css' in root or 'md_docs_2doxygen-awesome-css' in f:
                    continue
                files_to_check.append(os.path.join(root, f))

    raw_patterns = [re.compile(p) for p in FORBIDDEN_RAW_TAGS]

    for path in files_to_check:
        try:
            with open(path, 'r', encoding='utf-8') as fd:
                content = fd.read()

            rel_path = os.path.relpath(path, html_dir)

            # Case A: JS files (navigation) - we don't want ANY literal tags
            if path.endswith('.js'):
                for p in raw_patterns:
                    matches = p.findall(content)
                    if matches:
                        print(f"❌ [FORBIDDEN RAW TAG IN JS] {rel_path}: Found {len(matches)} occurrences of '{matches[0]}'")
                        errors += 1
                        break

            # Case B: HTML files - detect escaped tags &lt;tag&gt;
            elif path.endswith('.html'):
                escaped_matches = ESCAPED_TAG_PATTERN.findall(content)
                if escaped_matches:
                    # Filter out some false positives if they are inside <div class="fragment">
                    # but for a strict suckless project, we want no escaped tags in normal text.
                    # Especially not things like blockquote, strong, h1-6.
                    for tag in escaped_matches:
                        if tag.lower() in ['blockquote', 'strong', 'tt', 'b', 'code', 'h1', 'h2', 'h3']:
                            print(f"❌ [FORBIDDEN ESCAPED TAG] {rel_path}: Found '&lt;{tag}&gt;'")
                            errors += 1
                            break # One error per file is enough

        except Exception as e:
            print(f"❌ [ERROR] Reading {path}: {e}")
            errors += 1

    if errors == 0:
        print("✅ [OK] No unrendered/escaped HTML tags found in output files.")

    return errors

if __name__ == "__main__":
    if len(sys.argv) > 1: DOCS_DIR = sys.argv[1]
    if len(sys.argv) > 2: HTML_DIR = sys.argv[2]

    if not os.path.exists(HTML_DIR):
        print(f"Error: HTML directory {HTML_DIR} does not exist. Run doxygen first.")
        sys.exit(1)

    total_errors = 0
    total_errors += verify_diagrams(get_files_with_diagrams(DOCS_DIR), HTML_DIR)
    total_errors += verify_html_sanitization(HTML_DIR)

    print("\n" + "="*40)
    print(f"Final Report: {total_errors} errors found.")
    sys.exit(0 if total_errors == 0 else 1)
