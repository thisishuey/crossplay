// SUDOKU+ game tests. The layer the player touches, over the shared engine.
//
//   host-tests/sudokuplus/run.sh
//
// The engine itself -- generation, grading, uniqueness, hints -- is proved in
// host-tests/sudoku and is not re-proved here. What this file owns is every row
// of the spec's I/O matrix, the undo rules (one cell wide, except FILL NOTES),
// "until the next edit", the solved lock and the save round trip.

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "SudokuPlusGame.h"
#include "SudokuPlusSave.h"

namespace {

namespace sp = sudokuplus;

int checks = 0;
int failures = 0;

void check(const bool ok, const char* what) {
  ++checks;
  if (ok) return;
  ++failures;
  std::printf("  FAIL %s\n", what);
}

void checkEq(const long got, const long want, const char* what) {
  ++checks;
  if (got == want) return;
  ++failures;
  std::printf("  FAIL %s: got %ld want %ld\n", what, got, want);
}

sudoku::Workspace& workspace() {
  static sudoku::Workspace work;
  return work;
}

sp::Game aGame(const sudoku::Level level, const uint32_t seed) {
  uint32_t rng = seed;
  sudoku::Puzzle puzzle;
  check(sudoku::generate(puzzle, level, workspace(), rng, 400), "generated a puzzle");
  sp::Game game;
  sp::startGame(game, puzzle);
  return game;
}

int firstEmpty(const sp::Game& game, const int from = 0) {
  for (int cell = from; cell < sp::kCells; ++cell) {
    if (sp::valueAt(game, cell) == 0) return cell;
  }
  return -1;
}

int firstGiven(const sp::Game& game) {
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (sp::isGiven(game, cell)) return cell;
  }
  return -1;
}

bool sameBoard(const sp::Game& a, const sp::Game& b) {
  return std::memcmp(a.entry, b.entry, sizeof(a.entry)) == 0 && std::memcmp(a.note, b.note, sizeof(a.note)) == 0;
}

// The nth digit (from 0) that no peer of `cell` holds, or 0. NOTES refuses
// the rest, so a test that pencils has to pick from these.
int openDigit(const sp::Game& game, const int cell, int nth) {
  const sp::Mask taken = sp::takenAround(game, cell);
  for (int digit = 1; digit <= sp::kSize; ++digit) {
    if ((taken & sp::bitFor(digit)) != 0) continue;
    if (nth-- == 0) return digit;
  }
  return 0;
}

// A digit that is legal in `cell` and is not its answer, or 0.
int aWrongButLegalDigit(const sp::Game& game, const int cell) {
  const sp::Mask taken = sp::takenAround(game, cell);
  for (int digit = 1; digit <= sp::kSize; ++digit) {
    if (digit == game.puzzle.solution[cell]) continue;
    if ((taken & sp::bitFor(digit)) == 0) return digit;
  }
  return 0;
}

// --- the matrix --------------------------------------------------------------

void testSelect() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1001u);
  check(game.selected == sp::kNoCell, "a fresh board has nothing selected");
  checkEq(game.focus, 0, "and no focus");

  const int empty = firstEmpty(game);
  check(sp::tapCell(game, empty), "tapping an empty cell does something");
  checkEq(game.selected, empty, "it selects that cell");
  checkEq(game.focus, 0, "an empty cell leaves the focus alone");

  const int given = firstGiven(game);
  check(sp::tapCell(game, given), "tapping a clue does something");
  checkEq(game.selected, given, "a clue can be selected");
  checkEq(game.focus, game.puzzle.given[given], "and it focuses its digit");

  // The focus survives moving to an empty cell: the board stays lit for it.
  sp::tapCell(game, empty);
  checkEq(game.focus, game.puzzle.given[given], "moving to an empty cell keeps the focus");

  check(sp::tapCell(game, empty), "tapping the selected cell again does something");
  checkEq(game.selected, sp::kNoCell, "it deselects");
  checkEq(game.undoCount, 0, "selection is never an undo step");

  // A player's digit focuses too.
  sp::tapCell(game, empty);
  sp::tapDigit(game, 4);
  sp::tapCell(game, empty);  // deselect
  game.focus = 0;
  sp::tapCell(game, empty);
  checkEq(game.focus, 4, "tapping your own digit focuses it");
}

void testEnterAndSameDigit() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1002u);
  const int cell = firstEmpty(game);
  game.note[cell] = static_cast<sp::Mask>(sp::bitFor(2) | sp::bitFor(7));
  sp::tapCell(game, cell);
  check(sp::tapDigit(game, 6), "a digit with a writable cell selected writes");
  checkEq(game.entry[cell], 6, "the entry becomes the digit");
  checkEq(game.note[cell], 0, "the cell's notes clear");
  checkEq(game.focus, 6, "focus becomes the digit");
  checkEq(game.undoCount, 1, "one undo step");
  checkEq(game.selected, cell, "the cell stays selected");

  // Overwriting is one step as well.
  sp::tapDigit(game, 8);
  checkEq(game.entry[cell], 8, "another digit overwrites");
  checkEq(game.undoCount, 2, "one more step");

  check(sp::tapDigit(game, 8), "the same digit again does something");
  checkEq(game.entry[cell], 0, "the same digit clears the entry");

  check(sp::undoOnce(game), "undo the clear");
  checkEq(game.entry[cell], 8, "undo brings the 8 back");
  sp::undoOnce(game);
  checkEq(game.entry[cell], 6, "undo walks back one cell at a time");
  sp::undoOnce(game);
  checkEq(game.entry[cell], 0, "and back to empty");
  checkEq(game.note[cell], static_cast<sp::Mask>(sp::bitFor(2) | sp::bitFor(7)), "with the notes the entry wiped");
  check(!sp::undoOnce(game), "an exhausted undo says so");
}

