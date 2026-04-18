#!/bin/bash
cd "$(dirname "$0")/.."

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
cd ..

echo "Build Complete!"
