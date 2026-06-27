#!/usr/bin/env bash
# Run benchmark visualizations in WSL (no Windows Python required).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PACKAGES_DIR="$SCRIPT_DIR/.packages"
INPUT="${1:-$SCRIPT_DIR/fixtures/mock_stress_test_summary.json}"
OUTPUT="${2:-$SCRIPT_DIR/output}"

if [[ ! -f "$INPUT" ]]; then
    echo "Generating mock fixture..."
    python3 fixtures/generate_mock.py
fi

if [[ ! -d "$PACKAGES_DIR" ]]; then
    echo "Installing Python dependencies to .packages/ (first run only)..."
    if ! python3 -m pip --version &>/dev/null; then
        curl -sS https://bootstrap.pypa.io/get-pip.py -o /tmp/get-pip.py
        python3 /tmp/get-pip.py --user --break-system-packages
    fi
    python3 -m pip install --target "$PACKAGES_DIR" -r requirements.txt --break-system-packages
fi

PYTHONPATH="$PACKAGES_DIR" python3 generate_visualizations.py --input "$INPUT" --output "$OUTPUT"