void testNotes() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1003u);
  const int cell = firstEmpty(game);
  const int a = openDigit(game, cell, 0);
  const int b = openDigit(game, cell, 1);
  check(a != 0 && b != 0, "two digits the cell can take");
  game.notesMode = 1;
  sp::tapCell(game, cell);
  const uint8_t focusBefore = game.focus;
  check(sp::tapDigit(game, a), "a digit in NOTES pencils");
  checkEq(game.note[cell], sp::bitFor(a), "the mark is set");
  checkEq(game.entry[cell], 0, "and nothing is written");
  checkEq(game.undoCount, 1, "one undo step");
  checkEq(game.focus, focusBefore, "pencilling does not move the focus");

  sp::tapDigit(game, b);
  checkEq(game.note[cell], static_cast<sp::Mask>(sp::bitFor(a) | sp::bitFor(b)), "marks accumulate");
  sp::tapDigit(game, a);
  checkEq(game.note[cell], sp::bitFor(b), "the same digit rubs its mark out");

  // Undo of a note.
  check(sp::undoOnce(game), "undo a note");
  checkEq(game.note[cell], static_cast<sp::Mask>(sp::bitFor(a) | sp::bitFor(b)), "undo restores the mark");
  sp::undoOnce(game);
  sp::undoOnce(game);
  checkEq(game.note[cell], 0, "and back to no marks");

  // Note on a filled cell: nothing happens.
  game.notesMode = 0;
  sp::tapDigit(game, 9);
  game.notesMode = 1;
  const sp::Game before = game;
  check(!sp::tapDigit(game, 2), "a note on a cell holding a digit does nothing");
  check(sameBoard(before, game), "the board is unchanged");
  checkEq(game.undoCount, before.undoCount, "and no undo step is spent");
}

void testNoTarget() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1004u);
  const sp::Game before = game;
  check(sp::tapDigit(game, 7), "a digit with nothing selected does something");
  checkEq(game.focus, 7, "it focuses the digit");
  check(sameBoard(before, game), "and writes nothing");
  sp::tapDigit(game, 7);
  checkEq(game.focus, 0, "the same digit again clears the focus");
  sp::tapDigit(game, 7);
  sp::tapDigit(game, 2);
  checkEq(game.focus, 2, "another digit moves the focus");

  // A clue selected is no target either, in either mode.
  for (int notes = 0; notes <= 1; ++notes) {
    sp::Game onClue = aGame(sudoku::Level::Easy, 0x1004u);
    onClue.notesMode = static_cast<uint8_t>(notes);
    sp::tapCell(onClue, firstGiven(onClue));
    const int clue = onClue.puzzle.given[onClue.selected];
    const int other = clue == 9 ? 1 : clue + 1;
    sp::tapDigit(onClue, other);
    checkEq(onClue.focus, other, "a digit on a selected clue focuses");
    check(sameBoard(before, onClue), "and never writes");
    checkEq(onClue.undoCount, 0, "and spends no undo");
  }
}

void testErase() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1005u);
  const int cell = firstEmpty(game);
  check(!sp::canErase(game), "nothing selected: ERASE is disabled");
  sp::tapCell(game, cell);
  check(!sp::canErase(game), "an empty, unmarked cell: ERASE is disabled");
  check(!sp::erase(game), "and does nothing");
  sp::tapCell(game, cell);
  sp::tapCell(game, firstGiven(game));
  check(!sp::canErase(game), "a clue: ERASE is disabled");
  sp::tapCell(game, cell);

  game.notesMode = 1;
  const int a = openDigit(game, cell, 0);
  const int b = openDigit(game, cell, 1);
  sp::tapDigit(game, a);
  sp::tapDigit(game, b);
  check(sp::canErase(game), "a marked cell can be erased");
  const uint8_t steps = game.undoCount;
  check(sp::erase(game), "erase");
  checkEq(game.note[cell], 0, "erase clears the notes");
  checkEq(game.undoCount, steps + 1, "one undo step");
  sp::undoOnce(game);
  checkEq(game.note[cell], static_cast<sp::Mask>(sp::bitFor(a) | sp::bitFor(b)), "undo restores them");

  game.notesMode = 0;
  sp::tapDigit(game, 5);
  check(sp::erase(game), "erase an entry");
  checkEq(game.entry[cell], 0, "the entry clears");
  checkEq(game.note[cell], 0, "and so do the notes");
  check(!sp::canErase(game), "and then there is nothing left to erase");
}

