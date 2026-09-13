#pragma once

// What Hex writes down, and how. Freestanding: no storage, no renderer, so the
// round trip is host-tested rather than discovered on a device.
//
// Three things are saved and they have different lifetimes. The RECORD and the
// SETTINGS outlive every game; the LAST FINISHED position is the front door's
// ornament; the GAME IN PROGRESS is one position and is cleared the moment it
// ends. Wavelength shipped an onExit() that wrote the first and not the last,
// and a cold tester lost a round by pressing Home one key from Back -- a field
// that is never written cannot be recovered by fixing when the write happens.
//
// **A short line is accepted, a wrong version is not.** The two failures are
// not the same shape. A file written by a build with fewer fields is missing a
// TAIL, so everything it does carry is still in the place this build looks for
// it and the rest can take its default -- which is how a player who set HARD
// once keeps it across the release that added the ornament. A file with a
// different version number is a file whose fields may have MOVED, and reading
// one of those as far as it goes does not fail at the missing number: it reads
// the next one in its place and shifts a whole board along by one. That is a
// game that loads and is wrong, which is worse than one that does not load.
//
// So `unpack` walks forward through three checkpoints -- settings, ornament,
// game -- and commits whatever it reached. Nothing before a checkpoint is ever
// half-applied: the whole thing is parsed into a local and copied over at the
// end.

#include <cstdint>

#include "HexCore.h"
#include "HexFlow.h"

namespace hexsave {

// Bumped whenever a field is INSERTED or its meaning changes. Appending to the
// end does not need it, which is the whole point of the checkpoints above.
constexpr int kVersion = 1;

struct Save {
  int wins = 0;
  int losses = 0;

  // The settings, because a player who chose HARD once meant it.
  hex::Opponent opponent = hex::Opponent::Computer;
  hex::Level level = hex::Level::Normal;
  uint8_t playAs = hex::kBlack;

  // The last finished game, for the front door's ornament. The packed board
  // only: nothing plays on it, so it needs no union-find and no turn.
  bool hasHistory = false;
  uint8_t lastCells[hex::kCellBytes] = {};
  bool lastWon = false;

  // A game part-played. `inProgress` is false when there is nothing to resume,
  // and `game` is then not read at all.
  bool inProgress = false;
  hex::Game game{};
  uint8_t seat = hex::kBlack;
};

// Text, space separated, one line. Returns the bytes written, or 0 when the
// buffer could not hold it -- never a truncated line, because a truncated line
// parses as a shorter save rather than as a failure, and this file's whole
// tolerance rule is built on a short line being honest.
int pack(const Save& save, char* out, int capacity);

// Parses what pack() wrote, including a line written by a build with fewer
// fields. Returns false and leaves `save` untouched only when the version does
// not match or the very first numbers are missing.
bool unpack(const char* text, Save& save);

}  // namespace hexsave
