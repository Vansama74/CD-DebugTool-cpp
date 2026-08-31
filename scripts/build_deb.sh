#!/bin/bash
set -euo pipefail
VERSION="${1:-1.0.0}"
PACKAGE="cd-debugtool"
ARCH="amd64"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${ROOT}/build"
# 暂存目录仍放 /tmp（纯临时内容，打包后即删）；最终 .deb 输出到项目内发布目录。
DEB_STAGE="/tmp/${PACKAGE}_${VERSION}"
DEB_OUT_DIR="${ROOT}/dist/linux"
DEB_FILE="${DEB_OUT_DIR}/${PACKAGE}_${VERSION}_${ARCH}.deb"

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

rm -rf "$DEB_STAGE"
mkdir -p "$DEB_STAGE/DEBIAN" \
         "$DEB_STAGE/usr/bin" \
         "$DEB_STAGE/usr/share/applications" \
         "$DEB_STAGE/usr/share/icons/hicolor/scalable/apps"

install -Dm755 "$BUILD_DIR/cd-debugtool" "$DEB_STAGE/usr/bin/cd-debugtool"
install -Dm644 "$ROOT/resources/icons/cd-debugtool.svg" "$DEB_STAGE/usr/share/icons/hicolor/scalable/apps/cd-debugtool.svg"
install -Dm644 "$ROOT/debian/cd-debugtool.desktop" "$DEB_STAGE/usr/share/applications/cd-debugtool.desktop"

# control: stamp the version
sed "s/^Version:.*/Version: ${VERSION}/" "$ROOT/debian/control" > "$DEB_STAGE/DEBIAN/control"

install -Dm755 "$ROOT/debian/postinst" "$DEB_STAGE/DEBIAN/postinst"
install -Dm755 "$ROOT/debian/prerm" "$DEB_STAGE/DEBIAN/prerm"

mkdir -p "$DEB_OUT_DIR"
dpkg-deb --build --root-owner-group "$DEB_STAGE" "$DEB_FILE"
echo "Built: $DEB_FILE"
