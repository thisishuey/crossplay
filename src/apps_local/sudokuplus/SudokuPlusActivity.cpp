#include "SudokuPlusActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "SudokuPlusSave.h"

namespace sp = sudokuplus;

namespace {
sudoku::Level nextLevel(const sudoku::Level level) {
  return static_cast<sudoku::Level>((static_cast<int>(level) + 1) % sudoku::kLevelCount);
}
}  // namespace

std::unique_ptr<Activity> SudokuPlusActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<SudokuPlusActivity>(renderer, mappedInput);
}

void SudokuPlusActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  screen = sp::Screen::Menu;
  panelOpen = false;
  panelPaint = {};
  menuSelected = -1;
  lastTickMs = millis();
  // The only entropy that differs between two boots, entering here and nowhere
  // deeper: the rules take a seed and never reach for a clock.
  rng = static_cast<uint32_t>(millis()) * 2654435761u + 1u;
  loadState();
  shownMinute = game.elapsedMs / 60000;
  requestUpdate();
}

void SudokuPlusActivity::onExit() {
  // Runs on sleep as well as on leaving, which is the case that matters.
  saveState();
  Activity::onExit();
}

void SudokuPlusActivity::goTo(const sp::Screen next) {
  screen = next;
  // The panel belongs to the board; leaving the board takes it down.
  panelOpen = false;
  requestUpdate();
}

void SudokuPlusActivity::beginGame() {
  generating = true;
  generateDeferred = true;
  generatingLevel = menuLevel;
  resultRecorded = false;
  newBest = false;
  goTo(sp::Screen::Board);
}

void SudokuPlusActivity::cancelCarve() {
  generating = false;
  generateDeferred = false;
  // Restore what the surviving game actually is, so a cancelled carve over a
  // solved game cannot record that solve a second time.
  resultRecorded = game.solvedFlag != 0;
}

bool SudokuPlusActivity::handleHomeGesture() {
  cancelCarve();
  return false;
}

void SudokuPlusActivity::tickClock() {
  const unsigned long now = millis();
  const unsigned long since = now - lastTickMs;
  lastTickMs = now;
  if (screen != sp::Screen::Board || generating || game.solvedFlag != 0 || !hasGame) return;
  game.elapsedMs += static_cast<uint32_t>(since);
  // The header shows whole minutes, so it is repainted when the minute turns
  // and at no other time. A seconds clock on a panel that takes 300ms to
  // repaint would spend the battery telling you the time.
  const uint32_t minute = game.elapsedMs / 60000;
  if (minute != shownMinute) {
    shownMinute = minute;
    requestUpdate();
  }
}

void SudokuPlusActivity::choosePanelRow(const sp::PanelRow row) {
  // The rule for each row is sudokuplus::applyPanelRow, freestanding and
  // host-tested; this only carries its answer about the panel.
  panelOpen = sp::applyPanelRow(game, row);
  requestUpdate();
}

void SudokuPlusActivity::recordResult() {
  if (resultRecorded) return;
  resultRecorded = true;
  const int index = static_cast<int>(game.puzzle.level);
  previousBestMs = record.bestMs[index];
  sp::recordSolve(record, game.puzzle.level, game.elapsedMs, game.hintsUsed);
  newBest = game.hintsUsed == 0 && record.bestMs[index] == game.elapsedMs && previousBestMs != game.elapsedMs;
  saveState();
}

void SudokuPlusActivity::loadState() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.exists(sp::kStatePath)) return;
  auto buffer = makeUniqueNoThrow<char[]>(sp::kStateBytes + 1);
  // About a kilobyte, so on the heap rather than the loop task's stack.
  auto state = makeUniqueNoThrow<sp::SaveState>();
  if (!buffer || !state) {
    LOG_ERR("SUDOKUPLUS", "OOM reading the save");
    return;
  }
  std::memset(buffer.get(), 0, sp::kStateBytes + 1);
  if (Storage.readFileToBuffer(sp::kStatePath, buffer.get(), sp::kStateBytes) == 0) return;
  if (!sp::unpackState(buffer.get(), *state)) {
    LOG_ERR("SUDOKUPLUS", "Save unreadable; starting fresh");
    return;
  }
  game = state->game;
  record = state->record;
  menuLevel = state->menuLevel;
  hasGame = state->hasGame;
  resultRecorded = game.solvedFlag != 0;
