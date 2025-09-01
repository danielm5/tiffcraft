
#!/bin/bash

# Parse --preset argument
PRESET_ARG=""
for ((i=1; i<=$#; i++)); do
    arg="${!i}"
    if [[ "$arg" == "--preset" ]]; then
        next_idx=$((i+1))
        PRESET_ARG="${!next_idx}"
    fi
done

source "$(dirname "$0")/shared_cmake_preset.sh" "$PRESET_ARG"

echo "Installing ${PRESET_NAME}"
cmake --build --preset "${PRESET_NAME}" --target install

# Check for --version argument anywhere in the argument list
VERSION_ARG=false
for ((i=1; i<=$#; i++)); do
	arg="${!i}"
	if [[ "$arg" == "--version" ]]; then
		VERSION_ARG=true
		break
	fi
done

if $VERSION_ARG; then
	TIFF_HEADER="install/include/tiffcraft/TiffImage.hpp"
	if [[ ! -f "$TIFF_HEADER" ]]; then
		echo "Error: $TIFF_HEADER not found. Cannot extract version information."
		exit 1
	fi
	MAJOR_VERSION=$(grep 'constexpr int MAJOR_VERSION' "$TIFF_HEADER" | grep -o '[0-9]\+')
	MINOR_VERSION=$(grep 'constexpr int MINOR_VERSION' "$TIFF_HEADER" | grep -o '[0-9]\+')
	PATCH_VERSION=$(grep 'constexpr int PATCH_VERSION' "$TIFF_HEADER" | grep -o '[0-9]\+')
	VERSION="v${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
	echo "Creating tarball tiffcraft-$VERSION.tar.gz"
	tar -czvf "tiffcraft-$VERSION.tar.gz" -C install include
	if command -v zip >/dev/null 2>&1; then
		echo "Creating zip file tiffcraft-$VERSION.zip"
		(cd install && zip -r "../tiffcraft-$VERSION.zip" include)
	else
		echo "zip command not found, skipping zip archive creation."
	fi
fi
