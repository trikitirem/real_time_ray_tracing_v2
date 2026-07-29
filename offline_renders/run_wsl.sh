#!/usr/bin/env bash
# Run the offline-render generator in WSL (no Windows Python required).
# Pure stdlib, nothing to install -- unlike tests/benchmarks/run_wsl.sh this
# has no dependency-install step.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

python3 generate.py "$@"
