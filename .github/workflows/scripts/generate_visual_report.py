import os
import sys
import json
import re

def parse_ref_name(filename):
    # Pattern: ref_<view>_<mode>_<effect>.png or ref_<view>.png
    # Examples:
    # ref_front.png -> view=front, mode=default, effect=none
    # ref_front_subtle_bloom.png -> view=front, mode=subtle, effect=bloom
    # ref_front_default_auto_exposure.png -> view=front, mode=default, effect=auto_exposure

    basename = filename.replace("ref_", "").replace(".png", "")
    parts = basename.split("_")

    if len(parts) == 1:
        return parts[0], "default", "none"
    elif len(parts) == 3:
        return parts[0], parts[1], parts[2]
    elif len(parts) == 4: # auto_exposure has an underscore
        return parts[0], parts[1], "_".join(parts[2:])
    else:
        # Fallback
        return parts[0], "unknown", "_".join(parts[1:])

def generate_effect_diff(base_path, effect_path, out_path):
    """Generates a high-contrast difference map between baseline and effect image."""
    try:
        from PIL import Image, ImageChops, ImageEnhance
        base = Image.open(base_path).convert("RGB")
        eff = Image.open(effect_path).convert("RGB")
        # Absolute difference
        diff = ImageChops.difference(base, eff)
        # Enhance contrast to make changes visible
        enhancer = ImageEnhance.Contrast(diff)
        diff = enhancer.enhance(5.0) # 5x contrast
        diff.save(out_path)
        return True
    except ImportError:
        # Fallback to ImageMagick if Pillow is missing
        import subprocess
        try:
            # convert base.png effect.png -compose difference -composite -evaluate multiply 5 out.png
            subprocess.run([
                "convert", base_path, effect_path,
                "-compose", "difference", "-composite",
                "-evaluate", "multiply", "5",
                out_path
            ], check=True, capture_output=True)
            return True
        except Exception as e:
            print(f"[WARN] Failed to generate effect diff via ImageMagick: {e}")
            return False
    except Exception as e:
        print(f"[WARN] Failed to generate effect diff via Pillow: {e}")
        return False

def generate_report(sha, repository, pr_number=None):
    test_dir = "tests/references"
    html_index_path = "build-coverage/coverage_report/visual_tests/index.html"
    comment_path = ".github/workflows/scripts/comment.md"

    # Discovery
    ref_files = [f for f in os.listdir(test_dir) if f.startswith("ref_") and f.endswith(".png")]
    ref_files.sort()

    html_dir = os.path.dirname(html_index_path)
    os.makedirs(html_dir, exist_ok=True)

    import shutil

    data = []
    views = set()
    modes = set()
    effects = set()

    # First pass to find baselines (none)
    baselines = {} # (view, mode) -> path
    for f in ref_files:
        view, mode, effect = parse_ref_name(f)
        if effect == "none":
            baselines[(view, mode)] = os.path.join(test_dir, f)

    for f in ref_files:
        view, mode, effect = parse_ref_name(f)
        basename = f.replace("ref_", "").replace(".png", "")

        actual_name = f"failed_actual_{basename}.png"
        diff_name = f"failed_diff_{basename}.png"
        effect_diff_name = f"effect_diff_{basename}.png"

        entry = {
            "file": f,
            "view": view,
            "mode": mode,
            "effect": effect,
            "actual": actual_name,
            "diff": diff_name,
            "effect_diff": None,
            "status": "PASS",
            "baseline": None
        }

        # Copy reference
        shutil.copy2(os.path.join(test_dir, f), os.path.join(html_dir, f))

        # Handle failures
        if os.path.exists(os.path.join(test_dir, actual_name)):
            entry["status"] = "FAIL"
            shutil.copy2(os.path.join(test_dir, actual_name), os.path.join(html_dir, actual_name))
            shutil.copy2(os.path.join(test_dir, diff_name), os.path.join(html_dir, diff_name))
        else:
            entry["actual"] = f # Self-reference for hover/compare on success

        # Handle effect visualization
        if effect != "none" and (view, mode) in baselines:
            base_path = baselines[(view, mode)]
            effect_path = os.path.join(test_dir, f)
            out_diff_path = os.path.join(html_dir, effect_diff_name)
            if generate_effect_diff(base_path, effect_path, out_diff_path):
                entry["baseline"] = os.path.basename(base_path)
                entry["effect_diff"] = effect_diff_name

        data.append(entry)
        views.add(view)
        modes.add(mode)
        effects.add(effect)

    # Sort categories
    sorted_views = sorted(list(views))
    sorted_modes = sorted(list(modes))
    sorted_effects = sorted(list(effects))

    # Assemble HTML from external templates
    script_dir = os.path.dirname(os.path.abspath(__file__))

    with open(os.path.join(script_dir, "visual_report_template.html"), encoding="utf-8") as fh:
        html_template = fh.read()
    with open(os.path.join(script_dir, "visual_report.css"), encoding="utf-8") as fh:
        css = fh.read()
    with open(os.path.join(script_dir, "visual_report.js"), encoding="utf-8") as fh:
        js_template = fh.read()

    js = (js_template
          .replace("__REPORT_DATA__", json.dumps(data))
          .replace("__SORTED_VIEWS__", json.dumps(sorted_views)))

    html = (html_template
            .replace("__CSS_CONTENT__", css)
            .replace("__JS_CONTENT__", js))

    with open(html_index_path, "w", encoding="utf-8") as f:
        f.write(html)

    # Markdown Comment Generation
    comment = f"## 🎨 Visual Regression Report ({sha})\n\n"

    failing = [d for d in data if d["status"] == "FAIL"]

    if not failing:
        comment += "✅ **All visual tests passed!** (Analyzed versions: "
        comment += ", ".join(sorted_effects) + ")\n"
    else:
        comment += f"⚠️ **{len(failing)} regressions detected!**\n\n"
        comment += "| Variant | Reference | Comparison | Diff |\n"
        comment += "|:---:|:---:|:---:|:---:|\n"

        for entry in failing:
            variant = f"{entry['view']} / {entry['mode']} / {entry['effect']}"
            ref_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/{entry['file']}?raw=true"
            actual_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/{entry['actual']}?raw=true"
            diff_url = f"https://github.com/{repository}/blob/visual-artifacts/{sha}/{entry['diff']}?raw=true"

            ref_img = f"![Ref]({ref_url})"
            actual_img = f"![Actual]({actual_url})"
            diff_img = f"![Diff]({diff_url})"

            comment += f"| **{variant}** | {ref_img} | {actual_img} | {diff_img} |\n"

    comment += "\n> [!TIP]\n"
    if pr_number:
        interactive_url = f"https://yoyonel.github.io/suckless-ogl/pr-preview/pr-{pr_number}/coverage/visual_tests/index.html"
        comment += f"> Accédez au **[Rapport Interactif Complet]({interactive_url})** pour filtrer par vue/mode/effet et utiliser la loupe.\n"
    else:
        comment += "> Accédez au rapport interactif complet dans l'onglet **Summary** du CI pour filtrer par vue/mode/effet.\n"

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
        print("Usage: generate_visual_report.py <sha> <repository> [pr_number]")
        sys.exit(1)

    pr_num = sys.argv[3] if len(sys.argv) > 3 else None
    generate_report(sys.argv[1], sys.argv[2], pr_num)
