#!/usr/bin/env sh
set -eu

BUILD_DIR="build"
BUILD_TYPE="Debug"
RECONFIGURE=0
CLEAN=0

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
            echo "Unknown argument: $arg"
            exit 1
            ;;
    esac
done

if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

GENERATOR_ARGS=""
if command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS="-G Ninja"
fi

if [ ! -d "$BUILD_DIR" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || [ "$RECONFIGURE" -eq 1 ]; then
    echo "Initializing CMake build directory ($BUILD_TYPE)..."
    cmake -S . -B "$BUILD_DIR" $GENERATOR_ARGS \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
else
    echo "Build directory already initialized."
    echo "Run with --reconfigure to change the build type."
fi

echo
echo "To build:"
echo "  cmake --build $BUILD_DIR --parallel"
echo
echo "To build a target:"
echo "  cmake --build $BUILD_DIR --target <target> --parallel"
