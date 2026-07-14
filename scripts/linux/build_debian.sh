#!/usr/bin/env bash
# scripts/linux/build_debian.sh





set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
VERSION="$(cat "${PROJECT_ROOT}/VERSION")"
PKG_NAME="bettercpu"
BUILD_DIR="${PROJECT_ROOT}/build"
DIST_DIR="${PROJECT_ROOT}/dist"
ARCH="$(dpkg --print-architecture 2>/dev/null || uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')"
STAGING_DIR="${BUILD_DIR}/.deb_staging/${PKG_NAME}_${VERSION}_${ARCH}"

if ! command -v dpkg-deb &>/dev/null; then
    echo "[!!] dpkg-deb not found. Install it with: sudo apt install dpkg"
    exit 1
fi

rm -rf "${BUILD_DIR}"

mkdir -p "${DIST_DIR}"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j"$(nproc)"

ldd "${BUILD_DIR}/cli/bettercpu" | grep -E "GLIBC|GLIBCXX" || true

mkdir -p "${STAGING_DIR}/usr/bin"
mkdir -p "${STAGING_DIR}/DEBIAN"
cp "${BUILD_DIR}/cli/bettercpu" "${STAGING_DIR}/usr/bin/bettercpu"
chmod 755 "${STAGING_DIR}/usr/bin/bettercpu"

cat > "${STAGING_DIR}/DEBIAN/control" <<CONTROL
Package: ${PKG_NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: Christian <pusheandoando@github>
Section: admin
Priority: optional
Description: bettercpu - Linux CPU optimizer
 Adaptive real-time tuning of CPU frequency scaling, thermal limits,
 I/O schedulers, and memory pressure on Linux systems.
CONTROL

dpkg-deb --root-owner-group --build "${STAGING_DIR}" "${DIST_DIR}/${PKG_NAME}_${VERSION}.deb"

rm -rf "${BUILD_DIR}"

echo "[OK] package ready  ->  dist/${PKG_NAME}_${VERSION}.deb  (${VERSION})"
echo "[OK] install with:  sudo apt install ./dist/${PKG_NAME}_${VERSION}.deb"