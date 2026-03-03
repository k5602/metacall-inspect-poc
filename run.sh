#!/usr/bin/env bash

set -e

BUILD_DIR="build-poc"
METACALL_DIR="${1:-}"

if [ -n "$METACALL_DIR" ] && [ ! -d "$METACALL_DIR" ]; then
    echo "Error: MetaCall build directory not found at: $METACALL_DIR"
    echo "Usage: ./run.sh [/path/to/metacall/core/build]"
    exit 1
fi

if [ -n "$METACALL_DIR" ]; then
    echo "Configuring with runtime targets enabled"
    CMAKE_ARGS=(
        -DMETACALL_ROOT_DIR="$METACALL_DIR"
        -DMETACALL_INSPECT_POC_BUILD_RUNTIME=ON
        -DMETACALL_INSPECT_POC_BUILD_RUNTIME_TESTS=ON
        -DMETACALL_INSPECT_POC_BUILD_BENCHMARKS=ON
    )
else
    echo "Configuring static-only PoC"
    CMAKE_ARGS=(
        -DMETACALL_INSPECT_POC_BUILD_RUNTIME=OFF
        -DMETACALL_INSPECT_POC_BUILD_RUNTIME_TESTS=OFF
        -DMETACALL_INSPECT_POC_BUILD_BENCHMARKS=OFF
    )
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "${CMAKE_ARGS[@]}" ..
cmake --build .

echo "--- Running Tests (if enabled) ---"
ctest --output-on-failure

echo "--- Running AST Inspect (Python) ---"
./ast_inspect "${OLDPWD}/scripts/test.py"

echo "--- Running AST Inspect (JavaScript) ---"
./ast_inspect "${OLDPWD}/scripts/test.js"

if [ -n "$METACALL_DIR" ]; then
    echo "--- Running Runtime Inspect POC ---"
    ./inspect_poc
fi
