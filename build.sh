#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Return to original directory
cd ..

echo "Build script finished running" 