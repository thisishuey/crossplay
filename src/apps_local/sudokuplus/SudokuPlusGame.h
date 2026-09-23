#pragma once

// SUDOKU+: the same puzzles as SUDOKU, played cell first. Freestanding, like the
// rules underneath it; `Game` is also what the save file describes.
//
// The engine is not copied. Generation, grading, solving and hints all come
// from ../sudoku/SudokuCore.h, so the two games carve identical puzzles and a
// fix to the solver reaches both. What is copied is the layer the player
// touches, because that is the layer this game changes:
//
// **You pick a cell, then a digit.** `selected` is the cell and `focus` is the
// digit the board is lit for. They are separate facts: tapping a cell that
// holds a 7 focuses 7, but tapping an empty cell leaves the focus where it was,
// so you can keep reading the board for 7s while you move around it. A write
// that lands -- a digit, a note, or tapping the digit a cell already holds --
// lets go of the cell: the board is read again before the next write is aimed,
// and a selection that outlived its write would be the next tap's accident. A
// write that is refused keeps it, so the player can see where the tap went.
// ERASE is not number entry, and keeps the selection.
//
// **A note is never edited by anything but the player**, for SUDOKU's reason.
// Placing a 5 does not strike 5 from its peers' notes; `visibleNotes` hides it
// at draw time, so undo stays one cell wide and undoing the placement brings
// the marks back by itself. FILL NOTES is the one deliberate exception to
// "one cell wide": it writes many cells at once and undoes as one step, through
// the `batch` tag on each Change.
//
// **Every write goes through `commitEdit`.** That is what makes "until the next
// edit" one rule rather than a promise each path has to keep: a hint's header,
// CHECK's marks and the solved lock are all settled there.

#include "../sudoku/SudokuCore.h"
#include "SudokuPlusFlow.h"

namespace sudokuplus {

using sudoku::bitFor;
using sudoku::kAllDigits;
using sudoku::kBoxSize;
using sudoku::kCells;
using sudoku::kLevelCount;
using sudoku::kSize;
using sudoku::Level;
using sudoku::Mask;
using sudoku::Puzzle;
using sudoku::Technique;

// Slots, not steps. A FILL NOTES step can touch every empty cell -- at most 64,
// since no Sudoku has fewer than 17 clues -- so the ring has to hold one of
// those plus a useful run of single-cell steps behind it. Four bytes each.
constexpr int kUndoSlots = 96;

// "No cell". kCells rather than -1 so it survives being a uint8_t.
constexpr uint8_t kNoCell = kCells;

// One cell's worth of undo. `batch` says this change belongs to the same step
// as the change pushed immediately before it, so undo keeps popping until it
// has restored a change whose batch bit is clear -- the first of its step.
struct Change {
  uint8_t cell : 7;
  uint8_t batch : 1;
  uint8_t entry;  // what the cell held before
  Mask note;      // what was pencilled in it before
};
static_assert(sizeof(Change) == 4, "a change is four bytes");
static_assert(kCells < 128, "a cell index fits in seven bits");

// What the header says instead of the clock. Each is an answer to something the
// player asked, so each is cleared by the next edit.
enum class Notice : uint8_t {
  None = 0,
  Technique,      // HINT found a deduction; the header names its rule
  WrongDigit,     // HINT found one of your digits disagreeing with the answer
  NothingYet,     // HINT found nothing the solver can prove
  AllCorrect,     // CHECK found nothing wrong
  NothingToFill,  // FILL NOTES found every empty cell already pencilled exactly
};

struct Game {
  Puzzle puzzle;

  uint8_t entry[kCells] = {};  // the player's digits, 0 for empty
  Mask note[kCells] = {};      // their pencil marks

  Change undo[kUndoSlots] = {};
  uint8_t undoCount = 0;  // live slots, at most kUndoSlots
  uint8_t undoNext = 0;   // where the next change goes

  // The cell the next digit goes to, or kNoCell. Never saved: a board you come
  // back to has nothing selected, so the first tap cannot write somewhere you
  // chose an hour ago.
  uint8_t selected = kNoCell;
  // The digit the board is lit for, or 0 for none.
  uint8_t focus = 0;

