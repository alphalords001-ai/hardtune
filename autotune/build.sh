#!/usr/bin/env bash
# Compilation Linux (étape 16 du plan). Ne pas lancer maintenant.
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
find build -name "*.vst3" -maxdepth 6
