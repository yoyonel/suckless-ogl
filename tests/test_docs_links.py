import os
import re
import requests
import pytest
import subprocess
import time
import signal

def test_diagram_index_links():
    # 1. Build the documentation to ensure diagram_index.md is generated
    subprocess.run(["python3", "-m", "mkdocs", "build"], check=True)

    # 2. Check if the file exists
    index_path = "docs/diagram_index.md"
    assert os.path.exists(index_path), "diagram_index.md was not generated"

    # 3. Start mkdocs serve in the background
    process = subprocess.Popen(
        ["python3", "-m", "mkdocs", "serve", "-a", "127.0.0.1:8001"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

    try:
        # Wait for server to be ready
        max_retries = 10
        server_ready = False
        for i in range(max_retries):
            try:
                requests.get("http://127.0.0.1:8001/diagram_index/", timeout=2)
                server_ready = True
                break
            except:
                time.sleep(1)

        assert server_ready, "MkDocs server did not start in time"

        # 4. Parse links from the generated markdown
        with open(index_path, "r") as f:
            content = f.read()

        # Extract links in HTML href="..." format since we use HTML in the hook
        links = re.findall(r'href="([^"]+)"', content)
        assert len(links) > 0, "No links found in diagram_index.md"

        # 5. Verify each link
        failed_links = []
        for link in links:
            # We are on /diagram_index/, so ../page/ becomes /page/
            if link.startswith("../"):
                url = "http://127.0.0.1:8001/" + link[3:]
            else:
                url = "http://127.0.0.1:8001/diagram_index/" + link

            # Remove anchor for request
            clean_url = url.split("#")[0]

            try:
                # Use allow_redirects=True because MkDocs often redirects /page to /page/
                resp = requests.get(clean_url, timeout=5, allow_redirects=True)
                if resp.status_code != 200:
                    failed_links.append(f"{link} (HTTP {resp.status_code}) -> URL: {clean_url}")
            except Exception as e:
                failed_links.append(f"{link} (Error: {str(e)})")

        assert not failed_links, f"The following links returned 404 or errors:\n" + "\n".join(failed_links)
        print(f"\nSuccessfully verified {len(links)} links in the diagram index.")

    finally:
        # Kill the background server
        process.send_signal(signal.SIGINT)
        process.wait()

if __name__ == "__main__":
    test_diagram_index_links()