// The shading rule restated independently: every empty cell that shares a row,
// column or box with a cell holding the focused digit, walked by coordinates
// rather than through arePeers.
bool independentlyShaded(const sp::Game& game, const int cell) {
  if (game.shadePeers == 0 || game.selected >= sp::kCells) return false;
  if (sp::isGiven(game, cell)) return false;
  const uint8_t value = sp::valueAt(game, cell);
  if (value != 0 && value == game.focus) return false;
  const int row = cell / 9;
  const int column = cell % 9;
  const int r = game.selected / 9;
  const int c = game.selected % 9;
  return r == row || c == column || (r / 3 == row / 3 && c / 3 == column / 3);
}

void testFocusShading() {
  sp::Game game = aGame(sudoku::Level::Medium, 0x1006u);
  for (int selected = 0; selected < sp::kCells; ++selected) {
    game.selected = static_cast<uint8_t>(selected);
    game.focus = static_cast<uint8_t>(1 + selected % sp::kSize);
    int shaded = 0;
    bool agrees = true;
    for (int cell = 0; cell < sp::kCells; ++cell) {
      const bool got = sp::isShadedPeer(game, cell);
      if (got != independentlyShaded(game, cell)) agrees = false;
      if (got) ++shaded;
      // A clue keeps its DarkGray and the focused digit its black.
      if (got && (sp::isGiven(game, cell) || sp::valueAt(game, cell) == game.focus)) agrees = false;
    }
    check(agrees, "shading is exactly the selected cell's row, column and box, less clues and the focus");
    check(shaded > 0 && shaded <= 21, "a selection shades at most its 21 cells");
  }
  // The selection's units, and nothing that merely shares a digit with it.
  game.selected = 0;
  game.focus = 0;
  check(!sp::isShadedPeer(game, 80), "a cell outside the selection's units stays paper");
  game.shadePeers = 0;
  bool none = true;
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (sp::isShadedPeer(game, cell)) none = false;
  }
  check(none, "SHADE PEERS off shades nothing");
  game.shadePeers = 1;
  game.selected = sp::kNoCell;
  game.focus = 5;
  none = true;
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (sp::isShadedPeer(game, cell)) none = false;
  }
  check(none, "a focus with nothing selected shades nothing");
}

void testRemaining() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1007u);
  for (int digit = 1; digit <= sp::kSize; ++digit) {
    checkEq(sp::remainingCount(game, digit) + sp::placedCount(game, digit), sp::kSize,
            "remaining and placed make nine");
  }
  // Ten of a digit is a clash, not minus one to go.
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (!sp::isGiven(game, cell)) game.entry[cell] = 1;
  }
  checkEq(sp::remainingCount(game, 1), 0, "remaining never goes negative");
}

void testFillNotesIsOneStep() {
  sp::Game game = aGame(sudoku::Level::Hard, 0x1008u);
  const int a = firstEmpty(game);
  const int b = firstEmpty(game, a + 1);
  sp::tapCell(game, a);
  game.notesMode = 1;
  sp::tapDigit(game, game.puzzle.solution[a]);
  sp::tapCell(game, b);
  game.notesMode = 0;
  sp::tapDigit(game, game.puzzle.solution[b]);
  const sp::Game before = game;

  check(sp::fillNotes(game), "FILL NOTES changes something");
  check(game.undoCount > before.undoCount + 1, "the ring holds the batch, one slot per changed cell");
  bool legal = true;
  bool complete = true;
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (sp::valueAt(game, cell) != 0) {
      if (game.note[cell] != before.note[cell]) legal = false;  // a filled cell is not touched
      continue;
    }
    const sp::Mask want = static_cast<sp::Mask>(sp::kAllDigits & ~sp::takenAround(game, cell));
    if (game.note[cell] != want) complete = false;
    // The answer is always a candidate, or the fill would be lying.
    if ((game.note[cell] & sp::bitFor(game.puzzle.solution[cell])) == 0) legal = false;
  }
  check(complete, "every empty cell holds exactly its legal candidates");
  check(legal, "filled cells are untouched and every answer survives");
  check(!sp::fillNotes(game), "a second fill changes nothing and says so");

  check(sp::undoOnce(game), "undo the fill");
  check(sameBoard(before, game), "one undo restores every note the fill changed");
  checkEq(game.undoCount, before.undoCount, "and spends exactly one step's worth");
  sp::undoOnce(game);
  checkEq(game.entry[b], 0, "the step before the fill is still there");
}

