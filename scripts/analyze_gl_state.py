#!/usr/bin/env python3
"""
OpenGL State Integrity Analyzer.

This script performs static analysis on C source files to detect potential
OpenGL state inconsistencies, specifically focusing on `glGen*` and `glDelete*`
mismatches within the same file scope.

It helps identify resource leaks (generation without deletion) and potential
use-after-free or double-free scenarios (deletion without generation in context).

Usage:
    python3 scripts/analyze_gl_state.py [source_dir]

Exit Code:
    0: No inconsistencies found.
    1: Inconsistencies detected.
"""

import os
import re
import sys
from typing import Dict, List


def analyze_file(filepath: str) -> List[str]:
    """
    Analyze a single C source file for OpenGL generation/deletion mismatches.

    Args:
        filepath: Path to the C source file.

    Returns:
        List of strings describing found inconsistencies.
    """
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except UnicodeDecodeError:
        return [f"Skipped {filepath} (encoding error)"]
    except OSError as e:
        return [f"Skipped {filepath} ({e})"]

    # Remove comments to avoid false positives in commented-out code
    # Remove single-line comments
    content = re.sub(r"//.*", "", content)
    # Remove multi-line comments
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)

    # Find glGen* and glDelete* calls
    # We use regex to capture the object type (e.g., "Textures" from "glGenTextures")
    # Exclude 'erate' to avoid matching glGenerateMipmap as a resource creation
    gen_pattern = re.compile(r"glGen(?!erate)([a-zA-Z]+)")
    del_pattern = re.compile(r"glDelete([a-zA-Z]+)")

    gen_matches = gen_pattern.findall(content)
    del_matches = del_pattern.findall(content)

    gen_counts: Dict[str, int] = {}
    for m in gen_matches:
        gen_counts[m] = gen_counts.get(m, 0) + 1

    del_counts: Dict[str, int] = {}
    for m in del_matches:
        del_counts[m] = del_counts.get(m, 0) + 1

    mismatches = []

    # Check for Gen without Delete (Potential Leak)
    for obj_type, count in gen_counts.items():
        del_count = del_counts.get(obj_type, 0)
        if count != del_count:
            mismatches.append(
                f"Potential Leak: glGen{obj_type} ({count}) vs glDelete{obj_type} ({del_count})"
            )

    # Check for Delete without Gen (Potential Ownership/Logic Issue)
    for obj_type, count in del_counts.items():
        if obj_type not in gen_counts:
            # This is common for resource managers that only delete, so we mark it as Note
            mismatches.append(
                f"Note: glDelete{obj_type} ({count}) found without local glGen{obj_type}"
            )

    return mismatches


def main() -> int:
    """
    Main entry point.

    Returns:
        Exit code (0 for success, 1 for failure).
    """
    source_dir = "src"
    if len(sys.argv) > 1:
        source_dir = sys.argv[1]

    if not os.path.exists(source_dir):
        print(f"Error: Source directory '{source_dir}' not found.")
        return 1

    print(f"[*] Analyzing OpenGL state in '{source_dir}'...")
    mismatches_found = False

    for root, _, files in os.walk(source_dir):
        for file in files:
            if file.endswith(".c"):
                filepath = os.path.join(root, file)
                mismatches = analyze_file(filepath)
                if mismatches:
                    print(f"\nFile: {filepath}")
                    for m in mismatches:
                        print(f"  - {m}")
                        if "Potential Leak" in m:
                            mismatches_found = True

    print("\n" + "=" * 60)
    if mismatches_found:
        print("[!] FAILURE: Potential OpenGL resource leaks detected.")
        print("    Please verify the reported files.")
        return 1
    else:
        print("[+] SUCCESS: No obvious OpenGL resource leaks found.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
