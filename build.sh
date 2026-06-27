#!/usr/bin/env sh
set -eu

BUILD_DIR="build"
RECONFIGURE=0
CLEAN=0
BUILD_TYPE="Release"
TARGET=""

# Parse args
for arg in "$@"; do
    case "$arg" in
        --reconfigure)
            RECONFIGURE=1
            ;;
        -c|--clean)
            CLEAN=1
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            ;;
        *)
            if [ -z "$TARGET" ]; then
                TARGET="$arg"
            else
                echo "Unknown extra argument: $arg"
                exit 1
            fi
            ;;
    esac
done

# Detect generator
GENERATOR=""
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="-G Ninja"
fi

# Clean
if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Configure
if [ ! -d "$BUILD_DIR" ] || [ "$RECONFIGURE" -eq 1 ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring ($BUILD_TYPE)..."
    cmake -S . -B "$BUILD_DIR" $GENERATOR -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

# Build
echo "Building ($BUILD_TYPE)..."

if [ -n "$TARGET" ]; then
    echo "Target: $TARGET"
    cmake --build "$BUILD_DIR" --parallel --target "$TARGET"
else
    cmake --build "$BUILD_DIR" --parallel
fi
