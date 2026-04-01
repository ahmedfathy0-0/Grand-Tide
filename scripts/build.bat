@echo off
cd /d "%~dp0\.."
if not exist build mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..
echo Build Complete!
