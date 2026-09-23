#pragma once

// SUDOKU+ on the device. The thin layer: renderer, storage, input, shelf.
//
// A separate game from SUDOKU rather than a setting inside it, so the two can
// be played side by side and neither one's habits leak into the other. They
// share the puzzle engine and nothing else: not a class, not a save, not a
// record.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "SudokuPlusFlow.h"
#include "SudokuPlusGame.h"
#include "SudokuPlusScreens.h"

class SudokuPlusActivity final : public Activity {
 public:
  SudokuPlusActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SudokuPlus", renderer, mappedInput) {}
  ~SudokuPlusActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // The Home gesture's swap is deferred, so this loop() runs once more after
  // the player has gone; with a carve in flight that pass would land it over
  // the saved game. See SudokuActivity::handleHomeGesture.
  bool handleHomeGesture() override;

 private:
  void goTo(sudokuplus::Screen next);
  void beginGame();
  void cancelCarve();
  void choosePanelRow(sudokuplus::PanelRow row);
  void recordResult();
  void loadState();
  void saveState();
  void tickClock();

  // What a tap on the play surface means. See Activity::surfaceMeaning().
  uint32_t surfaceMeaning() const override;

  sudokuplus::Screen screen = sudokuplus::Screen::Menu;
  sudokuplus::Game game{};
  sudokuplus::Record record{};
  // 1.1KB of generator scratch, owned here so the rules need no heap.
  sudoku::Workspace work{};

  // What the next puzzle will be, which is not the same as what the open one
  // is. Whether they agree is asked through sudokuplus::canResume().
  sudoku::Level menuLevel = sudoku::Level::Easy;
  bool hasGame = false;

  // Carving happens one loop pass AFTER the frame that says so.
  bool generating = false;
  bool generateDeferred = false;
  sudoku::Level generatingLevel = sudoku::Level::Easy;
  uint32_t rng = 0;

  // The MENU panel, a full-page sheet in place of the board. A sub-state of
  // Board, not a screen.
  bool panelOpen = false;
  // Which paints flash as the panel comes and goes; see sudokuplus::PanelPaint.
  sudokuplus::PanelPaint panelPaint{};

  unsigned long lastTickMs = 0;
  // The minute the header last showed, so the clock repaints once a minute and
  // never more often.
  uint32_t shownMinute = 0;

  int howToPage = 0;
  int menuSelected = -1;
  bool resultRecorded = false;
  bool newBest = false;
  uint32_t previousBestMs = 0;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
