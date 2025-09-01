
#!/bin/bash

source "$(dirname "$0")/shared_cmake_preset.sh" "$1"

echo "Testing ${PRESET_NAME}"

ctest --preset "${PRESET_NAME}" --output-on-failure --rerun-failed
