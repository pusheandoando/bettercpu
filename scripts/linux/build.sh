#!/usr/bin/env bash
# scripts/linux/build.sh





set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
VERSION="$(cat "${PROJECT_ROOT}/VERSION")"
BUILD_DIR="${PROJECT_ROOT}/build"
DIST_DIR="${PROJECT_ROOT}/dist"

rm -rf "${BUILD_DIR}"

mkdir -p "${DIST_DIR}"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j"$(nproc)"

ldd "${BUILD_DIR}/cli/bettercpu" | grep -E "GLIBC|GLIBCXX" || true

cp "${BUILD_DIR}/cli/bettercpu" "${DIST_DIR}/bettercpu"
chmod 755 "${DIST_DIR}/bettercpu"
rm -rf "${BUILD_DIR}"

echo "[OK] build complete  ->  dist/bettercpu  (${VERSION})"