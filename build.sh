#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

command -v qmake >/dev/null || { echo "Qt 5 qmake is required." >&2; exit 1; }
command -v make >/dev/null || { echo "make is required." >&2; exit 1; }
mkdir -p build dist
(
    cd build
    qmake ../SNet.pro
    make -j"${JOBS:-$(nproc)}"
)
cp build/SNet dist/SNet
chmod +x dist/SNet
echo "Build complete: ./dist/SNet"
