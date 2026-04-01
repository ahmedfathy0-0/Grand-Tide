@echo off
cd /d "%~dp0\.."
if exist build (
    cd build
    ctest -C Release --output-on-failure
    cd ..
) else (
    echo Build directory not found. Please build the project first.
)
