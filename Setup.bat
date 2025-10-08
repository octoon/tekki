@echo off
setlocal enabledelayedexpansion

set PROFILE=conan\profiles\windows-msvc-debug
set CONFIGURE_PRESET=conan-default
set BUILD_PRESET=conan-debug
set BUILD_DIR=build\debug

if "%1"=="release" (
    set PROFILE=conan\profiles\windows-msvc-release
    set CONFIGURE_PRESET=conan-default
    set BUILD_PRESET=conan-release
    set BUILD_DIR=build\release
)

where conan >nul 2>nul
if errorlevel 1 (
    echo [Error] Conan not found. Please install Conan 2.x and ensure it is on PATH.
    exit /b 1
)

conan install . ^
    --profile:host !PROFILE! ^
    --profile:build !PROFILE! ^
    --build=missing ^
    --output-folder=!BUILD_DIR!

cmake --preset !CONFIGURE_PRESET!
cmake --build --preset !BUILD_PRESET! --parallel

endlocal
