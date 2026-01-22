#!/bin/sh

# Get project root
ROOT_DIR=$(git rev-parse --show-toplevel)
cd "$ROOT_DIR" || exit 1

echo "Installing git hooks..."

# Copy/Symlink hooks
ln -sf "$ROOT_DIR/hooks/pre-commit" "$ROOT_DIR/.git/hooks/pre-commit"
ln -sf "$ROOT_DIR/hooks/pre-push" "$ROOT_DIR/.git/hooks/pre-push"

echo "✅ Git hooks installed successfully!"
echo "   - pre-commit: runs 'make format'"
echo "   - pre-push:   runs 'make lint'"
