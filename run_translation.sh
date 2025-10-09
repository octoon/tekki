#!/bin/bash
# Kajiya Translation Tool - Quick Start Script (Linux/Mac)
# Usage: ./run_translation.sh <base_url> <api_key> [model]

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: ./run_translation.sh <base_url> <api_key> [model]"
    echo ""
    echo "Examples:"
    echo "  ./run_translation.sh \"https://api.openai.com/v1\" \"sk-xxxxx\""
    echo "  ./run_translation.sh \"https://api.openai.com/v1\" \"sk-xxxxx\" \"gpt-4-turbo\""
    echo ""
    exit 1
fi

BASE_URL="$1"
API_KEY="$2"
MODEL="${3:-gpt-5-high}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================"
echo "Kajiya Rust to C++ Translation Tool"
echo "========================================"
echo ""
echo "Base URL: $BASE_URL"
echo "Model: $MODEL"
echo "Output: $SCRIPT_DIR"
echo "Progress File: $SCRIPT_DIR/.translation_progress.json"
echo ""
echo "Starting translation..."
echo "Press Ctrl+C to interrupt (progress will be saved)"
echo ""

python3 "$SCRIPT_DIR/translate_kajiya.py" \
    --base-url "$BASE_URL" \
    --api-key "$API_KEY" \
    --model "$MODEL" \
    --kajiya-root "$SCRIPT_DIR/kajiya" \
    --output-root "$SCRIPT_DIR" \
    --progress-file "$SCRIPT_DIR/.translation_progress.json"

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Translation completed successfully!"
    echo "========================================"
else
    echo ""
    echo "========================================"
    echo "Translation stopped with errors"
    echo "Check the output above for details"
    echo "========================================"
fi
