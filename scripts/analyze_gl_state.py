import os
import re
import sys

def analyze_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove comments
    content = re.sub(r'//.*', '', content)
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

    # Find glGen* and glDelete*
    # We look for function calls like glGenTextures(..., glDeleteTextures(...
    # But checking just the name is safer for now.
    # Exclude 'erate' to avoid matching glGenerateMipmap
    gen_matches = re.findall(r'glGen(?!erate)([a-zA-Z]+)', content)
    del_matches = re.findall(r'glDelete([a-zA-Z]+)', content)

    gen_counts = {}
    for m in gen_matches:
        gen_counts[m] = gen_counts.get(m, 0) + 1

    del_counts = {}
    for m in del_matches:
        del_counts[m] = del_counts.get(m, 0) + 1

    mismatches = []
    # Check for Gen without Delete
    for obj_type, count in gen_counts.items():
        del_count = del_counts.get(obj_type, 0)
        if count != del_count:
            mismatches.append(f"glGen{obj_type}: {count}, glDelete{obj_type}: {del_count}")

    # Check for Delete without Gen (less likely but possible)
    for obj_type, count in del_counts.items():
        if obj_type not in gen_counts:
             mismatches.append(f"glDelete{obj_type}: {count}, glGen{obj_type}: 0")

    return mismatches

def main():
    mismatches_found = False
    for root, dirs, files in os.walk('src'):
        for file in files:
            if file.endswith('.c'):
                filepath = os.path.join(root, file)
                mismatches = analyze_file(filepath)
                if mismatches:
                    print(f"File: {filepath}")
                    for m in mismatches:
                        print(f"  - {m}")
                    mismatches_found = True

    if not mismatches_found:
        print("No mismatches found.")

if __name__ == '__main__':
    main()
