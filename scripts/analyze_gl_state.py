#!/usr/bin/env python3
import os
import re
import sys

def analyze_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    lines = content.splitlines()
    filename = os.path.basename(filepath)
    issues = []

    # Regex patterns
    gl_gen_pattern = re.compile(r'(glGen[a-zA-Z]+|glCreate[a-zA-Z]+)\s*\(([^,)]+)(?:,\s*&?([a-zA-Z0-9_>.-]+))?')
    gl_delete_pattern = re.compile(r'(glDelete[a-zA-Z]+)\s*\(([^,)]+)(?:,\s*&?([a-zA-Z0-9_>.-]+))?')
    gl_enable_pattern = re.compile(r'glEnable\s*\(\s*([a-zA-Z0-9_]+)\s*\)')
    gl_disable_pattern = re.compile(r'glDisable\s*\(\s*([a-zA-Z0-9_]+)\s*\)')

    # Store resources found
    generated_resources = []
    deleted_resources = []

    # Scan for resources
    for i, line in enumerate(lines):
        line_num = i + 1

        # Check for glGen/glCreate
        match_gen = gl_gen_pattern.search(line)
        if match_gen:
            func = match_gen.group(1)
            # For glGen*(n, &ids), group 3 is ids
            # For glCreate*(type), return value is assigned. Need to handle assignment.

            if func.startswith("glGen"):
                if match_gen.group(3):
                    var_name = match_gen.group(3).strip()
                    generated_resources.append({'func': func, 'var': var_name, 'line': line_num})
            elif func.startswith("glCreate"):
                # Look for var = glCreate...
                assign_match = re.search(r'([a-zA-Z0-9_>.-]+)\s*=\s*' + func, line)
                if assign_match:
                    var_name = assign_match.group(1).strip()
                    generated_resources.append({'func': func, 'var': var_name, 'line': line_num})

        # Check for glDelete
        match_del = gl_delete_pattern.search(line)
        if match_del:
            func = match_del.group(1)
            # For glDelete*(n, &ids), group 3 is ids
            # For glDelete*(id), group 2 is id

            if func == "glDeleteShader" or func == "glDeleteProgram":
                var_name = match_del.group(2).strip()
                deleted_resources.append({'func': func, 'var': var_name, 'line': line_num})
            else:
                if match_del.group(3):
                    var_name = match_del.group(3).strip()
                    deleted_resources.append({'func': func, 'var': var_name, 'line': line_num})

        # Check for glFinish
        if "glFinish" in line:
            issues.append({
                'type': 'Performance Warning',
                'line': line_num,
                'msg': 'glFinish() detected. This causes CPU-GPU sync and potential stalls.'
            })

        # Check for glGetError (legacy)
        if "glGetError" in line and "gl_debug" not in filepath: # Ignore in debug module
             issues.append({
                'type': 'Legacy Warning',
                'line': line_num,
                'msg': 'glGetError() detected. Prefer KHR_debug callbacks for error handling.'
            })

    # Heuristic: Check if generated resources are deleted
    # This is very simple and prone to false positives/negatives (e.g. passed to other functions)
    # But useful for identifying local variables or struct members that are never cleaned up in the same file.

    # Filter out local variables if function scope analysis is too hard.
    # Instead, report resources that look like struct members (->) but have no matching delete.

    for gen in generated_resources:
        var = gen['var']
        found = False

        # Simple string match in deleted resources
        for dell in deleted_resources:
            if dell['var'] == var: # Exact match
                found = True
                break
            # Handle &var vs var? Regex handles & extraction mostly.

        if not found:
            # Check if it's likely a struct member
            if "->" in var or "." in var:
                issues.append({
                    'type': 'Potential Leak',
                    'line': gen['line'],
                    'msg': f"Resource '{var}' created with {gen['func']} but no matching glDelete found in this file."
                })
            else:
                 # Local variable? Check if it is returned?
                 pass

    # Function-scope state check (glEnable/glDisable)
    # We need to detect functions. simple regex for C functions: type name(...) {

    func_pattern = re.compile(r'^[a-zA-Z0-9_*]+\s+([a-zA-Z0-9_]+)\s*\(.*\)\s*\{?$')

    current_func = None
    state_stack = [] # (cap, line)

    for i, line in enumerate(lines):
        line_num = i + 1
        line = line.strip()

        # Detect function start (heuristic)
        if line.startswith("{") and current_func is None:
             # Assume previous line was func def
             pass

        match_func = func_pattern.match(lines[i-1].strip() if i>0 else "") if line.startswith("{") else None
        if match_func:
            current_func = match_func.group(1)
            state_stack = []

        # Detect function end
        if line == "}" and current_func:
            # Check unclosed states
            for state in state_stack:
                issues.append({
                    'type': 'State Leak',
                    'line': state['line'],
                    'msg': f"glEnable({state['cap']}) in function '{current_func}' might not be disabled."
                })
            current_func = None
            state_stack = []

        # Track glEnable/glDisable
        match_enable = gl_enable_pattern.search(line)
        if match_enable:
            cap = match_enable.group(1)
            state_stack.append({'cap': cap, 'line': line_num})

        match_disable = gl_disable_pattern.search(line)
        if match_disable:
            cap = match_disable.group(1)
            # Find matching enable in stack (reverse)
            found = False
            for j in range(len(state_stack)-1, -1, -1):
                if state_stack[j]['cap'] == cap:
                    state_stack.pop(j)
                    found = True
                    break

    return issues

def main():
    src_dir = "src"
    all_issues = []

    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith(".c"):
                filepath = os.path.join(root, file)
                file_issues = analyze_file(filepath)
                for issue in file_issues:
                    issue['file'] = filepath
                    all_issues.append(issue)

    # Print Report
    print("Static Analysis Report")
    print("======================")

    if not all_issues:
        print("No issues found.")
    else:
        for issue in all_issues:
            print(f"[{issue['type']}] {issue['file']}:{issue['line']} - {issue['msg']}")

if __name__ == "__main__":
    main()
