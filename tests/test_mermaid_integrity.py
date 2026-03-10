import os
import re
import urllib.request
import subprocess
import time
import signal

def test_mermaid_index_full():
    print("Starting Mermaid Integration Test...")

    # 1. Build
    subprocess.run(["python3", "-m", "mkdocs", "build"], check=True)

    # 2. Basic file checks
    index_path = "docs/diagram_index.md"
    assert os.path.exists(index_path), "diagram_index.md missing"

    with open(index_path, "r") as f:
        content = f.read()

    # 3. Content Integrity (Sanity check)
    problematic = ["💀", "✓", "→", "⏳", "—"] # Ensure no problematic chars remain in index
    for char in problematic:
        assert char not in content, f"Index contains problematic character: {char}"

    # Check for mermaid blocks (matches the new fenced blocks style)
    mermaid_blocks = re.findall(r'```mermaid\n(.*?)\n```', content, flags=re.DOTALL)
    assert len(mermaid_blocks) > 0, "No mermaid diagrams found in index"

    for i, block in enumerate(mermaid_blocks):
        # 1. Sanity check: Ensure NO HTML tags leaked into the diagram code
        # (This catches the case where Markdown parser corrupts the code into lists/etc)
        assert "<ul" not in block.lower(), f"Block {i} is corrupted by Markdown (contains <ul>)"
        assert "<li" not in block.lower(), f"Block {i} is corrupted by Markdown (contains <li>)"

        # 2. Extract body for type validation
        body = re.sub(r'%%\{init:.*?\}%%', '', block, flags=re.DOTALL).strip()
        lines = [l.strip() for l in body.splitlines() if l.strip()]
        assert len(lines) > 0, f"Block {i} is empty"

        valid_types = ["sequenceDiagram", "graph", "flowchart", "classDiagram", "stateDiagram", "stateDiagram-v2", "gantt", "pie", "erDiagram"]
        assert any(lines[0].startswith(t) for t in valid_types), f"Block {i} has unknown type: {lines[0]}"

    # 4. Reachability Test (Serve)
    # Using 127.0.0.1:8002 to avoid conflicts
    process = subprocess.Popen(
        ["python3", "-m", "mkdocs", "serve", "-a", "127.0.0.1:8002"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )

    try:
        # Wait for server to start
        max_retries = 10
        server_ready = False
        for i in range(max_retries):
            try:
                with urllib.request.urlopen("http://127.0.0.1:8002/diagram_index/") as r:
                    if r.getcode() == 200:
                        server_ready = True
                        break
            except:
                time.sleep(1)

        assert server_ready, "Server didn't start in time"

        links = re.findall(r'href="([^"]+)"', content)
        failed = []
        for link in links:
            if link.startswith("../"):
                url = "http://127.0.0.1:8002/" + link[3:].split("#")[0]
            else:
                continue

            try:
                with urllib.request.urlopen(url) as r:
                    if r.getcode() != 200:
                        failed.append(f"{link} (HTTP {r.getcode()})")
            except Exception as e:
                failed.append(f"{link} (Error: {str(e)})")

        if failed:
            print("Failed links:")
            for f in failed: print(f" - {f}")
            exit(1)

        print(f"Verified {len(mermaid_blocks)} diagrams and {len(links)} links successfully.")

    finally:
        process.send_signal(signal.SIGINT)
        process.wait()

if __name__ == "__main__":
    test_mermaid_index_full()
