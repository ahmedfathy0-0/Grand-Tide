#!/bin/bash
cd "$(dirname "$0")"

# Run the build script
./build.sh

# Check if the build was successful
if [ $? -eq 0 ]; then
    echo "Starting application..."
    ./run.sh
else
    echo "Build failed! Aborting run."
    exit 1
fi
