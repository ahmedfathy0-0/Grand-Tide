#!/bin/bash
cd "$(dirname "$0")/.."

if [ -d "build" ]; then
    cd build
    ctest --output-on-failure
    cd ..
else
    echo "Build directory not found. Please build the project first."
fi
