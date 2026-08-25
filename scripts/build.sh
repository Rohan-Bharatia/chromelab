#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${ROOT_DIR}/build"
BUILD_TYPE="${1:-Release}"

echo "=== chromelab build ==="
echo "  Source:  ${ROOT_DIR}"
echo "  Build:   ${BUILD_DIR}"
echo "  Type:    ${BUILD_TYPE}"

# Check for required tools
for tool in cmake protoc; do
    if ! command -v "$tool" &>/dev/null; then
        echo "ERROR: $tool not found. Run scripts/build-deps.sh first."
        exit 1
    fi
done
if ! command -v g++ &>/dev/null; then
    echo "ERROR: g++ not found. Run scripts/build-deps.sh first."
    exit 1
fi

# Create build directory
mkdir -p "${BUILD_DIR}"

# Configure
echo ""
echo "--- CMake configure ---"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo ""
echo "--- Building ---"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 4)"

# Report
echo ""
echo "=== Build complete ==="
echo "  labd:   ${BUILD_DIR}/labd"
echo "  labctl: ${BUILD_DIR}/labctl"
echo ""
echo "To install: cmake --install ${BUILD_DIR} --prefix /usr/local"