// A FILL NOTES step pushed far enough back is dropped WHOLE. Undoing half a
// fill would leave the board in a state nobody ever saw.
void testAFillNeverUndoesByHalves() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1009u);
  check(sp::fillNotes(game), "fill");
  const sp::Game filled = game;
  const int fillSlots = game.undoCount;
  check(fillSlots > 1 && fillSlots < sp::kUndoSlots, "a fill is several slots and fits the ring");
  const int cell = firstEmpty(game);
  sp::tapCell(game, cell);
  game.notesMode = 1;
  // Exactly enough single steps to overwrite the fill's FIRST slot and no
  // more, which leaves the rest of the fill in the ring with no beginning.
  // That is the only shape of wrap that can undo a fill by halves: more steps
  // than this evict the whole fill anyway.
  const int steps = sp::kUndoSlots - fillSlots + 1;
  // One open digit toggled on and off: every tap is a real, single-cell step.
  const int digit = openDigit(game, cell, 0);
  for (int i = 0; i < steps; ++i) sp::tapDigit(game, digit);
  int undone = 0;
  while (sp::undoOnce(game)) ++undone;
  checkEq(undone, steps, "the orphaned tail of the fill is not an undo step");
  bool whole = true;
  for (int other = 0; other < sp::kCells; ++other) {
    if (other == cell) continue;
    if (game.note[other] != filled.note[other]) whole = false;
  }
  check(whole, "the fill is either all there or all undone, never half");
}

void testCheckClearsOnTheNextEdit() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Au);
  sp::check(game);
  checkEq(game.notice, static_cast<int>(sp::Notice::AllCorrect), "an untouched board is all correct");
  check(std::strcmp(sp::noticeText(game), "ALL CORRECT") == 0, "and says so");
  checkEq(game.checkShown, 0, "with nothing marked");

  const int cell = firstEmpty(game);
  const int wrong = aWrongButLegalDigit(game, cell);
  check(wrong != 0, "found a wrong digit that clashes with nothing");
  sp::tapCell(game, cell);
  checkEq(game.notice, static_cast<int>(sp::Notice::AllCorrect), "selecting is not an edit");
  sp::tapDigit(game, wrong);
  checkEq(game.notice, 0, "an edit clears ALL CORRECT");

  sp::check(game);
  checkEq(game.checkShown, 1, "CHECK marks a wrong digit");
  check(sp::isWrong(game, cell), "the digit is the wrong one");
  check(sp::noticeText(game) == nullptr, "and the header keeps its clock");

  const int other = firstEmpty(game, cell + 1);
  sp::tapCell(game, other);
  checkEq(game.checkShown, 1, "moving the selection keeps the marks");
  game.notesMode = 1;
  sp::tapDigit(game, openDigit(game, other, 0));
  checkEq(game.checkShown, 0, "the next edit, even a note, clears them");

  sp::check(game);
  sp::undoOnce(game);
  checkEq(game.checkShown, 0, "undo is an edit too");
  sp::check(game);
  sp::tapCell(game, cell);
  sp::erase(game);
  checkEq(game.checkShown, 0, "and so is ERASE");
}

void testHintSelectsAndClearsOnEdit() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Bu);
  sp::takeHint(game);
  checkEq(game.notice, static_cast<int>(sp::Notice::Technique), "a hint names a rule");
  check(game.hintCell < sp::kCells, "and a cell");
  checkEq(game.selected, game.hintCell, "the hint cell is selected");
  checkEq(game.hintsUsed, 1, "it counts");
  check(sp::noticeText(game) != nullptr && sp::noticeText(game)[0] != '\0', "the header has the rule's name");
  checkEq(sp::valueAt(game, game.hintCell), 0, "the hint never fills the cell");

  // Tapping the answer goes straight in, because the cell is selected.
  const int hinted = game.hintCell;
  sp::tapDigit(game, game.puzzle.solution[hinted]);
  checkEq(game.entry[hinted], game.puzzle.solution[hinted], "the next digit lands in the hint cell");
  checkEq(game.notice, 0, "and the edit clears the header");
  checkEq(game.hintCell, sp::kNoCell, "and the hint");

  // A wrong digit outranks any deduction.
  const int cell = firstEmpty(game);
  sp::tapCell(game, cell);
  sp::tapDigit(game, aWrongButLegalDigit(game, cell));
  sp::tapCell(game, cell);  // deselect
  sp::takeHint(game);
  checkEq(game.notice, static_cast<int>(sp::Notice::WrongDigit), "a wrong digit is named first");
  checkEq(game.selected, cell, "and selected");
}

void testTheSolvedBoardLocks() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Cu);
  int last = -1;
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (sp::isGiven(game, cell)) continue;
    if (last >= 0) game.entry[last] = game.puzzle.solution[last];
    last = cell;
  }
  check(!sp::isSolved(game), "one cell short is not solved");
  sp::tapCell(game, last);
  sp::tapDigit(game, game.puzzle.solution[last]);
  checkEq(game.solvedFlag, 1, "the last correct digit solves it");
  checkEq(game.selected, sp::kNoCell, "and clears the selection");

  const sp::Game locked = game;
  check(!sp::tapCell(game, last), "a cell tap is refused");
  check(!sp::tapDigit(game, 3), "a digit tap is refused");
  check(!sp::erase(game), "erase is refused");
  check(!sp::canUndo(game) && !sp::undoOnce(game), "undo is refused");
  check(!sp::fillNotes(game), "fill notes is refused");
  sp::takeHint(game);
  sp::check(game);
  check(sameBoard(locked, game), "nothing writes to a solved board");
  checkEq(game.focus, locked.focus, "not even the focus moves");
  checkEq(game.hintsUsed, locked.hintsUsed, "and a hint is not spent on it");
}

