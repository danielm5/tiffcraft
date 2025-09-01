
#!/bin/bash

source "$(dirname "$0")/shared_cmake_preset.sh" "$1"

echo "Building ${PRESET_NAME}"

cmake --preset "${PRESET_NAME}" -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} && \
  cmake --build --preset "${PRESET_NAME}" -j
