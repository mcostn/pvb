#!/usr/bin/env sh
set -eu

APP_NAME="pvb"
BUILD_DIR="build"

PACKAGE_ROOT=".release"
PACKAGE_NAME="${APP_NAME}-linux-x86_64"
PACKAGE_DIR="$PACKAGE_ROOT/$PACKAGE_NAME"
ARCHIVE="${PACKAGE_NAME}.tar.gz"

rm -rf "$PACKAGE_ROOT" "$ARCHIVE"

./init.sh --release --reconfigure -- -DBUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel

mkdir -p "$PACKAGE_DIR"

cp "$BUILD_DIR/$APP_NAME" "$PACKAGE_DIR/"
cp -r examples "$PACKAGE_DIR/"

if command -v strip >/dev/null 2>&1; then
    strip "$PACKAGE_DIR/$APP_NAME"
fi

tar -czf "$ARCHIVE" -C "$PACKAGE_ROOT" "$PACKAGE_NAME"

rm -rf "$PACKAGE_ROOT"

echo "Created $ARCHIVE"
