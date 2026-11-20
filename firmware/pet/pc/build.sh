#!/usr/bin/env bash
# Builds the PC checks (clang): bash firmware/pet/pc/build.sh
# menu_shot draws the pet's own screens with the face library; sfx_wav renders its sounds and dances to WAV files.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
face="$here/../../lib/face/src"
mkdir -p "$here/out"
clang++ -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -D_CRT_SECURE_NO_WARNINGS \
  -I"$face" -I"$here/../../lib/petconfig/src" -o "$here/out/menu_shot.exe" "$here/menu_shot.cpp" -I"$here/../src" "$here/../src/menu.cpp" "$here/../src/lab.cpp" "$face"/*.cpp
echo "built $here/out/menu_shot.exe"
clang++ -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers -D_CRT_SECURE_NO_WARNINGS   -I"$here/../src" -o "$here/out/sfx_wav.exe" "$here/sfx_wav.cpp" "$here/../src/sfx.cpp" "$here/../src/dances.cpp"
echo "built $here/out/sfx_wav.exe"
