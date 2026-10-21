#!/usr/bin/env bash
# Builds the host renderer. Needs only a C++17 compiler.
#
# CI runs this exact script rather than its own copy of the compile line --
# the two used to be separate and drifted the first time a source file was
# added, which failed the build for a reason unrelated to the change.
set -euo pipefail
cd "$(dirname "$0")"
FW=../../firmware
CXX_BIN="${CXX:-$(command -v clang++ || command -v g++)}"

"$CXX_BIN" -std=c++17 -O2 -DDOTGRID_CHECK_CLIP=1 -o sim \
  -I shim -I "$FW/include" \
  sim.cpp \
  "$FW/src/dotgrid.cpp" \
  "$FW/src/cards.cpp" \
  "$FW/src/fonts.cpp" \
  "$FW/src/state.cpp" \
  "$FW/src/textlayer.cpp"
echo "built tools/sim/sim  (with $CXX_BIN)"
