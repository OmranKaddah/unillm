#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! command -v doxygen >/dev/null 2>&1; then
  echo "doxygen is required to build API reference." >&2
  exit 1
fi

doxygen docs/Doxyfile
mkdocs build --strict

echo "Docs built at: site/"
