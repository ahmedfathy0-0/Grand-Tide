#!/bin/bash
cd "$(dirname "$0")/.."

if [ -f "bin/GAME_APPLICATION" ]; then
    cd bin
    ./GAME_APPLICATION
    cd ..
else
    echo "Executable not found in bin folder. Please build the project first."
fi
