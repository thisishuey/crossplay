#pragma once

// SUDOKU+'s save file as text, and back. Freestanding: no storage, no
// renderer, so the round trip is host-tested (host-tests/sudokuplus) rather
// than trusted. The activity only moves these bytes to and from the card.
//
// The format is SUDOKU's, with the three flags this game adds in the header:
//
//   version level elapsedMs hintsUsed notes showRemaining shadePeers solved
//   hasGame  solved[4]  bestMs[4]  hintsTaken
//   81 clue digits
//   81 entry digits
//   81 three-digit hex note masks
//
// The answer is not saved. The clues have exactly one solution, so it is
// re-derived on load, and a save that cannot be solved is a corrupt save. What
// is also not saved, on purpose: the selection, the focus, a hint and CHECK's
// marks. A board you come back to has nothing selected, so the first tap after
// a week away cannot write into a cell chosen before it.

#include <cstdio>
#include <cstdlib>

#include "SudokuPlusGame.h"

namespace sudokuplus {

// Its own file. SUDOKU's is /.crosspoint/sudoku.sav, and the two games share an
// engine but never a save or a record.
constexpr char kStatePath[] = "/.crosspoint/sudokuplus.sav";

constexpr int kStateVersion = 1;
// Eighteen small integers, then 81 + 81 digits and 81 three-digit hex masks.
constexpr int kStateBytes = 768;

struct SaveState {
  Level menuLevel = Level::Easy;
  bool hasGame = false;
  Game game{};
  Record record{};
};

// Writes the whole state into `out`, NUL-terminated. Returns the length, or -1
// when it does not fit (which a correct build never sees: the budget above is
// about 150 bytes over the longest possible file).
inline int packState(const SaveState& state, char* out, const int size) {
  const Game& game = state.game;
  const Record& record = state.record;
  int used = std::snprintf(out, static_cast<size_t>(size), "%d %d %lu %d %d %d %d %d %d", kStateVersion,
                           static_cast<int>(state.menuLevel), static_cast<unsigned long>(game.elapsedMs),
                           game.hintsUsed, game.notesMode != 0 ? 1 : 0, game.showRemaining != 0 ? 1 : 0,
                           game.shadePeers != 0 ? 1 : 0, game.solvedFlag != 0 ? 1 : 0, state.hasGame ? 1 : 0);
  for (int i = 0; i < kLevelCount && used > 0 && used < size; ++i) {
    used += std::snprintf(out + used, static_cast<size_t>(size - used), " %d", record.solved[i]);
  }
  for (int i = 0; i < kLevelCount && used > 0 && used < size; ++i) {
    used += std::snprintf(out + used, static_cast<size_t>(size - used), " %lu",
                          static_cast<unsigned long>(record.bestMs[i]));
  }
  if (used > 0 && used < size) {
    used += std::snprintf(out + used, static_cast<size_t>(size - used), " %d\n", record.hintsTaken);
  }
  // Three runs and three newlines, plus the terminator.
  if (used <= 0 || used + 2 * kCells + 3 * kCells + 4 > size) return -1;

  for (int cell = 0; cell < kCells; ++cell) out[used++] = static_cast<char>('0' + game.puzzle.given[cell]);
  out[used++] = '\n';
  for (int cell = 0; cell < kCells; ++cell) {
    out[used++] = static_cast<char>('0' + (game.entry[cell] <= 9 ? game.entry[cell] : 0));
  }
  out[used++] = '\n';
  static const char kHex[] = "0123456789ABCDEF";
  for (int cell = 0; cell < kCells; ++cell) {
    const Mask mask = game.note[cell];
    out[used++] = kHex[(mask >> 8) & 0xF];
    out[used++] = kHex[(mask >> 4) & 0xF];
    out[used++] = kHex[mask & 0xF];
  }
  out[used++] = '\n';
  out[used] = '\0';
  return used;
}

// Parses `text` into `out`. Parsed into locals and committed only at the end,
// so a truncated or corrupt file leaves `out` exactly as it was and returns
// false: a fresh app rather than half a puzzle.
inline bool unpackState(const char* text, SaveState& out) {
  constexpr int kHeaderCount = 18;
  long header[kHeaderCount] = {};
  const char* cursor = text;
  for (int i = 0; i < kHeaderCount; ++i) {
    char* next = nullptr;
    const long value = std::strtol(cursor, &next, 10);
    if (next == cursor) return false;
    header[i] = value;
    cursor = next;
  }
  if (header[0] != kStateVersion) return false;

  auto takeRun = [&cursor](char* run, const int length) {
    while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r') ++cursor;
    for (int i = 0; i < length; ++i) {
      if (cursor[i] == '\0') return false;
      run[i] = cursor[i];
    }
    cursor += length;
    return true;
  };
  char clues[kCells + 1] = {};
  char entries[kCells + 1] = {};
  char marks[kCells * 3 + 1] = {};
  if (!takeRun(clues, kCells) || !takeRun(entries, kCells) || !takeRun(marks, kCells * 3)) return false;

  SaveState state;
  int at = 1;
  // Range-checked rather than reduced: a negative long survives `%` as a
  // negative, and a Level(255) indexes a record array off its end.
  const long savedLevel = header[at++];
  state.menuLevel = static_cast<Level>(savedLevel >= 0 && savedLevel < kLevelCount ? savedLevel : 0);
  Game& game = state.game;
  game.elapsedMs = static_cast<uint32_t>(header[at++]);
  game.hintsUsed = static_cast<uint8_t>(header[at++]);
  game.notesMode = header[at++] != 0 ? 1 : 0;
  game.showRemaining = header[at++] != 0 ? 1 : 0;
  game.shadePeers = header[at++] != 0 ? 1 : 0;
  game.solvedFlag = header[at++] != 0 ? 1 : 0;
  state.hasGame = header[at++] != 0;
  for (int i = 0; i < kLevelCount; ++i) state.record.solved[i] = static_cast<uint16_t>(header[at++]);
  for (int i = 0; i < kLevelCount; ++i) state.record.bestMs[i] = static_cast<uint32_t>(header[at++]);
  state.record.hintsTaken = static_cast<uint16_t>(header[at++]);

  if (state.hasGame) {
    Puzzle& puzzle = game.puzzle;
    for (int cell = 0; cell < kCells; ++cell) {
      const char c = clues[cell];
      if (c < '0' || c > '9') return false;
      puzzle.given[cell] = static_cast<uint8_t>(c - '0');
    }
    sudoku::Grid grid;
    if (!sudoku::load(grid, puzzle.given)) return false;
    const sudoku::SolveReport report = sudoku::solve(grid, sudoku::ceilingFor(Level::Expert));
    if (!report.solved || report.broken) return false;
    for (int cell = 0; cell < kCells; ++cell) puzzle.solution[cell] = grid.value[cell];
    puzzle.hardest = report.hardest;
    puzzle.level = sudoku::levelOf(report.hardest);
    puzzle.clues = 0;
    for (int cell = 0; cell < kCells; ++cell) {
      if (puzzle.given[cell] != 0) ++puzzle.clues;
    }
    for (int cell = 0; cell < kCells; ++cell) {
      const char c = entries[cell];
      if (c < '0' || c > '9') return false;
      game.entry[cell] = puzzle.given[cell] != 0 ? 0 : static_cast<uint8_t>(c - '0');
      const char hex[4] = {marks[cell * 3], marks[cell * 3 + 1], marks[cell * 3 + 2], '\0'};
      game.note[cell] = static_cast<Mask>(std::strtol(hex, nullptr, 16) & kAllDigits);
    }
    // A solved flag the board does not bear out is a corrupt flag, and the
    // lock it sets would leave a playable grid that refuses every tap.
    if (game.solvedFlag != 0 && !isSolved(game)) game.solvedFlag = 0;
  } else {
    game.solvedFlag = 0;
  }

  // The menu opens on the level of the puzzle it is offering to resume; see
  // SudokuActivity::loadState for why that is a repair and not a preference.
  if (state.hasGame && game.solvedFlag == 0) state.menuLevel = game.puzzle.level;

  out = state;
  return true;
}

}  // namespace sudokuplus