#endif
}

void SudokuPlusActivity::saveState() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  auto buffer = makeUniqueNoThrow<char[]>(sp::kStateBytes);
  auto state = makeUniqueNoThrow<sp::SaveState>();
  if (!buffer || !state) {
    LOG_ERR("SUDOKUPLUS", "OOM writing the save");
    return;
  }
  state->menuLevel = menuLevel;
  state->hasGame = hasGame;
  state->game = game;
  state->record = record;
  if (sp::packState(*state, buffer.get(), sp::kStateBytes) < 0) {
    LOG_ERR("SUDOKUPLUS", "Save did not fit %d bytes", sp::kStateBytes);
    return;
  }
  Storage.writeFile(sp::kStatePath, String(buffer.get()));
#endif
}

// What moves the pixel-to-effect map of the grid and the pad: the screen, a
// puzzle arriving with no tap at all, and the panel, which replaces the whole
// grid and pad while it is up. The selection, focus and cell contents are
// deliberately absent: they change on every tap, and gating consecutive taps
// on a repaint is the frozen-device failure. See SudokuActivity.cpp.
uint32_t SudokuPlusActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(screen));
  meaning = paintclock::mixMeaning(meaning, generating ? 1u : 0u);
  return paintclock::mixMeaning(meaning, panelOpen ? 1u : 0u);
}

void SudokuPlusActivity::loop() {
  namespace fui = freeink::ui;

  tickClock();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Which of the three it is lives in sudokuplus::backAction, where the
    // panel-first rule is host-tested.
    const sp::BackAction action = sp::backAction(screen, panelOpen);
    if (action == sp::BackAction::ClosePanel) {
      panelOpen = false;
      requestUpdate();
      return;
    }
    if (action == sp::BackAction::LeaveApp) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (screen == sp::Screen::Board) {
      // Back off a board still being carved CANCELS the carve; see
      // SudokuActivity::loop for the bug that is.
      cancelCarve();
      saveState();
    }
    goTo(sp::back(screen));
    return;
  }

  if (generating) {
    if (generateDeferred) {
      generateDeferred = false;
      return;
    }
    sudoku::Puzzle puzzle;
    if (sudoku::generate(puzzle, generatingLevel, work, rng, 24)) {
      sp::startGame(game, puzzle);
      hasGame = true;
      generating = false;
      shownMinute = 0;
      saveState();
      requestUpdate();
    }
    return;
  }

  // The record is written the moment the rules say it is over. The SCREEN does
  // not change: the finished grid stays, and SOLVED on the rail is the door.
  if (screen == sp::Screen::Board && game.solvedFlag != 0) recordResult();

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  // Grid and pad are hit-tested here against the geometry that drew them --
  // ninety regions against twenty-four slots -- and not at all while the panel
  // is up, which is what makes it modal.
  if (screen == sp::Screen::Board && !panelOpen) {
    const fui::DeviceContext device = toybox::makeTarget(renderer).deviceContext();
    int cell = 0;
    int digit = 0;
    if (sudokuplusui::cellAt(device, tapX, tapY, cell)) {
      if (!surfaceRevealed()) return;
      if (sp::tapCell(game, cell)) requestUpdate();
      return;
    }
    if (sudokuplusui::padKeyAt(device, tapX, tapY, digit)) {
      if (!surfaceRevealed()) return;
      if (sp::tapDigit(game, digit)) requestUpdate();
      return;
    }
  }

  const fui::ActionEvent event = interactions.route(input);
  switch (event.action) {
    case sudokuplusui::ActionPlay:
      // Resuming and starting are one door; which it is has exactly one
      // definition, shared with the label the player read before tapping.
      if (sp::canResume(game, hasGame, menuLevel)) {
        game.selected = sp::kNoCell;
        goTo(sp::Screen::Board);
        return;
      }
      beginGame();
      return;

    case sudokuplusui::ActionMenuRow:
      switch (static_cast<sudokuplusui::MenuRow>(event.value)) {
        case sudokuplusui::MenuRow::Level:
          menuLevel = nextLevel(menuLevel);
          menuSelected = static_cast<int>(sudokuplusui::MenuRow::Level);
          requestUpdate();
          return;
        case sudokuplusui::MenuRow::HowTo:
          howToPage = 0;
          goTo(sp::Screen::HowTo);
          return;
        case sudokuplusui::MenuRow::Count:
          return;
      }
      return;

    case sudokuplusui::ActionHowToNext:
      if (howToPage + 1 < sudokuplusui::howToPages()) {
        ++howToPage;
        requestUpdate();
        return;
      }
      goTo(sp::Screen::Menu);
      return;

    // The rail's four actions return early while the panel is up. The rail
    // registers nothing then, but a tap can still route against the table from
    // the frame BEFORE the panel was drawn, and the panel is modal.
    case sudokuplusui::ActionNotes:
      if (panelOpen) return;
      if (game.solvedFlag == 0) {
        game.notesMode = game.notesMode != 0 ? 0 : 1;
        requestUpdate();
      }
      return;

    case sudokuplusui::ActionErase:
      if (panelOpen) return;
      if (sp::erase(game)) requestUpdate();
      return;

    case sudokuplusui::ActionUndo:
      if (panelOpen) return;
      if (sp::undoOnce(game)) requestUpdate();
      return;

    case sudokuplusui::ActionOpenPanel:
      if (panelOpen) return;
      panelOpen = true;
      requestUpdate();
      return;

    case sudokuplusui::ActionPanelRow:
      if (panelOpen && event.value >= 0 && event.value < static_cast<int>(sp::PanelRow::Count)) {
        choosePanelRow(static_cast<sp::PanelRow>(event.value));
      }
      return;

    case sudokuplusui::ActionSeeResult:
      goTo(sp::Screen::Result);
      return;

    case sudokuplusui::ActionAgain:
      beginGame();
      return;

    case sudokuplusui::ActionDone:
      goTo(sp::Screen::Board);
      return;

    default:
      return;
  }
}

void SudokuPlusActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);
  const bool drewPanel = screen == sp::Screen::Board && panelOpen;

  switch (screen) {
    case sp::Screen::Menu: {
      sudokuplusui::MenuModel model;
      model.level = menuLevel;
      model.hasGame = hasGame;
      model.game = game;
      model.record = record;
      model.selected = menuSelected;
      sudokuplusui::buildMenu(surface, model);
      break;
    }
    case sp::Screen::HowTo: {
      sudokuplusui::HowToModel model;
      model.page = howToPage;
      sudokuplusui::buildHowTo(surface, model);
      break;
    }
    case sp::Screen::Board: {
      sudokuplusui::BoardModel model;
      model.game = game;
      model.generating = generating;
      model.generatingLevel = generatingLevel;
      model.panelOpen = panelOpen;
      sudokuplusui::buildBoard(surface, model);
      break;
    }
    case sp::Screen::Result: {
      sudokuplusui::ResultModel model;
      model.game = game;
      model.level = game.puzzle.level;
      model.hardest = game.puzzle.hardest;
      model.elapsedMs = game.elapsedMs;
      model.bestMs = record.bestMs[static_cast<int>(game.puzzle.level)];
      model.newBest = newBest;
      model.hintsUsed = game.hintsUsed;
      model.clues = game.puzzle.clues;
      model.solvedAtThisLevel = record.solved[static_cast<int>(game.puzzle.level)];
      sudokuplusui::buildResult(surface, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "SudokuPlus");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  // Decided from what this frame drew against what the last one did, so every
  // way the panel goes up or comes down flashes, and nothing else does.
  const bool flash = panelPaint.next(drewPanel);
  renderer.displayBuffer(flash ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
}
