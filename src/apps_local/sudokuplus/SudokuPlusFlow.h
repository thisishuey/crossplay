#pragma once

// SUDOKU+'s navigation, and the one refresh rule that follows from it: which
// paints flash as the MENU panel comes and goes. Freestanding. The same four
// screens as SUDOKU, for SUDOKU's reasons (see ../sudoku/SudokuFlow.h).
//
// The MENU panel is not a screen. It is a sub-state of the board: what it
// offers -- a hint, filling notes, a check, three toggles -- is all about the
// board, and the three questions close it so their answers land on the board.
// It is drawn as a full-page sheet below the header with the board hidden,
// so nothing under it can look live. Its Back is the activity's business:
// Back closes the panel before it does anything else.

#include <cstdint>

namespace sudokuplus {

enum class Screen : uint8_t {
  // The top. Back from here leaves the app, and it is the only screen that does.
  Menu,
  HowTo,
  Board,
  Result,
};

// Exhaustive switch and no default, so a new screen without a decided Back
// fails the build.
constexpr Screen back(const Screen screen) {
  switch (screen) {
    case Screen::Menu:
      return Screen::Menu;
    case Screen::HowTo:
      return Screen::Menu;
    case Screen::Board:
      // Stop solving, not leave: the puzzle is saved either way.
      return Screen::Menu;
    case Screen::Result:
      // Back to the finished grid, which is the thing worth looking at.
      return Screen::Board;
  }
  return Screen::Menu;
}

constexpr bool leavesApp(const Screen screen) { return screen == Screen::Menu; }

// The MENU panel's rows, top to bottom. Its whole vocabulary: the three things
// you might ask of a board, the two ways it can be drawn, and the door. What
// each one does is sudokuplus::applyPanelRow (SudokuPlusGame.h).
enum class PanelRow : int { Hint = 0, FillNotes, Check, ShowRemaining, ShadePeers, NoteStyle, Close, Count };

// What Back does, all of it, including the panel the screen table cannot see.
enum class BackAction : uint8_t {
  ClosePanel,  // the panel was up: take it down and stay on the board
  LeaveApp,    // Back from the front door
  GoTo,        // go to back(screen)
};

constexpr BackAction backAction(const Screen screen, const bool panelOpen) {
  // The panel is modal, so Back takes it down before anything else. A Back that
  // walked off the board from under it would read as "undo what I was doing".
  if (screen == Screen::Board && panelOpen) return BackAction::ClosePanel;
  if (leavesApp(screen)) return BackAction::LeaveApp;
  return BackAction::GoTo;
}

// Whether a paint is a full refresh: exactly when it puts the panel up or takes
// it down. The sheet replaces a dense board wholesale, and the board the sheet,
// so a fast refresh would leave the one ghosting under the other. A toggle
// repaints the panel over the panel and a tap repaints the board over the
// board; both stay fast, so browsing the panel and playing stay quiet.
//
// It remembers what the last paint showed, so the activity holds one, asks it
// once per paint, and starts a fresh one on entry: nothing is on screen then.
struct PanelPaint {
  bool shown = false;  // the last paint was the panel

  // `drewPanel` is what this paint shows. Returns whether it flashes.
  bool next(const bool drewPanel) {
    const bool flash = drewPanel != shown;
    shown = drewPanel;
    return flash;
  }
};

}  // namespace sudokuplus
