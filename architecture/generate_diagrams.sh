#!/usr/bin/env bash
# Regenerate all PlantUML diagrams (PNG + SVG) into architecture/out/.
#
# Usage (from repo root or any directory):
#   ./architecture/generate_diagrams.sh
#   ./architecture/generate_diagrams.sh --png-only
#   ./architecture/generate_diagrams.sh --svg-only
#   ./architecture/generate_diagrams.sh --checkonly
#
# Requires: plantuml (https://plantuml.com)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIAGRAMS_DIR="${SCRIPT_DIR}/diagrams"
OUT_DIR="${SCRIPT_DIR}/out"

FORMAT_ARGS=(-tsvg -tpng)
CHECKONLY=false

for arg in "$@"; do
  case "$arg" in
    --png-only)
      FORMAT_ARGS=(-tpng)
      ;;
    --svg-only)
      FORMAT_ARGS=(-tsvg)
      ;;
    --checkonly)
      CHECKONLY=true
      FORMAT_ARGS=(-checkonly)
      ;;
    -h|--help)
      sed -n '2,9p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      echo "Run with --help for usage." >&2
      exit 1
      ;;
  esac
done

if ! command -v plantuml >/dev/null 2>&1; then
  echo "error: plantuml not found in PATH" >&2
  exit 1
fi

if [[ ! -d "$DIAGRAMS_DIR" ]]; then
  echo "error: diagrams directory not found: $DIAGRAMS_DIR" >&2
  exit 1
fi

mapfile -t PUML_FILES < <(find "$DIAGRAMS_DIR" -maxdepth 1 -name '*.puml' -printf '%f\n' | sort)

if [[ ${#PUML_FILES[@]} -eq 0 ]]; then
  echo "error: no .puml files in $DIAGRAMS_DIR" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

echo "PlantUML: $(plantuml -version 2>&1 | head -n 1)"
echo "Source:   $DIAGRAMS_DIR"
echo "Output:   $OUT_DIR"
echo "Files:    ${#PUML_FILES[@]}"
echo

# -o is relative to each input file's directory; cwd must be diagrams/.
cd "$DIAGRAMS_DIR"

run_plantuml() {
  plantuml "$@" -o ../out "${PUML_FILES[@]}"
}

if [[ "$CHECKONLY" == true ]]; then
  run_plantuml -checkonly
elif [[ ${#FORMAT_ARGS[@]} -eq 1 ]]; then
  run_plantuml "${FORMAT_ARGS[@]}"
else
  # Older PlantUML builds may emit only one format when both flags are combined.
  run_plantuml -tpng
  run_plantuml -tsvg
fi

if [[ "$CHECKONLY" == true ]]; then
  echo "Syntax check passed for ${#PUML_FILES[@]} file(s)."
  exit 0
fi

echo
echo "Done. Generated outputs in: $OUT_DIR"
