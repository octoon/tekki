@echo off
REM Kajiya Translation Tool - Quick Start Script
REM 使用方法: run_translation.bat <base_url> <api_key> [model]

setlocal

if "%~1"=="" (
    echo Usage: run_translation.bat ^<base_url^> ^<api_key^> [model]
    echo.
    echo Examples:
    echo   run_translation.bat "https://api.openai.com/v1" "sk-xxxxx"
    echo   run_translation.bat "https://api.openai.com/v1" "sk-xxxxx" "gpt-4-turbo"
    echo.
    exit /b 1
)

if "%~2"=="" (
    echo Error: API key is required
    exit /b 1
)

set BASE_URL=%~1
set API_KEY=%~2
set MODEL=%~3

if "%MODEL%"=="" (
    set MODEL=gpt-5-high
)

echo ========================================
echo Kajiya Rust to C++ Translation Tool
echo ========================================
echo.
echo Base URL: %BASE_URL%
echo Model: %MODEL%
echo Output: %CD%
echo Progress File: %CD%\.translation_progress.json
echo.
echo Starting translation...
echo Press Ctrl+C to interrupt (progress will be saved)
echo.

python translate_kajiya.py ^
    --base-url "%BASE_URL%" ^
    --api-key "%API_KEY%" ^
    --model "%MODEL%" ^
    --kajiya-root "%CD%\kajiya" ^
    --output-root "%CD%" ^
    --progress-file "%CD%\.translation_progress.json"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Translation completed successfully!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Translation stopped with errors
    echo Check the output above for details
    echo ========================================
)

endlocal
