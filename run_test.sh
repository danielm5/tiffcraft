#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

source "./shared_cmake_preset.sh" "${1:-}"

echo "Testing ${PRESET_NAME}"

ctest --preset "${PRESET_NAME}" --output-on-failure --rerun-failed
