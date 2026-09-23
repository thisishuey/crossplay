#pragma once

// SUDOKU+'s navigation, and nothing else. Freestanding. The same four screens
// as SUDOKU, for SUDOKU's reasons (see ../sudoku/SudokuFlow.h).
//
// The MENU panel is not a screen. It is a sub-state of the board, drawn over
// the grid, because what it offers -- a hint, filling notes, a check, two
// toggles -- is all about the board underneath it and all of it wants that
// board still in view. Its Back is the activity's business: Back closes the
// panel before it does anything else.

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

}  // namespace sudokuplus