void testClashesAreDerived() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Du);
  const int given = firstGiven(game);
  int peer = -1;
  for (int cell = 0; cell < sp::kCells && peer < 0; ++cell) {
    if (!sp::isGiven(game, cell) && sudoku::arePeers(cell, given)) peer = cell;
  }
  check(peer >= 0, "a clue has an empty peer");
  sp::tapCell(game, peer);
  sp::tapDigit(game, game.puzzle.given[given]);
  check(sp::isClashing(game, peer), "a copy of a peer's digit clashes");
  check(sp::isClashing(game, given), "and so does the clue it copies");
  sp::undoOnce(game);
  check(!sp::isClashing(game, given), "undo clears the clash");
}

void testVisibleNotesHideWhatAPeerTook() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Eu);
  const int a = firstEmpty(game);
  int b = -1;
  for (int cell = 0; cell < sp::kCells && b < 0; ++cell) {
    if (cell != a && sp::valueAt(game, cell) == 0 && sudoku::arePeers(a, cell)) b = cell;
  }
  check(b >= 0, "two empty peers");
  // A digit neither cell's peers hold yet, so the mark starts visible.
  const sp::Mask open = static_cast<sp::Mask>(sp::kAllDigits & ~sp::takenAround(game, a) & ~sp::takenAround(game, b));
  check(open != 0, "a digit both cells could take");
  const int digit = sudoku::lowestDigit(open);
  game.note[b] = sp::kAllDigits;
  check((sp::visibleNotes(game, b) & sp::bitFor(digit)) != 0, "the mark starts visible");
  sp::tapCell(game, a);
  sp::tapDigit(game, digit);
  check((sp::visibleNotes(game, b) & sp::bitFor(digit)) == 0, "a placed digit hides that mark in its peers");
  checkEq(game.note[b], sp::kAllDigits, "without rewriting the stored notes");
  sp::undoOnce(game);
  check((sp::visibleNotes(game, b) & sp::bitFor(digit)) != 0, "undo brings the mark back by itself");
}

void testStartGameKeepsThePreferences() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Fu);
  game.showRemaining = 0;
  game.shadePeers = 0;
  game.notesMode = 1;
  game.focus = 3;
  game.selected = 10;
  const sudoku::Puzzle next = aGame(sudoku::Level::Easy, 0x2000u).puzzle;
  sp::startGame(game, next);
  checkEq(game.showRemaining, 0, "SHOW REMAINING survives a new puzzle");
  checkEq(game.shadePeers, 0, "and so does SHADE PEERS");
  checkEq(game.notesMode, 0, "NOTES starts off");
  checkEq(game.selected, sp::kNoCell, "nothing selected");
  checkEq(game.focus, 0, "nothing focused");

  const sp::Game fresh{};
  checkEq(fresh.showRemaining, 1, "SHOW REMAINING defaults on");
  checkEq(fresh.shadePeers, 1, "SHADE PEERS defaults on");
}

void testTheRecordOnlyTimesUnhintedSolves() {
  sp::Record record;
  sp::recordSolve(record, sudoku::Level::Medium, 500000, 1);
  checkEq(record.solved[1], 1, "a hinted solve counts");
  checkEq(static_cast<long>(record.bestMs[1]), 0, "but sets no best time");
  sp::recordSolve(record, sudoku::Level::Medium, 600000, 0);
  checkEq(static_cast<long>(record.bestMs[1]), 600000, "an unhinted one does");
  sp::recordSolve(record, sudoku::Level::Medium, 400000, 2);
  checkEq(static_cast<long>(record.bestMs[1]), 600000, "a faster hinted one does not beat it");
  checkEq(sp::totalSolved(record), 3, "every solve counts");
  checkEq(record.hintsTaken, 3, "hints add up");
}

void testEveryNoticeFitsTheHeader() {
  // Every Notice value up to the last one, so a new notice is measured the day
  // it is added.
  sp::Game game;
  const int last = static_cast<int>(sp::Notice::NothingToFill);
  for (int n = 1; n <= last; ++n) {
    game.notice = static_cast<uint8_t>(n);
    for (int t = 0; t < sudoku::kTechniqueCount; ++t) {
      game.hintTechnique = static_cast<uint8_t>(t);
      const char* text = sp::noticeText(game);
      check(text != nullptr && text[0] != '\0', "every notice has text");
      check(text != nullptr && static_cast<int>(std::strlen(text)) <= sp::kMaxHeaderNoticeChars,
            "every notice fits the header");
    }
  }
  game.notice = static_cast<uint8_t>(sp::Notice::NothingToFill);
  check(std::strcmp(sp::noticeText(game), "NOTHING TO FILL") == 0, "FILL NOTES with nothing to do says so");
}

