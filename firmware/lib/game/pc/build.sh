#!/usr/bin/env bash
# Builds the PC harness (clang with the VS Build Tools). Run from anywhere: bash firmware/lib/game/pc/build.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$here/out"
clang++ -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers -D_CRT_SECURE_NO_WARNINGS \
  -I"$here/../../petconfig/src" -o "$here/out/sim.exe" "$here/sim.cpp" "$here"/../src/*.cpp
clang++ -std=c++17 -O1 -Wall -Wextra -Wno-missing-field-initializers -D_CRT_SECURE_NO_WARNINGS   -I"$here/../../petconfig/src" -o "$here/out/save_test.exe" "$here/save_test.cpp" "$here/../src/save.cpp"
echo "built $here/out/sim.exe and $here/out/save_test.exe"
