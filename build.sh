#!/bin/bash

# Exit on error
set -e

# Define build directory
BUILD_DIR=build

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Change to build directory
cd "$BUILD_DIR"

# Run CMake configuration
cmake ..

# Default target
TARGET=${1:-all}

# Special case for clean
if [ "$TARGET" = "clean" ]; then
    echo "Cleaning build directory..."
    find . -mindepth 1 ! -name '.gitkeep' ! -name '.' -exec rm -rf {} +
    echo "Build directory removed."
    exit 0
fi

# Build the specified target
echo "Building target: $TARGET"
cmake --build . --target "$TARGET"