// A hint count that wrapped to zero would read as an unaided solve and set a
// best time. Both of takeHint's counting branches saturate.
void testHintsUsedSaturates() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x100Au);
  game.hintsUsed = 0xFF;
  sp::takeHint(game);
  checkEq(game.notice, static_cast<int>(sp::Notice::Technique), "the deduction branch ran");
  checkEq(game.hintsUsed, 0xFF, "a deduction hint saturates");

  const int cell = firstEmpty(game);
  sp::tapCell(game, cell);
  sp::tapDigit(game, aWrongButLegalDigit(game, cell));
  game.hintsUsed = 0xFF;
  sp::takeHint(game);
  checkEq(game.notice, static_cast<int>(sp::Notice::WrongDigit), "the wrong-digit branch ran");
  checkEq(game.hintsUsed, 0xFF, "a wrong-digit hint saturates");

  sp::Record record;
  sp::recordSolve(record, sudoku::Level::Easy, 1000, game.hintsUsed);
  checkEq(static_cast<long>(record.bestMs[0]), 0, "so a heavily hinted solve still sets no best time");
}

// An empty cell and a digit one of its peers already holds.
bool aCellWithATakenDigit(const sp::Game& game, int& cell, int& digit) {
  for (cell = 0; cell < sp::kCells; ++cell) {
    if (sp::valueAt(game, cell) != 0) continue;
    const sp::Mask taken = sp::takenAround(game, cell);
    if (taken == 0) continue;
    digit = sudoku::lowestDigit(taken);
    return true;
  }
  return false;
}

void testNotesRefuseADigitAPeerHolds() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1010u);
  int cell = 0;
  int digit = 0;
  check(aCellWithATakenDigit(game, cell, digit), "found an empty cell with a taken digit");
  sp::tapCell(game, cell);
  game.notesMode = 1;
  const sp::Game before = game;
  check(!sp::tapDigit(game, digit), "a mark for a digit a peer holds is refused");
  check(sameBoard(before, game), "and writes nothing");
  checkEq(game.undoCount, before.undoCount, "and spends no undo");
}

void testEraseIgnoresHiddenMarks() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1011u);
  int cell = 0;
  int digit = 0;
  check(aCellWithATakenDigit(game, cell, digit), "found an empty cell with a taken digit");
  // A stored mark nobody can see: its digit already stands among the peers.
  game.note[cell] = sp::bitFor(digit);
  checkEq(sp::visibleNotes(game, cell), 0, "the mark is hidden");
  sp::tapCell(game, cell);
  check(!sp::canErase(game), "ERASE is disabled over marks that are not shown");
  check(!sp::erase(game), "and does nothing");
  const sp::Mask open = static_cast<sp::Mask>(sp::kAllDigits & ~sp::takenAround(game, cell));
  game.note[cell] = static_cast<sp::Mask>(game.note[cell] | sp::bitFor(sudoku::lowestDigit(open)));
  check(sp::canErase(game), "a visible mark makes it live again");
}

// Every MENU row, through the same function the activity calls.
void testEveryPanelRow() {
  sp::Game game = aGame(sudoku::Level::Easy, 0x1012u);

  sp::Game hint = game;
  check(!sp::applyPanelRow(hint, sp::PanelRow::Hint), "HINT closes the panel");
  checkEq(hint.notice, static_cast<int>(sp::Notice::Technique), "and names a rule");
  checkEq(hint.selected, hint.hintCell, "and selects the cell");

  sp::Game fill = game;
  check(!sp::applyPanelRow(fill, sp::PanelRow::FillNotes), "FILL NOTES closes the panel");
  check(fill.undoCount > 0, "and fills");
  checkEq(fill.notice, 0, "saying nothing when it did something");
  check(!sp::applyPanelRow(fill, sp::PanelRow::FillNotes), "a second FILL NOTES closes the panel too");
  checkEq(fill.notice, static_cast<int>(sp::Notice::NothingToFill), "but says there was nothing to fill");

  sp::Game checked = game;
  check(!sp::applyPanelRow(checked, sp::PanelRow::Check), "CHECK closes the panel");
  checkEq(checked.notice, static_cast<int>(sp::Notice::AllCorrect), "and answers");

  sp::Game toggles = game;
  check(sp::applyPanelRow(toggles, sp::PanelRow::ShowRemaining), "SHOW REMAINING keeps the panel open");
  checkEq(toggles.showRemaining, 0, "and turns it off");
  sp::applyPanelRow(toggles, sp::PanelRow::ShowRemaining);
  checkEq(toggles.showRemaining, 1, "and back on");
  check(sp::applyPanelRow(toggles, sp::PanelRow::ShadePeers), "SHADE PEERS keeps the panel open");
  checkEq(toggles.shadePeers, 0, "and turns it off");
  sp::applyPanelRow(toggles, sp::PanelRow::ShadePeers);
  checkEq(toggles.shadePeers, 1, "and back on");
  check(sameBoard(game, toggles), "neither toggle writes the board");

  sp::Game closed = game;
  check(!sp::applyPanelRow(closed, sp::PanelRow::Close), "CLOSE closes the panel");
  check(sameBoard(game, closed) && closed.notice == 0, "and does nothing else");

  // On a finished board the questions do nothing, and nothing is announced.
  sp::Game solved = game;
  for (int cell = 0; cell < sp::kCells; ++cell)
    solved.entry[cell] = sp::isGiven(solved, cell) ? 0 : solved.puzzle.solution[cell];
  solved.solvedFlag = 1;
  for (const sp::PanelRow row : {sp::PanelRow::Hint, sp::PanelRow::FillNotes, sp::PanelRow::Check}) {
    sp::Game locked = solved;
    sp::applyPanelRow(locked, row);
    checkEq(locked.notice, 0, "a question on a solved board says nothing");
    checkEq(locked.hintsUsed, 0, "and spends nothing");
  }
}

