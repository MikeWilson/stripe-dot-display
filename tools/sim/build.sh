#!/usr/bin/env bash
# Builds the host renderer. Needs only a C++17 compiler.
set -euo pipefail
cd "$(dirname "$0")"
FW=../../firmware
clang++ -std=c++17 -O2 -DDOTGRID_CHECK_CLIP=1 -o sim \
  -I shim -I "$FW/include" \
  sim.cpp "$FW/src/dotgrid.cpp" "$FW/src/cards.cpp" "$FW/src/fonts.cpp" "$FW/src/state.cpp"
echo "built tools/sim/sim"
