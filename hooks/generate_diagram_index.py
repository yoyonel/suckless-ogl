import os
import re
import unicodedata
from mkdocs.plugins import event_priority

def slugify(text):
    text = unicodedata.normalize('NFD', text).encode('ascii', 'ignore').decode('utf-8')
    text = text.lower()
    text = text.replace("'", "")
    text = text.replace(".", "")
    text = re.sub(r'[^a-z0-9]+', '-', text)
    return text.strip('-')

def clean_mermaid_code(text):
    # 1. Clean up problematic chars and strip each line rigorously
    # We join with \n but we'll use a fenced block to avoid Markdown corruption
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    if not lines: return ""

    clean_text = "\n".join(lines)
    clean_text = clean_text.replace('💀', '(DEADLOCK)')
    clean_text = clean_text.replace('✓', '(OK)')
    clean_text = clean_text.replace('→', '->')
    clean_text = clean_text.replace('⏳', '(Wait)')
    clean_text = clean_text.replace('—', '-')

    return clean_text

@event_priority(100)
def on_config(config):
    docs_dir = config['docs_dir']
    use_directory_urls = config.get('use_directory_urls', True)
    output_file = os.path.join(docs_dir, "diagram_index.md")

    all_files = sorted([f for f in os.listdir(docs_dir) if f.endswith('.md') and not f.endswith('.fr.md') and f != "diagram_index.md"])

    content = "# Diagram Index\n\n"
    content += "*This page is auto-generated. **Hover over the titles** to preview the diagram.*\n\n"

    content += '<style>\n'
    content += '.diagram-item { position: relative; display: block; padding: 12px 0; border-bottom: 1px solid var(--md-code-bg-color); }\n'
    content += '.mermaid-preview { \n'
    content += '  opacity: 0; \n'
    content += '  visibility: hidden; \n'
    content += '  position: absolute; \n'
    content += '  left: max(300px, 30%); \n'
    content += '  top: -80px; \n'
    content += '  z-index: 999; \n'
    content += '  background: #1a1b26; \n'
    content += '  border: 2px solid #7aa2f7; \n'
    content += '  padding: 24px; \n'
    content += '  border-radius: 12px; \n'
    content += '  box-shadow: 0 15px 55px rgba(0,0,0,0.9); \n'
    content += '  width: 750px; \n'
    content += '  max-height: 600px; \n'
    content += '  overflow: auto; \n'
    content += '  pointer-events: none; \n'
    content += '  transition: opacity 0.3s cubic-bezier(0.4, 0, 0.2, 1), transform 0.3s cubic-bezier(0.4, 0, 0.2, 1); \n'
    content += '  transform: translateX(30px) scale(0.95); \n'
    content += '}\n'
    content += '.diagram-item:hover .mermaid-preview { \n'
    content += '  opacity: 1; \n'
    content += '  visibility: visible; \n'
    content += '  transform: translateX(0) scale(1); \n'
    content += '}\n'
    # Important: The .mermaid class is added by superfences to the div inside the code block
    content += '.mermaid-preview .mermaid { background: transparent !important; color: white !important; }\n'
    content += '</style>\n\n'

    for filename in all_files:
        filepath = os.path.join(docs_dir, filename)
        page_url = f"../{filename.replace('.md', '/')}" if use_directory_urls else filename.replace(".md", ".html")
        if filename == "index.md": page_url = "../"

        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                lines = f.readlines()
        except: continue

        page_mermaid = []
        page_title = filename
        current_header = ""
        current_slug = ""

        i = 0
        while i < len(lines):
            line = lines[i]
            if line.startswith("# "):
                page_title = line.strip("# \n")
            elif line.startswith("## ") or line.startswith("### "):
                current_header = line.strip("# \n")
                current_slug = slugify(current_header)

            if "```mermaid" in line:
                mermaid_lines = []
                j = i + 1
                while j < len(lines):
                    if "```" in lines[j]:
                        break
                    mermaid_lines.append(lines[j])
                    j += 1

                title = current_header if current_header else "Top of page"
                anchor = f"#{current_slug}" if current_slug else "#"

                desc = ""
                for k in range(i-1, 0, -1):
                    p = lines[k].strip()
                    if p and not p.startswith("#") and not p.startswith("%%") and not p.startswith("```"):
                        # Escape problematic characters for HTML in description
                        safe_p = p.replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")
                        desc = f' : <span style="opacity: 0.6; font-size: 0.85em;">{re.sub(r"[*_]", "", safe_p)}</span>'
                        break

                pure_mermaid = clean_mermaid_code("".join(mermaid_lines))

                # We use a fenced block INSIDE the HTML div to ensure it's handled
                # correctly by the Markdown parser and Mermaid extension without corruption.
                item = f'<div class="diagram-item">\n'
                item += f'  <a href="{page_url}{anchor}" style="font-weight: 500; font-size: 1.1em; color: var(--md-typeset-a-color);">{title}</a>{desc}\n'
                item += f'  <div class="mermaid-preview">\n\n'
                item += f'```mermaid\n{pure_mermaid}\n```\n\n'
                item += f'  </div>\n'
                item += f'</div>\n'
                page_mermaid.append(item)
                i = j
            i += 1

        if page_mermaid:
            content += f"## [{page_title}]({page_url})\n\n"
            content += "\n".join(page_mermaid) + "\n\n"

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(content)

    return config