// Back, the whole table: the panel first, then the screens.
void testBackClosesThePanelFirst() {
  check(sp::backAction(sp::Screen::Board, true) == sp::BackAction::ClosePanel, "Back with the panel up closes it");
  check(sp::backAction(sp::Screen::Board, false) == sp::BackAction::GoTo, "Back on the board goes somewhere");
  check(sp::back(sp::Screen::Board) == sp::Screen::Menu, "and that is the menu");
  check(sp::backAction(sp::Screen::Menu, false) == sp::BackAction::LeaveApp, "Back from the menu leaves");
  check(sp::backAction(sp::Screen::Menu, true) == sp::BackAction::LeaveApp,
        "a stale panel flag off the board is ignored");
  check(sp::backAction(sp::Screen::HowTo, false) == sp::BackAction::GoTo, "Back from HOW TO goes somewhere");
  check(sp::back(sp::Screen::HowTo) == sp::Screen::Menu, "and that is the menu");
  check(sp::backAction(sp::Screen::Result, false) == sp::BackAction::GoTo, "Back from the result goes somewhere");
  check(sp::back(sp::Screen::Result) == sp::Screen::Board, "and that is the finished board");
  check(sp::back(sp::Screen::Menu) == sp::Screen::Menu, "the table is total");
}

// --- the save ------------------------------------------------------------------

void testTheSaveRoundTrips() {
  sp::SaveState state;
  state.hasGame = true;
  state.game = aGame(sudoku::Level::Medium, 0x3001u);
  state.menuLevel = state.game.puzzle.level;
  sp::Game& game = state.game;
  const int a = firstEmpty(game);
  const int b = firstEmpty(game, a + 1);
  sp::tapCell(game, a);
  sp::tapDigit(game, game.puzzle.solution[a]);
  sp::tapCell(game, b);
  game.notesMode = 1;
  sp::tapDigit(game, 2);
  sp::tapDigit(game, 8);
  game.showRemaining = 0;
  game.shadePeers = 1;
  game.elapsedMs = 1234567;
  game.hintsUsed = 2;
  state.record.solved[2] = 5;
  state.record.bestMs[2] = 777000;
  state.record.hintsTaken = 9;

  char buffer[sp::kStateBytes];
  const int used = sp::packState(state, buffer, sizeof(buffer));
  check(used > 0 && used < sp::kStateBytes, "the save fits its buffer");

  sp::SaveState back;
  check(sp::unpackState(buffer, back), "the save reads back");
  check(sameBoard(state.game, back.game), "entries and notes survive");
  checkEq(back.game.notesMode, 1, "NOTES survives");
  checkEq(back.game.showRemaining, 0, "SHOW REMAINING survives");
  checkEq(back.game.shadePeers, 1, "SHADE PEERS survives");
  checkEq(static_cast<long>(back.game.elapsedMs), 1234567, "the clock survives");
  checkEq(back.game.hintsUsed, 2, "hints used survive");
  checkEq(back.game.selected, sp::kNoCell, "no cell is selected after a reopen");
  checkEq(back.game.focus, 0, "and nothing focused");
  check(std::memcmp(back.game.puzzle.solution, state.game.puzzle.solution, sizeof(state.game.puzzle.solution)) == 0,
        "the answer is re-derived from the clues");
  check(back.game.puzzle.level == state.game.puzzle.level, "and so is the level");
  check(back.hasGame, "the game is still there");
  checkEq(back.record.solved[2], 5, "the record survives");
  checkEq(static_cast<long>(back.record.bestMs[2]), 777000, "with its best time");
  checkEq(back.record.hintsTaken, 9, "and its hint count");

  // Both toggles, both ways.
  for (int bits = 0; bits < 4; ++bits) {
    state.game.showRemaining = static_cast<uint8_t>(bits & 1);
    state.game.shadePeers = static_cast<uint8_t>((bits >> 1) & 1);
    sp::packState(state, buffer, sizeof(buffer));
    sp::SaveState again;
    sp::unpackState(buffer, again);
    checkEq(again.game.showRemaining, bits & 1, "SHOW REMAINING round trips either way");
    checkEq(again.game.shadePeers, (bits >> 1) & 1, "SHADE PEERS round trips either way");
  }
}

