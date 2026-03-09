import os
import re
import zlib
import base64
import hashlib
import pathlib
import subprocess
import shutil
from mkdocs.plugins import event_priority

def get_local_diagram_path(code, label, config):
    hash_id = hashlib.sha256(code.encode('utf-8')).hexdigest()[:16]
    docs_dir = pathlib.Path(config['docs_dir'])
    # Diagram cache directory in assets
    assets_dir = docs_dir / "assets" / "diagrams"
    assets_dir.mkdir(parents=True, exist_ok=True)
    
    filename = f"{label.lower()}_{hash_id}.svg"
    return assets_dir / filename, f"assets/diagrams/{filename}"

def render_local_dot(code, output_path):
    dot_path = shutil.which("dot")
    if not dot_path:
        raise RuntimeError("❌ [DOT ERROR] binary 'dot' not found. Please install Graphviz locally.")
        
    try:
        process = subprocess.Popen(
            [dot_path, "-Tsvg", "-o", str(output_path)],
            stdin=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        stdout, stderr = process.communicate(input=code)
        if process.returncode == 0:
            return True
        else:
            print(f"❌ [DOT] Error {process.returncode} for {output_path}:\n{stderr}")
            return False
    except Exception as e:
        print(f"❌ [DOT] Exception during rendering: {e}")
        return False

@event_priority(50)
def on_page_markdown(markdown, page, config, files):
    # Match both ```graphviz and ```dot
    pattern = re.compile(r'```(?:graphviz|dot)\n(.*?)\n```', re.DOTALL)
    
    def replacer(match):
        code = match.group(1).strip()
        if not code: return ""
        
        local_file, web_path = get_local_diagram_path(code, "graphviz", config)
        
        # Render if not already cached
        if not local_file.exists():
            success = render_local_dot(code, local_file)
            if not success:
                return f'<div class="admonition error"><p class="admonition-title">Diagram Error</p><p>Failed to render Graphviz diagram locally.</p><pre>{code}</pre></div>'

        # Reference the local SVG file relative to site root
        # We use a leading slash to ensure it's absolute from the domain root
        return f'<div class="kroki-diagram local-diagram"><img src="/{web_path}" alt="Graphviz Diagram" /></div>'
            
    return pattern.sub(replacer, markdown)
