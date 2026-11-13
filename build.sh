
#!/bin/bash

source "$(dirname "$0")/shared_cmake_preset.sh" "$1"

git submodule update --init --recursive || echo "warning: could not update submodules" >&2
if git lfs version >/dev/null 2>&1; then
  git -C tests/libtiff-pics lfs pull || echo "warning: git lfs pull failed; test fixtures may be unresolved" >&2
else
  echo "warning: git-lfs not found; test fixtures may be unresolved Git LFS pointers (apt install git-lfs). Configure will fail with fixture sanity-check instructions." >&2
fi

echo "Building ${PRESET_NAME}"

cmake --preset "${PRESET_NAME}" -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} && \
  cmake --build --preset "${PRESET_NAME}" -j
