#pragma once

// SUDOKU+ on screen. Freestanding builders over plain models: FreeInkUI and
// Toybox tokens and nothing else, so host-tests/ui can build every screen
// against a fake draw target and ask what it drew and what it made tappable.

#include "../ui/ToyboxScreen.h"
#include "SudokuPlusFlow.h"
#include "SudokuPlusGame.h"

namespace sudokuplusui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  // The headline: the largest thing on the front door is also the commonest tap.
  ActionPlay = 2,
  ActionHowToNext = 3,
  // The rail, top to bottom. NOTES is a toggle; once the grid is finished its
  // slot becomes the SOLVED door instead (ActionSeeResult).
  ActionNotes = 4,
  ActionErase = 5,
  ActionUndo = 6,
  ActionOpenPanel = 7,
  ActionSeeResult = 8,
  ActionAgain = 9,
  ActionDone = 10,
  // A row of the MENU panel, value = the PanelRow.
  ActionPanelRow = 11,
};

enum class MenuRow : int { Level = 0, HowTo, Count };

using PanelRow = sudokuplus::PanelRow;

struct MenuModel {
  sudoku::Level level = sudoku::Level::Easy;
  bool hasGame = false;
  sudokuplus::Game game{};
  sudokuplus::Record record{};
  int selected = -1;
};

struct HowToModel {
  int page = 0;
};

struct BoardModel {
  sudokuplus::Game game{};
  // True while a puzzle is being carved: the grid is empty and the rail says so.
  bool generating = false;
  // The level being carved, which is what the header names while `generating`:
  // `game` is still the previous puzzle then.
  sudoku::Level generatingLevel = sudoku::Level::Easy;
  // The MENU panel is up. It covers the page below the header and the board is
  // not drawn at all -- no grid, pad or rail -- so its rows are the only things
  // on screen, and the activity hit-tests neither grid nor pad.
  bool panelOpen = false;
};

struct ResultModel {
  sudokuplus::Game game{};
  sudoku::Level level = sudoku::Level::Easy;
  sudoku::Technique hardest = sudoku::Technique::None;
  uint32_t elapsedMs = 0;
  uint32_t bestMs = 0;
  bool newBest = false;
  int hintsUsed = 0;
  int clues = 0;
  int solvedAtThisLevel = 0;
};

// The grid's geometry and its exact inverse, and the pad's. Neither is in the
// interaction buffer -- ninety regions against twenty-four slots -- so both
// are hit-tested arithmetically from the numbers that drew them. Each `...At`
// returns false outside its own area; host-tests/ui walks every pixel of both.
fui::Rect cellRect(const fui::DeviceContext& device, int cell);
bool cellAt(const fui::DeviceContext& device, int x, int y, int& cell);
fui::Rect padKeyRect(const fui::DeviceContext& device, int digit);
bool padKeyAt(const fui::DeviceContext& device, int x, int y, int& digit);

// Exposed for host-tests/ui, which checks every control stays on the panel.
fui::Rect railRowRect(const fui::DeviceContext& device, int row);
constexpr int kRailRows = 4;
fui::Rect panelRect(const fui::DeviceContext& device);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);
void buildResult(toybox::Screen& screen, const ResultModel& model);

int howToPages();

// "12:04", or "1:02:03" past the hour.
void formatClock(uint32_t ms, char* out, int size);
// "12 MIN", or "1H 05M" past the hour: the board header's clock, which only
// changes once a minute because that is how often it is repainted.
void formatMinutes(uint32_t ms, char* out, int size);

}  // namespace sudokuplusui
