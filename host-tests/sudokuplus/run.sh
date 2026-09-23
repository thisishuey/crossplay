#!/bin/sh
# Builds and runs the SUDOKU+ game tests. No device, no PlatformIO and no SDK:
# SudokuPlusGame and SudokuPlusSave are freestanding over the shared SudokuCore.
#
#   host-tests/sudokuplus/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-sudokuplus-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/sudokuplus
CORE=../../src/apps_local/sudoku
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_sudokuplus.cpp $CORE/SudokuCore.cpp -o "$BUILD_DIR/test_sudokuplus"
"$BUILD_DIR/test_sudokuplus"
