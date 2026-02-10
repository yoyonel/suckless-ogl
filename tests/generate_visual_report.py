import os
import sys

def generate_report(sha, repository):
    faces = ["front", "back", "left", "right", "top", "bottom"]
    html_index_path = "build-coverage/coverage_report/visual_tests/index.html"
    comment_path = "comment.md"

    # HTML Index Generation
    html = f"""<!DOCTYPE html>
<html>
<head>
  <title>Visual Regression Report (6 Views)</title>
  <style>
    body {{ font-family: sans-serif; background: #121212; color: #eee; padding: 20px; text-align: center; }}
    .face-section {{ margin-bottom: 60px; border-bottom: 1px solid #333; padding-bottom: 40px; }}
    .compare-container {{ position: relative; width: 1024px; height: 768px; margin: 20px auto; border: 2px solid #444; cursor: crosshair; }}
    .compare-container img {{ position: absolute; top: 0; left: 0; width: 100%; opacity: 0; transition: opacity 0.2s; }}
    .compare-container .img-ref {{ opacity: 1; }}
    .compare-container:hover .img-actual {{ opacity: 1; }}
    .diff-map {{ margin-top: 20px; border: 2px solid #500; width: 1024px; }}
    h2 {{ color: #00ff95; text-transform: capitalize; }}
  </style>
</head>
<body>
  <h1>Visual Regression Analysis (6 Views)</h1>
  <p>Passez la souris sur une vue pour comparer : <b>Référence</b> vs <b>Rendu PR</b></p>
"""
    for face in faces:
        failed_actual = f"failed_actual_{face}.png"
        failed_diff = f"failed_diff_{face}.png"
        html += f"<div class='face-section'><h2>Face: {face}</h2>\n"
        html += f"<div class='compare-container'><img src='ref_{face}.png' class='img-ref'><img src='{failed_actual}' class='img-actual' onerror='this.style.opacity=0'></div>\n"

        if os.path.exists(os.path.join("tests", failed_diff)):
            html += f"<h3>Difference Map</h3><img src='{failed_diff}' class='diff-map'>\n"
        else:
            html += "<p style='color: #00ff00;'>✅ All pixels matching for this view.</p>\n"
        html += "</div>\n"

    html += "<br><a href='../index.html' style='color: #00ff95;'>Retour au Rapport de Couverture</a></body></html>"

    with open(html_index_path, "w") as f:
        f.write(html)

    # Markdown Comment Generation
    comment = f"## 🎨 Visual Regression Report ({sha})\n\n"
    comment += "| Face | Comparison | Diff Map |\n"
    comment += "|:---:|:---:|:---:|\n"

    for face in faces:
        ref_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/ref_{face}.png?raw=true"
        actual_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/failed_actual_{face}.png?raw=true"
        diff_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/failed_diff_{face}.png?raw=true"

        ref_img = f"![Ref]({ref_url})"
        if os.path.exists(os.path.join("tests", f"failed_actual_{face}.png")):
            actual_img = f"![Actual]({actual_url})"
            diff_img = f"![Diff]({diff_url})"
            status = "❌ **FAIL**"
            comparison = f"{ref_img} <br> {actual_img}"
            diff = diff_img
        else:
            status = "✅ **PASS**"
            comparison = f"{ref_img} <br> ✅ Same as ref"
            diff = "-"

        comment += f"| **{face}** ({status}) | {comparison} | {diff} |\n"

    comment += "\n> [!TIP]\n"
    comment += "> Si ces changements visuels sont attendus, mettez à jour les fichiers `tests/ref_*.png`.\n"

    with open(comment_path, "w") as f:
        f.write(comment)

    # Set GITHUB_ENV if available
    github_env = os.getenv('GITHUB_ENV')
    if github_env:
        with open(github_env, "a") as env_file:
            env_file.write("VISUAL_COMMENT<<EOF\n")
            env_file.write(comment)
            env_file.write("\nEOF\n")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: generate_visual_report.py <sha> <repository>")
        sys.exit(1)
    generate_report(sys.argv[1], sys.argv[2])