void testABrokenSaveChangesNothing() {
  sp::SaveState state;
  state.hasGame = true;
  state.game = aGame(sudoku::Level::Easy, 0x3002u);
  char buffer[sp::kStateBytes];
  const int used = sp::packState(state, buffer, sizeof(buffer));

  sp::SaveState untouched;
  untouched.game.elapsedMs = 42;
  for (int cut = 0; cut < used; cut += 37) {
    char truncated[sp::kStateBytes];
    std::memcpy(truncated, buffer, static_cast<size_t>(cut));
    truncated[cut] = '\0';
    sp::SaveState out = untouched;
    check(!sp::unpackState(truncated, out), "a truncated save is refused");
    checkEq(static_cast<long>(out.game.elapsedMs), 42, "and leaves the state as it was");
  }
  // A version from somewhere else.
  buffer[0] = '9';
  sp::SaveState out;
  check(!sp::unpackState(buffer, out), "another version is refused");
}

void testNoGameStillKeepsTheToggles() {
  sp::SaveState state;
  state.hasGame = false;
  state.game.showRemaining = 0;
  state.record.solved[0] = 3;
  char buffer[sp::kStateBytes];
  check(sp::packState(state, buffer, sizeof(buffer)) > 0, "an empty save packs");
  sp::SaveState back;
  check(sp::unpackState(buffer, back), "and reads back");
  check(!back.hasGame, "with no game");
  checkEq(back.game.showRemaining, 0, "but its toggles");
  checkEq(back.record.solved[0], 3, "and its record");
}

void testTheSaveIsNotSudokus() {
  check(std::strcmp(sp::kStatePath, "/.crosspoint/sudoku.sav") != 0, "SUDOKU+ never writes SUDOKU's save");
  check(std::strcmp(sp::kStatePath, "/.crosspoint/sudokuplus.sav") == 0, "it writes its own");
}

void testASolvedBoardSavesSolved() {
  sp::SaveState state;
  state.hasGame = true;
  state.game = aGame(sudoku::Level::Easy, 0x3003u);
  state.menuLevel = sudoku::Level::Hard;
  for (int cell = 0; cell < sp::kCells; ++cell) {
    if (!sp::isGiven(state.game, cell)) state.game.entry[cell] = state.game.puzzle.solution[cell];
  }
  state.game.solvedFlag = 1;
  char buffer[sp::kStateBytes];
  check(sp::packState(state, buffer, sizeof(buffer)) > 0, "a solved board packs");
  sp::SaveState back;
  check(sp::unpackState(buffer, back), "and reads back");
  checkEq(back.game.solvedFlag, 1, "still solved");
  check(back.menuLevel == sudoku::Level::Hard, "and the menu keeps pointing where it was, not at the finished level");
}

void testAForgedSolvedFlagIsDropped() {
  sp::SaveState state;
  state.hasGame = true;
  state.game = aGame(sudoku::Level::Easy, 0x3004u);
  state.game.solvedFlag = 1;  // the board is nowhere near solved
  char buffer[sp::kStateBytes];
  sp::packState(state, buffer, sizeof(buffer));
  sp::SaveState back;
  check(sp::unpackState(buffer, back), "a forged flag still reads");
  checkEq(back.game.solvedFlag, 0, "but the flag the board does not bear out is dropped");
}

void testAnUnsolvedGameReopensAtItsOwnLevel() {
  sp::SaveState state;
  state.hasGame = true;
  state.game = aGame(sudoku::Level::Medium, 0x3005u);
  const sudoku::Level level = state.game.puzzle.level;
  state.menuLevel = level == sudoku::Level::Expert ? sudoku::Level::Easy : sudoku::Level::Expert;
  char buffer[sp::kStateBytes];
  sp::packState(state, buffer, sizeof(buffer));
  sp::SaveState back;
  check(sp::unpackState(buffer, back), "reads back");
  check(back.menuLevel == level, "the menu reopens on the unsolved game's own level");
  check(sp::canResume(back.game, back.hasGame, back.menuLevel), "so the door is RESUME");
}

}  // namespace

int main() {
  std::printf("SUDOKU+ game\n");
  testSelect();
  testEnterAndSameDigit();
  testNotes();
  testNoTarget();
  testErase();
  testFocusShading();
  testRemaining();
  testFillNotesIsOneStep();
  testAFillNeverUndoesByHalves();
  testCheckClearsOnTheNextEdit();
  testHintSelectsAndClearsOnEdit();
  testTheSolvedBoardLocks();
  testClashesAreDerived();
  testVisibleNotesHideWhatAPeerTook();
  testStartGameKeepsThePreferences();
  testTheRecordOnlyTimesUnhintedSolves();
  testEveryNoticeFitsTheHeader();
  testHintsUsedSaturates();
  testNotesRefuseADigitAPeerHolds();
  testEraseIgnoresHiddenMarks();
  testEveryPanelRow();
  testBackClosesThePanelFirst();
  std::printf("SUDOKU+ save\n");
  testTheSaveRoundTrips();
  testABrokenSaveChangesNothing();
  testNoGameStillKeepsTheToggles();
  testTheSaveIsNotSudokus();
  testASolvedBoardSavesSolved();
  testAForgedSolvedFlagIsDropped();
  testAnUnsolvedGameReopensAtItsOwnLevel();

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
