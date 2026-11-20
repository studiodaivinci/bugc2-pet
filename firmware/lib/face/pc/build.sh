#!/usr/bin/env bash
# Builds the PC harness for the face library (clang). Run from anywhere: bash firmware/lib/face/pc/build.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$here/out"
clang++ -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -D_CRT_SECURE_NO_WARNINGS \
  -I"$here/../../petconfig/src" -o "$here/out/sim.exe" "$here/sim.cpp" "$here"/../src/*.cpp
echo "built $here/out/sim.exe"
