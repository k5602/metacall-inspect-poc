#!/usr/bin/env bash

# ---------------------------------------------------------------------------
# metacall-inspect-poc runner script
# ---------------------------------------------------------------------------

set -e

# Default to the parent 'build' folder if not provided
METACALL_DIR="${1:-$(readlink -f ../build)}"
BUILD_DIR="build-poc"

if [ ! -d "$METACALL_DIR" ]; then
    echo "Error: MetaCall build directory not found at: $METACALL_DIR"
    echo "Usage: ./run.sh [/path/to/metacall/core/build]"
    exit 1
fi

echo "Building POC with MetaCall from: $METACALL_DIR"

# Configure and Build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DMETACALL_ROOT_DIR="$METACALL_DIR" ..
cmake --build .

# Run POC
echo "--- Running POC ---"
./inspect_poc

# Run Tests
echo "--- Running Tests ---"
ctest --output-on-failure