  // Preferences that ride in the save. NOTES is a mode; the other two are the
  // MENU's toggles and default ON.
  uint8_t notesMode = 0;
  uint8_t showRemaining = 1;
  uint8_t shadePeers = 1;
  // How a pencil mark is drawn: a grey ring (a solid black dot for the focused
  // digit) whose place in the cell is its digit (1 top-left, 9 bottom-right, as
  // on the pad), or the small numeral.
  // Dots by default: a numeral at note size is hard to read on this panel.
  uint8_t noteShapes = 1;

  // CHECK's marks, shown until the next edit.
  uint8_t checkShown = 0;
  uint8_t notice = static_cast<uint8_t>(Notice::None);

  // The cell the last HINT named, and the rule that proves it.
  uint8_t hintCell = kNoCell;
  uint8_t hintTechnique = 0;
  uint8_t hintsUsed = 0;

  uint8_t solvedFlag = 0;

  uint32_t elapsedMs = 0;
};

// What a cell shows: its clue if it has one, otherwise the player's digit.
inline uint8_t valueAt(const Game& game, const int cell) {
  return game.puzzle.given[cell] != 0 ? game.puzzle.given[cell] : game.entry[cell];
}

inline bool isGiven(const Game& game, const int cell) { return game.puzzle.given[cell] != 0; }

// Which digits a cell's peers already hold.
inline Mask takenAround(const Game& game, const int cell) {
  Mask taken = 0;
  for (int other = 0; other < kCells; ++other) {
    if (!sudoku::arePeers(cell, other)) continue;
    const uint8_t value = valueAt(game, other);
    if (value != 0) taken = static_cast<Mask>(taken | bitFor(value));
  }
  return taken;
}

// The marks worth drawing: what was pencilled, minus what a peer has since
// taken. Computed rather than stored; see the header comment.
inline Mask visibleNotes(const Game& game, const int cell) {
  if (valueAt(game, cell) != 0) return 0;
  return static_cast<Mask>(game.note[cell] & ~takenAround(game, cell));
}

// A digit that clashes with a peer. Derivable from the board alone and never
// reveals the answer.
inline bool isClashing(const Game& game, const int cell) {
  const uint8_t value = valueAt(game, cell);
  if (value == 0) return false;
  for (int other = 0; other < kCells; ++other) {
    if (!sudoku::arePeers(cell, other)) continue;
    if (valueAt(game, other) == value) return true;
  }
  return false;
}

// How many of one digit are on the board, clues included.
inline int placedCount(const Game& game, const int digit) {
  int count = 0;
  for (int cell = 0; cell < kCells; ++cell) {
    if (valueAt(game, cell) == digit) ++count;
  }
  return count;
}

// How many of a digit are still to place: SHOW REMAINING's corner number.
// Never negative, because a board with ten 4s on it is a board with a clash,
// not a board owing minus one.
inline int remainingCount(const Game& game, const int digit) {
  const int left = kSize - placedCount(game, digit);
  return left > 0 ? left : 0;
}

// A player's digit that disagrees with the answer. Only CHECK and HINT read it.
inline bool isWrong(const Game& game, const int cell) {
  return game.entry[cell] != 0 && game.entry[cell] != game.puzzle.solution[cell];
}

inline int firstWrong(const Game& game) {
  for (int cell = 0; cell < kCells; ++cell) {
    if (isWrong(game, cell)) return cell;
  }
  return kNoCell;
}

inline bool isSolved(const Game& game) {
  for (int cell = 0; cell < kCells; ++cell) {
    if (valueAt(game, cell) != game.puzzle.solution[cell]) return false;
  }
  return true;
}

// A cell in the selected cell's row, column or box: SHADE PEERS. It lights
// the three units the next digit has to agree with, around the one cell you
// are about to write. Clues and copies of the focused digit keep their own
// grounds, so they are never shaded; the selected cell is, and its frame says
// which one it is.
inline bool isShadedPeer(const Game& game, const int cell) {
  if (game.shadePeers == 0 || game.selected >= kCells) return false;
  if (isGiven(game, cell)) return false;
  const uint8_t value = valueAt(game, cell);
  if (value != 0 && value == game.focus) return false;
  return cell == game.selected || sudoku::arePeers(cell, game.selected);
}

// The cell a digit would be written to: selected, and not a clue.
inline bool hasTarget(const Game& game) { return game.selected < kCells && !isGiven(game, game.selected); }

// ---------------------------------------------------------------------------
// Undo.
// ---------------------------------------------------------------------------

inline int oldestSlot(const Game& game) { return (game.undoNext + kUndoSlots - game.undoCount) % kUndoSlots; }

inline void pushChange(Game& game, const int cell, const bool batch) {
  Change& slot = game.undo[game.undoNext];
  slot.cell = static_cast<uint8_t>(cell) & 0x7F;
  slot.batch = batch ? 1 : 0;
  slot.entry = game.entry[cell];
  slot.note = game.note[cell];
  game.undoNext = static_cast<uint8_t>((game.undoNext + 1) % kUndoSlots);
  if (game.undoCount < kUndoSlots) {
    ++game.undoCount;
    return;
  }
  // The ring was full and has just overwritten its oldest change. If that was
  // the first change of a batch, the rest of the batch is now a step with no
  // beginning, and undoing it would restore half a FILL NOTES. Drop it whole.
  while (game.undoCount > 0 && game.undo[oldestSlot(game)].batch != 0) --game.undoCount;
}

inline bool canUndo(const Game& game) { return game.undoCount > 0 && game.solvedFlag == 0; }

// ---------------------------------------------------------------------------
// Edits. Every one of them ends in commitEdit.
// ---------------------------------------------------------------------------

inline void commitEdit(Game& game) {
  game.checkShown = 0;
  game.notice = static_cast<uint8_t>(Notice::None);
  game.hintCell = kNoCell;
  if (isSolved(game)) {
    // The lock. Nothing is selected on a finished board, because nothing on it
    // can be written.
    game.solvedFlag = 1;
    game.selected = kNoCell;
  }
}

inline bool undoOnce(Game& game) {
  if (!canUndo(game)) return false;
  while (game.undoCount > 0) {
    game.undoNext = static_cast<uint8_t>((game.undoNext + kUndoSlots - 1) % kUndoSlots);
    const Change& change = game.undo[game.undoNext];
    game.entry[change.cell] = change.entry;
    game.note[change.cell] = change.note;
    --game.undoCount;
    if (change.batch == 0) break;
  }
  commitEdit(game);
  return true;
}

// A tap on a cell. It selects, or deselects the cell already selected. A cell
// holding a digit also focuses that digit, which is what makes tapping the
// board a way of reading it.
inline bool tapCell(Game& game, const int cell) {
  if (cell < 0 || cell >= kCells || game.solvedFlag != 0) return false;
  if (game.selected == cell) {
    game.selected = kNoCell;
    return true;
  }
  game.selected = static_cast<uint8_t>(cell);
  const uint8_t value = valueAt(game, cell);
  if (value != 0) game.focus = value;
  return true;
}

// A tap on a pad key. With a writable cell selected it writes (or, in NOTES,
// pencils) and then deselects the cell; with nothing to write to it only moves
// the focus, so the pad is also the way to ask "where are the 6s". A refused
// note keeps the selection.
inline bool tapDigit(Game& game, const int digit) {
  if (digit < 1 || digit > kSize || game.solvedFlag != 0) return false;
  if (!hasTarget(game)) {
    game.focus = game.focus == digit ? 0 : static_cast<uint8_t>(digit);
    return true;
  }
  const int cell = game.selected;
  if (game.notesMode != 0) {
    // A mark on a cell that already holds a digit would be invisible, and a
    // write nobody can see is worse than none.
    if (game.entry[cell] != 0) return false;
    // Likewise a mark for a digit a peer already holds: visibleNotes would hide
    // it the moment it was written.
    if ((takenAround(game, cell) & bitFor(digit)) != 0) return false;
    pushChange(game, cell, false);
    game.note[cell] = static_cast<Mask>(game.note[cell] ^ bitFor(digit));
    commitEdit(game);
    game.selected = kNoCell;
    return true;
  }
  pushChange(game, cell, false);
  if (game.entry[cell] == digit) {
    game.entry[cell] = 0;
  } else {
    game.entry[cell] = static_cast<uint8_t>(digit);
    game.note[cell] = 0;
  }
  game.focus = static_cast<uint8_t>(digit);
  commitEdit(game);
  game.selected = kNoCell;
  return true;
}

inline bool canErase(const Game& game) {
  if (game.solvedFlag != 0 || !hasTarget(game)) return false;
  // Visible marks, not stored ones: a cell whose only marks are hidden by its
  // peers LOOKS empty, and an ERASE that changed nothing visible would read as
  // a button that did nothing.
  return game.entry[game.selected] != 0 || visibleNotes(game, game.selected) != 0;
}

inline bool erase(Game& game) {
  if (!canErase(game)) return false;
  const int cell = game.selected;
  pushChange(game, cell, false);
  game.entry[cell] = 0;
  game.note[cell] = 0;
  commitEdit(game);
  return true;
}

// Pencil every legal candidate into every empty cell, as ONE undo step. A cell
// whose marks already say exactly that is left out of the step, so undo
// restores only what the fill changed. Returns false when it changed nothing.
inline bool fillNotes(Game& game) {
  if (game.solvedFlag != 0) return false;
  bool first = true;
  for (int cell = 0; cell < kCells; ++cell) {
    if (valueAt(game, cell) != 0) continue;
    const Mask candidates = static_cast<Mask>(kAllDigits & ~takenAround(game, cell));
    if (game.note[cell] == candidates) continue;
    pushChange(game, cell, !first);
    first = false;
    game.note[cell] = candidates;
  }
  if (first) return false;
  commitEdit(game);
  return true;
}

// CHECK. Marks the player's digits that disagree with the answer, or says that
// none do. Not an edit: it changes nothing on the board.
inline void check(Game& game) {
  if (game.solvedFlag != 0) return;
  game.notice = static_cast<uint8_t>(Notice::None);
  if (firstWrong(game) == kNoCell) {
    game.checkShown = 0;
    game.notice = static_cast<uint8_t>(Notice::AllCorrect);
    return;
  }
  game.checkShown = 1;
}

// HINT. A wrong digit comes first, because any deduction made past one is a
// lie; otherwise the solver's next step. Either way the cell is SELECTED, so
// the next digit tapped goes straight into it. The rule is named, the digit is
// not: a hint that filled the cell would be a solve button.
inline void takeHint(Game& game) {
  if (game.solvedFlag != 0) return;
  const int wrong = firstWrong(game);
  if (wrong != kNoCell) {
    game.hintCell = static_cast<uint8_t>(wrong);
    game.selected = static_cast<uint8_t>(wrong);
    game.notice = static_cast<uint8_t>(Notice::WrongDigit);
    if (game.hintsUsed < 0xFF) ++game.hintsUsed;
    return;
  }
  uint8_t board[kCells];
  for (int cell = 0; cell < kCells; ++cell) board[cell] = valueAt(game, cell);
  const sudoku::Hint hint = sudoku::nextHint(board, sudoku::ceilingFor(Level::Expert));
  if (!hint.found) {
    game.notice = static_cast<uint8_t>(Notice::NothingYet);
    return;
  }
  game.hintCell = static_cast<uint8_t>(hint.cell);
  game.hintTechnique = static_cast<uint8_t>(hint.technique);
  game.selected = static_cast<uint8_t>(hint.cell);
  game.notice = static_cast<uint8_t>(Notice::Technique);
  // Saturating: a uint8_t that wrapped to 0 would read as an unaided solve
  // and set a best time.
  if (game.hintsUsed < 0xFF) ++game.hintsUsed;
}

// The longest thing the header's right label is asked to hold. Wider than
// sudoku::kMaxNoticeChars, which is SUDOKU's 244px rail capsule: here the label
// sits in the header band beside the seven-character title. Asserted over
// every notice in host-tests/sudokuplus.
constexpr int kMaxHeaderNoticeChars = 15;

// What the header says in place of the clock, or nullptr for the clock.
inline const char* noticeText(const Game& game) {
  switch (static_cast<Notice>(game.notice)) {
    case Notice::None:
      return nullptr;
    case Notice::Technique:
      return sudoku::techniqueName(static_cast<Technique>(game.hintTechnique));
    case Notice::WrongDigit:
      return "WRONG DIGIT";
    case Notice::NothingYet:
      return "NOTHING YET";
    case Notice::AllCorrect:
      return "ALL CORRECT";
    case Notice::NothingToFill:
      return "NOTHING TO FILL";
  }
  return nullptr;
}

// One row of the MENU panel, applied. Returns whether the panel stays open.
//
// The three questions close it, because each answer is ON the board: a
// selected cell and a rule in the header, a sheet of notes, a set of struck
// digits. The toggles keep it open, so the row itself shows the new state.
inline bool applyPanelRow(Game& game, const PanelRow row) {
  switch (row) {
    case PanelRow::Hint:
      takeHint(game);
      return false;
    case PanelRow::FillNotes:
      // A fill with nothing to change still answers, or the panel would close
      // on a tap that visibly did nothing.
      if (!fillNotes(game) && game.solvedFlag == 0) game.notice = static_cast<uint8_t>(Notice::NothingToFill);
      return false;
    case PanelRow::Check:
      check(game);
      return false;
    case PanelRow::ShowRemaining:
      game.showRemaining = game.showRemaining != 0 ? 0 : 1;
      return true;
    case PanelRow::ShadePeers:
      game.shadePeers = game.shadePeers != 0 ? 0 : 1;
      return true;
    case PanelRow::NoteStyle:
      game.noteShapes = game.noteShapes != 0 ? 0 : 1;
      return true;
    case PanelRow::Close:
      return false;
    case PanelRow::Count:
      break;
  }
  return true;
}

// ---------------------------------------------------------------------------
// The front door, and the record. As SUDOKU has them; see SudokuGame.h for why
// the offer is derived rather than latched.
// ---------------------------------------------------------------------------

enum class MenuOffer : uint8_t {
  Fresh,       // nothing saved
  Resume,      // an unsolved game at the level the menu is showing
  Solved,      // the saved game is finished; the door replaces it
  OtherLevel,  // a saved game, but the menu is pointed somewhere else
};

inline MenuOffer menuOffer(const Game& game, const bool hasGame, const Level menuLevel) {
  if (!hasGame) return MenuOffer::Fresh;
  if (menuLevel != game.puzzle.level) return MenuOffer::OtherLevel;
  return game.solvedFlag != 0 ? MenuOffer::Solved : MenuOffer::Resume;
}

inline bool canResume(const Game& game, const bool hasGame, const Level menuLevel) {
  return menuOffer(game, hasGame, menuLevel) == MenuOffer::Resume;
}

// A fresh board. The two MENU toggles are preferences rather than part of a
// puzzle, so they survive; everything else starts over, NOTES included.
inline void startGame(Game& game, const Puzzle& puzzle) {
  const uint8_t showRemaining = game.showRemaining;
  const uint8_t shadePeers = game.shadePeers;
  const uint8_t noteShapes = game.noteShapes;
  game = Game{};
  game.puzzle = puzzle;
  game.showRemaining = showRemaining;
  game.shadePeers = shadePeers;
  game.noteShapes = noteShapes;
}

struct Record {
  uint16_t solved[kLevelCount] = {};
  uint32_t bestMs[kLevelCount] = {};
  uint16_t hintsTaken = 0;
};

inline void recordSolve(Record& record, const Level level, const uint32_t elapsedMs, const int hintsUsed) {
  const int index = static_cast<int>(level);
  if (record.solved[index] < 0xFFFF) ++record.solved[index];
  // A hinted solve sets no best time: the clock is a claim about you.
  if (hintsUsed == 0 && (record.bestMs[index] == 0 || elapsedMs < record.bestMs[index])) {
    record.bestMs[index] = elapsedMs;
  }
  record.hintsTaken = static_cast<uint16_t>(record.hintsTaken + hintsUsed);
}

inline int totalSolved(const Record& record) {
  int total = 0;
  for (int i = 0; i < kLevelCount; ++i) total += record.solved[i];
  return total;
}

}  // namespace sudokuplus
