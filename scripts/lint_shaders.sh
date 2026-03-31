#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# GLSL Shader Lint — Resolve @header includes, validate via glslangValidator
#
# Usage: scripts/lint_shaders.sh [--strict] [shader_dir]
#   --strict   : Use SPIR-V OpenGL target (surfaces RenderDoc-class issues:
#                missing layout(location=), non-opaque uniforms without layout)
#   shader_dir : defaults to "shaders"
#
# Resolves the custom @header "path"; preprocessor into
# self-contained GLSL, then validates each top-level .vert/.frag/.comp
# with glslangValidator.
# ==============================================================================

STRICT=0
if [[ "${1:-}" == "--strict" ]]; then
	STRICT=1
	shift
fi

SHADER_DIR="${1:-shaders}"
TMPDIR_BASE="${TMPDIR:-/tmp}/suckless-ogl-shader-lint"
ERRORS=0
WARNINGS=0
VALIDATED=0

cleanup() { rm -rf "$TMPDIR_BASE"; }
trap cleanup EXIT

require_tool() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "ERROR: $1 not found. Install it to lint shaders." >&2
		exit 1
	fi
}

require_tool glslangValidator

# Recursively resolve @header "path"; directives into inline content.
# $1: input file  $2: base directory for relative resolution
resolve_headers() {
	local file="$1"
	local base_dir="$2"

	while IFS= read -r line; do
		# Match @header "relative/path";  or  @header "path";
		if [[ "$line" =~ ^[[:space:]]*@header[[:space:]]+\"([^\"]+)\"[[:space:]]*\;?[[:space:]]*$ ]]; then
			local inc_path="${BASH_REMATCH[1]}"
			local resolved

			# Try relative to current file first, then shader root
			if [[ -f "${base_dir}/${inc_path}" ]]; then
				resolved="${base_dir}/${inc_path}"
			elif [[ -f "${SHADER_DIR}/${inc_path}" ]]; then
				resolved="${SHADER_DIR}/${inc_path}"
			else
				echo "ERROR: Cannot resolve @header \"${inc_path}\" from ${file}" >&2
				return 1
			fi

			# Recurse into the included file
			resolve_headers "$resolved" "$(dirname "$resolved")"
		else
			printf '%s\n' "$line"
		fi
	done < "$file"
}

echo "Validating GLSL shaders in ${SHADER_DIR}/ ..."
if [[ "$STRICT" -eq 1 ]]; then
	echo "  Mode: STRICT (SPIR-V OpenGL target — surfaces RenderDoc issues)"
else
	echo "  Mode: Standard (desktop GLSL validation)"
fi
echo ""

mkdir -p "$TMPDIR_BASE"

# Build glslangValidator flags
GLSLANG_FLAGS=()
if [[ "$STRICT" -eq 1 ]]; then
	GLSLANG_FLAGS+=(--target-env opengl)
fi

# Find all top-level shader entry points (.vert, .frag, .comp)
for shader in "${SHADER_DIR}"/*.vert "${SHADER_DIR}"/*.frag "${SHADER_DIR}"/*.comp; do
	[[ -f "$shader" ]] || continue

	base="$(basename "$shader")"
	resolved_file="${TMPDIR_BASE}/${base}"

	# Resolve @header includes into a self-contained file
	if ! resolve_headers "$shader" "$(dirname "$shader")" > "$resolved_file" 2>&1; then
		echo "  [FAIL] ${shader}: include resolution failed"
		ERRORS=$((ERRORS + 1))
		continue
	fi

	# Validate with glslangValidator
	if output=$(glslangValidator "${GLSLANG_FLAGS[@]}" "$resolved_file" 2>&1); then
		# Check for warnings even on success
		if echo "$output" | grep -qi "warning"; then
			echo "  [WARN] ${shader}"
			echo "$output" | sed "s|${resolved_file}|${shader}|g" | grep -i "warning" | head -10
			WARNINGS=$((WARNINGS + 1))
		else
			echo "  [OK]   ${shader}"
		fi
	else
		echo "  [FAIL] ${shader}"
		# Show errors with reference to original file
		echo "$output" | sed "s|${resolved_file}|${shader}|g" | grep -i "error\|warning" | head -20
		ERRORS=$((ERRORS + 1))
	fi

	VALIDATED=$((VALIDATED + 1))
done

echo ""
echo "Summary: ${VALIDATED} shaders validated, ${ERRORS} failed, ${WARNINGS} warnings."

if [[ "$ERRORS" -gt 0 ]]; then
	echo ""
	echo "Resolved shaders are in ${TMPDIR_BASE}/ for inspection."
	# Keep tmpdir on failure for debugging
	trap - EXIT
	exit 1
fi

if [[ "$WARNINGS" -gt 0 ]]; then
	echo ""
	echo "Warnings found — these may cause issues in RenderDoc."
	echo "Run with --strict to see SPIR-V-level issues."
fi
