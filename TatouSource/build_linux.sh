#!/bin/bash
###############################################################################
# Linux build script for FITD (Alone In The Dark Re-Haunted)
#
# Prerequisites (Debian/Ubuntu):
#   sudo apt-get install build-essential cmake pkg-config \
#       libx11-dev libgl-dev libglu1-mesa-dev libasound2-dev \
#       libpulse-dev libdbus-1-dev libxext-dev libxrandr-dev \
#       libxcursor-dev libxi-dev libxss-dev libwayland-dev \
#       libxkbcommon-dev
#
# Prerequisites (Fedora/RHEL):
#   sudo dnf install gcc-c++ cmake pkgconfig \
#       libX11-devel mesa-libGL-devel alsa-lib-devel \
#       pulseaudio-libs-devel dbus-devel libXext-devel \
#       libXrandr-devel libXcursor-devel libXi-devel \
#       libXScrnSaver-devel wayland-devel libxkbcommon-devel
#
# Usage:
#   ./build_linux.sh          # Release build
#   ./build_linux.sh Debug    # Debug build
#   ./build_linux.sh clean    # Clean build directory
###############################################################################
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="${SCRIPT_DIR}/build/linux"

if [ "$1" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
    exit 0
fi

echo "=== FITD Linux Build ==="
echo "Build type: ${BUILD_TYPE}"
echo "Build dir:  ${BUILD_DIR}"
echo ""

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$SCRIPT_DIR" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build . --parallel "$(nproc)"

echo ""
echo "=== Build complete ==="
echo "Binary: ${BUILD_DIR}/Fitd/Tatou"
