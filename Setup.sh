#!/usr/bin/env bash
set -euo pipefail

PROFILE="conan/profiles/linux-gcc-debug"
CONFIG_PRESET="conan-debug"
BUILD_DIR="build/debug"

if [[ "${1:-}" == "release" ]]; then
  PROFILE="conan/profiles/linux-gcc-release"
  CONFIG_PRESET="conan-release"
  BUILD_DIR="build/release"
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if ! command -v conan >/dev/null 2>&1; then
  echo "[Error] Conan not found. Please install Conan 2.x first." >&2
  exit 1
fi

conan install . \
  --profile:host "$PROFILE" \
  --profile:build "$PROFILE" \
  --build=missing \
  --output-folder="$BUILD_DIR"

cmake --preset "$CONFIG_PRESET"
cmake --build --preset "$CONFIG_PRESET" --parallel
