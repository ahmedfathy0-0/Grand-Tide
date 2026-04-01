@echo off
cd /d "%~dp0\.."
if exist bin\GAME_APPLICATION.exe (
    cd bin
    GAME_APPLICATION.exe
    cd ..
) else (
    echo Executable not found in bin folder. Please build the project first.
)
