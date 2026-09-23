#include "SudokuPlusScreens.h"

#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace sudokuplusui {

namespace {

namespace sk = sudoku;
namespace sp = sudokuplus;

// ---------------------------------------------------------------------------
// The composition. SUDOKU's, number for number, so the two games put the grid
// and the pad in the same place (see ../sudoku/SudokuScreens.cpp). The one
// change is the rail beside the pad: four rows rather than three, because it
// holds NOTES, ERASE, UNDO and MENU, and no readout.
//
//   4 rows of 45 + 3 gaps of 9 = 207, exactly the pad's height
// ---------------------------------------------------------------------------

constexpr int16_t kCell = 50;
constexpr int16_t kGridSide = kCell * sk::kSize;
constexpr int16_t kBoardOuter = kGridSide + 2 * toybox::kBoardFrame;
constexpr int16_t kBoardTop = toybox::kChromeHeight + toybox::kGutter;

// The keys abut, sharing their outlines, so padKeyAt is an exact inverse of
// padKeyRect with no dead strip between keys.
constexpr int16_t kPadKey = 69;
constexpr int16_t kPadSide = 3 * kPadKey;
constexpr int16_t kRailRow = 45;
constexpr int16_t kRailRowGap = 9;

constexpr int16_t kPadTop = 800 - toybox::kMargin - kPadSide;
constexpr int16_t kRailGap = kPadTop - (kBoardTop + kBoardOuter);

static_assert(kPadSide == 207, "the pad is three keys wide");
static_assert(kPadTop + kPadSide == 800 - toybox::kMargin, "the pad ends on the page's bottom margin");
static_assert(kRailGap >= toybox::kGutter, "the pad needs air under the grid");
static_assert(kRailRows * kRailRow + (kRailRows - 1) * kRailRowGap == kPadSide,
              "the rail is exactly as tall as the pad");

// The MENU panel: a full-page sheet, the board's width, from the board's top
// down to the page's bottom margin. The board is not drawn beneath it, so the
// grid, the pad and the rail cannot sit there looking live while answering
// nothing. Its seven rows share the sheet between them, so no band of it is
// dead to a tap; whatever the division leaves over goes to the bottom padding.
constexpr int kPanelRows = static_cast<int>(PanelRow::Count);
constexpr int16_t kPanelRowGap = toybox::kGutter / 2;
constexpr int16_t kPanelPad = toybox::kGutter;
constexpr int16_t kPanelHeight = static_cast<int16_t>(800 - toybox::kMargin - kBoardTop);
constexpr int16_t kPanelWidth = kBoardOuter;
constexpr int16_t kPanelRow =
    static_cast<int16_t>((kPanelHeight - 2 * kPanelPad - (kPanelRows - 1) * kPanelRowGap) / kPanelRows);
static_assert(kPanelRows * kPanelRow + (kPanelRows - 1) * kPanelRowGap + 2 * kPanelPad <= kPanelHeight,
              "every panel row fits on the sheet");
static_assert(kPanelRow >= toybox::kRowHeight, "a panel row is at least a standard list row");

int16_t boardLeft(const fui::DeviceContext& device) { return static_cast<int16_t>((device.width - kBoardOuter) / 2); }

int16_t gridLeft(const fui::DeviceContext& device) {
  return static_cast<int16_t>(boardLeft(device) + toybox::kBoardFrame);
}

constexpr int16_t gridTop() { return kBoardTop + toybox::kBoardFrame; }

fui::Paint ink(const bool paper) { return fui::Paint::solid(paper ? fui::Color::White : fui::Color::Black); }

// A rectangular outline of a given weight, drawn as four bars inside the rect.
void frame(toybox::Screen& screen, const fui::Rect& box, const int16_t weight, const bool paper = false) {
  const fui::Paint paint = ink(paper);
  screen.target().fill(fui::makeRect(box.x, box.y, box.width, weight), paint);
  screen.target().fill(fui::makeRect(box.x, static_cast<int16_t>(box.bottom() - weight), box.width, weight), paint);
  screen.target().fill(fui::makeRect(box.x, box.y, weight, box.height), paint);
  screen.target().fill(fui::makeRect(static_cast<int16_t>(box.right() - weight), box.y, weight, box.height), paint);
}

fui::Rect inset(const fui::Rect& box, const int16_t by) {
  return fui::makeRect(static_cast<int16_t>(box.x + by), static_cast<int16_t>(box.y + by),
                       static_cast<int16_t>(box.width - 2 * by), static_cast<int16_t>(box.height - 2 * by));
}

// Four brackets at the corners of a box: the front door ornament's frame.
void cornerMarks(toybox::Screen& screen, const fui::Rect& box, const int16_t arm, const int16_t weight,
                 const bool paper) {
  const fui::Paint paint = ink(paper);
  const int16_t right = static_cast<int16_t>(box.right() - arm);
  const int16_t bottom = static_cast<int16_t>(box.bottom() - weight);
  const int16_t bottomArm = static_cast<int16_t>(box.bottom() - arm);
  const int16_t rightEdge = static_cast<int16_t>(box.right() - weight);
  screen.target().fill(fui::makeRect(box.x, box.y, arm, weight), paint);
  screen.target().fill(fui::makeRect(box.x, box.y, weight, arm), paint);
  screen.target().fill(fui::makeRect(right, box.y, arm, weight), paint);
  screen.target().fill(fui::makeRect(rightEdge, box.y, weight, arm), paint);
  screen.target().fill(fui::makeRect(box.x, bottom, arm, weight), paint);
  screen.target().fill(fui::makeRect(box.x, bottomArm, weight, arm), paint);
  screen.target().fill(fui::makeRect(right, bottom, arm, weight), paint);
  screen.target().fill(fui::makeRect(rightEdge, bottomArm, weight, arm), paint);
}

// The selection: two frames, one black and one white, so it reads on every
// ground. On a dark ground (a clue's DarkGray, the focus's black) it is 2px
// black outside a 3px white band, the white half carrying it. On a light one
// (paper, or LightGray) it is inverted: 3px white outside a 2px black line, so
// it never merges with the board frame or a box rule at the edge of the grid.
constexpr int16_t kStrikeHalo = 2;
// A pencil mark drawn as a shape: a 10px dot, a 2px grey ring or solid black.
constexpr int16_t kNoteShape = 10;
constexpr uint8_t kNoteRing = 2;
// The paper under a grey note numeral on a shaded cell: the tile cut's widest
// digit is 9px of ink, so a pixel of paper either side of it.
constexpr int16_t kNotePatch = 11;
constexpr int16_t kSelectBlack = 2;
constexpr int16_t kSelectWhite = 3;
void selectionFrame(toybox::Screen& screen, const fui::Rect& box, const bool light) {
  if (light) {
    frame(screen, box, kSelectWhite, true);
    frame(screen, inset(box, kSelectWhite), kSelectBlack, false);
    return;
  }
  frame(screen, box, kSelectBlack, false);
  frame(screen, inset(box, kSelectBlack), kSelectWhite, true);
}

// The two marks a digit can carry are one stroke in two directions: a 3px
// black diagonal inside a 2px white halo, so it reads over a numeral of either
// colour and on every ground. A clash rises (/); a digit CHECK found wrong
// falls (\); a wrong digit that also clashes wears both, an X. Neither is a
// frame, because the selection is one, nor a black ground, because black
// belongs to the focus.
enum class Stroke : uint8_t { Rising, Falling };
void strike(toybox::Screen& screen, const fui::Rect& box, const Stroke stroke) {
  const int16_t pad = 6;
  const int16_t left = static_cast<int16_t>(box.x + pad);
  const int16_t right = static_cast<int16_t>(box.right() - 1 - pad);
  const int16_t top = static_cast<int16_t>(box.y + pad);
  const int16_t bottom = static_cast<int16_t>(box.bottom() - 1 - pad);
  const fui::Point from = stroke == Stroke::Rising ? fui::Point{left, bottom} : fui::Point{left, top};
  const fui::Point to = stroke == Stroke::Rising ? fui::Point{right, top} : fui::Point{right, bottom};
  screen.target().line(from, to, static_cast<uint8_t>(toybox::kRule + 2 * kStrikeHalo), ink(true));
  screen.target().line(from, to, static_cast<uint8_t>(toybox::kRule), ink(false));
}

// The header band with the offset rule under it, as the other games wear it.
void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  // rightLabel is drawn with subtitleText, whose theme default is black, on the
  // black band.
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

constexpr int16_t textBand(const int lines, const toybox::CutMetrics& cut) {
  return static_cast<int16_t>(lines * cut.lineHeight);
}

void digitText(char* out, const int digit) {
  out[0] = static_cast<char>('0' + digit);
  out[1] = '\0';
}

// Knocks every other pixel of `box` back to paper, in the checkerboard the
// DarkGray dither leaves white, so black ink inside it reads as DarkGray.
// That is how a grey note numeral is drawn: GfxRenderer has no dithered text,
// and FreeInkUI falls back to solid black for a DarkGray text colour, so the
// numeral goes down black and is greyed afterwards. One-pixel lines at exactly
// 45 degrees hit exactly the pixels of one anti-diagonal (x + y constant), and
// every odd one is the dither's white. Where text does dither, the pixels it
// leaves white are these same ones, so this changes nothing.
void greyOut(toybox::Screen& screen, const fui::Rect& box) {
  const fui::Paint paper = fui::Paint::solid(fui::Color::White);
  const int left = box.x;
  const int top = box.y;
  const int right = box.right() - 1;
  const int bottom = box.bottom() - 1;
  for (int sum = left + top; sum <= right + bottom; ++sum) {
    if (sum % 2 == 0) continue;
    const int from = sum - bottom > left ? sum - bottom : left;
    const int to = sum - top < right ? sum - top : right;
    if (from > to) continue;
    screen.target().line(fui::Point{static_cast<int16_t>(from), static_cast<int16_t>(sum - from)},
                         fui::Point{static_cast<int16_t>(to), static_cast<int16_t>(sum - to)}, 1, paper);
  }
}

// The nine pencil marks, each where its digit sits on the pad: 1 top-left
// through 9 bottom-right. A mark is grey -- DarkGray, the 50% dither, because
// LightGray's 25% leaves a 10px ring or a note-sized numeral faint and broken
// on a 1-bit panel -- so the notes sit a step behind the digits. The focused
// digit's mark is plain black instead, which is the whole emphasis: "where can
// the 6 go" is answered by the notes as well as the board, with no box or chip
// to read round.
//
// A grey mark always sits on paper. On a SHADE PEERS cell a dither laid over
// the LightGray ground would mix into it, so the paper is knocked out first.
// A dot carries its own paper disc; a numeral gets a patch of paper.
void drawNotes(toybox::Screen& screen, const fui::Rect& cell, const sk::Mask notes, const int focus, const bool shapes,
               const bool shaded) {
  if (notes == 0) return;
  const int16_t pad = 4;
  const int16_t side = static_cast<int16_t>((cell.width - 2 * pad) / 3);
  const fui::Paint paper = fui::Paint::solid(fui::Color::White);
  const fui::Paint grey = fui::Paint::dither(fui::Color::DarkGray);
  for (int digit = 1; digit <= sk::kSize; ++digit) {
    if (!(notes & sk::bitFor(digit))) continue;
    const int16_t column = static_cast<int16_t>((digit - 1) % 3);
    const int16_t row = static_cast<int16_t>((digit - 1) / 3);
    const fui::Rect slot = fui::makeRect(static_cast<int16_t>(cell.x + pad + column * side),
                                         static_cast<int16_t>(cell.y + pad + row * side), side, side);
    const bool emphasised = digit == focus;
    if (shapes) {
      // A round dot where the digit would sit: solid black for the focused
      // digit, otherwise a grey ring on a paper disc. The ring is a grey disc
      // with a paper one inside it rather than a stroke, because a stroke has
      // no dithered ink on the device and would come out black.
      const fui::Rect mark = inset(slot, static_cast<int16_t>((side - kNoteShape) / 2));
      if (emphasised) {
        screen.target().fill(mark, fui::Paint::solid(fui::Color::Black), kNoteShape / 2);
      } else {
        screen.target().fill(mark, paper, kNoteShape / 2);
        screen.target().fill(mark, grey, kNoteShape / 2);
        screen.target().fill(inset(mark, kNoteRing), paper, kNoteShape / 2 - kNoteRing);
      }
      continue;
    }
    // Paper just wider than the widest numeral's ink, the slot's full height.
    const fui::Rect patch =
        fui::makeRect(static_cast<int16_t>(slot.x + (side - kNotePatch) / 2), slot.y, kNotePatch, slot.height);
    if (!emphasised && shaded) screen.target().fill(patch, paper);
    fui::TextStyle text;
    text.font = toybox::kTileFont;
    text.align = fui::TextAlign::Center;
    text.color = emphasised ? fui::Color::Black : fui::Color::DarkGray;
    char glyph[2];
    digitText(glyph, digit);
    screen.target().text(toybox::inkCentred(slot, toybox::kTileCut), glyph, text);
    if (!emphasised) greyOut(screen, patch);
  }
}

void drawGrid(toybox::Screen& screen, const BoardModel& model) {
  const fui::DeviceContext& device = screen.device();
  const int16_t left = boardLeft(device);
  const sp::Game& game = model.game;

  // The frame is knocked out rather than stroked: fill the board solid and
  // paint the playing surface back over it.
  screen.target().fill(fui::makeRect(left, kBoardTop, kBoardOuter, kBoardOuter), fui::Paint::solid(fui::Color::Black));
  screen.target().fill(fui::makeRect(gridLeft(device), gridTop(), kGridSide, kGridSide),
                       fui::Paint::solid(fui::Color::White));

  for (int cell = 0; cell < sk::kCells; ++cell) {
    const fui::Rect box = cellRect(device, cell);
    const uint8_t value = sp::valueAt(game, cell);
    const bool clue = sp::isGiven(game, cell);
    const bool focused = value != 0 && value == game.focus;

    // One ground per cell. The focused digit is the loudest thing on the board
    // because it is the question being asked of it: solid black, clue or not.
    // A clue is DarkGray with a white numeral, your own digits sit on paper,
    // and the selected cell's row, column and box are LightGray -- the lightest
    // ground there is, under a black numeral where one of your digits stands.
    const bool dark = focused || clue;
    if (focused) {
      screen.target().fill(box, fui::Paint::solid(fui::Color::Black));
    } else if (clue) {
      screen.target().fill(box, fui::Paint::dither(fui::Color::DarkGray));
    } else if (sp::isShadedPeer(game, cell)) {
      screen.target().fill(box, fui::Paint::dither(fui::Color::LightGray));
    }

    if (value != 0) {
      char text[2];
      digitText(text, value);
      fui::TextStyle digit;
      digit.font = toybox::kDisplayFont;
      digit.align = fui::TextAlign::Center;
      digit.color = dark ? fui::Color::White : fui::Color::Black;
      screen.target().text(toybox::inkCentred(box, toybox::kDisplayCut), text, digit);

      // A clash rises, and CHECK's mark falls; see `strike`.
      if (sp::isClashing(game, cell)) strike(screen, box, Stroke::Rising);
      if (game.checkShown != 0 && sp::isWrong(game, cell)) strike(screen, box, Stroke::Falling);
    } else {
      drawNotes(screen, box, sp::visibleNotes(game, cell), game.focus, game.noteShapes != 0,
                sp::isShadedPeer(game, cell));
    }
  }

  // The rules on top of the cells: hairlines between cells, the heavier rule
  // between boxes. Anything crossing a cell boundary gets its own pass.
  for (int index = 1; index < sk::kSize; ++index) {
    const bool boxEdge = index % sk::kBoxSize == 0;
    const int16_t weight = boxEdge ? toybox::kRule : toybox::kHairline;
    const int16_t offset = static_cast<int16_t>(index * kCell - weight / 2);
    screen.target().fill(fui::makeRect(static_cast<int16_t>(gridLeft(device) + offset), gridTop(), weight, kGridSide),
                         fui::Paint::solid(fui::Color::Black));
    screen.target().fill(fui::makeRect(gridLeft(device), static_cast<int16_t>(gridTop() + offset), kGridSide, weight),
                         fui::Paint::solid(fui::Color::Black));
  }

  // The selection last, over the rules, so no line can cut it.
  if (game.selected < sk::kCells) {
    const uint8_t value = sp::valueAt(game, game.selected);
    const bool dark = sp::isGiven(game, game.selected) || (value != 0 && value == game.focus);
    selectionFrame(screen, cellRect(device, game.selected), !dark);
  }
}

void drawPad(toybox::Screen& screen, const BoardModel& model) {
  const fui::DeviceContext& device = screen.device();
  for (int digit = 1; digit <= sk::kSize; ++digit) {
    const fui::Rect key = padKeyRect(device, digit);
    const int remaining = sp::remainingCount(model.game, digit);
    const bool focused = digit == model.game.focus;

    // A key with nothing left to place dims rather than disappearing: the pad
    // is a fixed shape and a hole in it would move every other key.
    if (remaining == 0) screen.target().fill(key, fui::Paint::dither(fui::Color::LightGray));
    frame(screen, key, focused ? toybox::kFrame : toybox::kHairline);

    char text[2];
    digitText(text, digit);
    fui::TextStyle label;
    label.font = toybox::kDisplayFont;
    label.align = fui::TextAlign::Center;
    screen.target().text(toybox::inkCentred(key, toybox::kDisplayCut), text, label);

    // SHOW REMAINING: how many are still to place, small, in the corner. Not
    // drawn at zero, where the dimmed key already says it.
    if (model.game.showRemaining != 0 && remaining > 0) {
      char count[2];
      digitText(count, remaining);
      fui::TextStyle corner;
      corner.font = toybox::kTileFont;
      corner.align = fui::TextAlign::Right;
      const int16_t reach = static_cast<int16_t>(toybox::kFrame + 4);
      screen.target().text(
          toybox::inkCentred(fui::makeRect(static_cast<int16_t>(key.right() - reach - 16),
                                           static_cast<int16_t>(key.y + reach), 16, toybox::kTileCut.inkHeight),
                             toybox::kTileCut),
          count, corner);
    }
  }
}

void drawRail(toybox::Screen& screen, const BoardModel& model) {
  const fui::DeviceContext& device = screen.device();
  const sp::Game& game = model.game;
  const bool solved = game.solvedFlag != 0;
  // Never drawn under the panel (see buildBoard), so only a carve silences it.
  const bool live = !model.generating;
  auto act = [live](const fui::ActionId action, const bool can) {
    return live && can ? action : static_cast<fui::ActionId>(fui::NO_ACTION);
  };

  // Row 0: NOTES, a toggle, inverted while on. Once the grid is finished the
  // slot is the SOLVED door instead, inverted because that is the payoff.
  fui::ButtonProps first;
  first.styles = toybox::rowStyles();
  if (model.generating) {
    first.label = "MAKING ONE";
    first.action = fui::NO_ACTION;
  } else if (solved) {
    first.label = "SOLVED";
    first.state = fui::StateSelected;
    first.action = act(ActionSeeResult, true);
  } else {
    first.label = "NOTES";
    first.state = game.notesMode != 0 ? fui::StateSelected : fui::StateNormal;
    first.action = act(ActionNotes, true);
  }
  screen.button(first, railRowRect(device, 0));

  // ERASE and UNDO dim when they cannot act, rather than vanishing.
  const bool canErase = sp::canErase(game);
  fui::ButtonProps erase;
  erase.label = "ERASE";
  erase.action = act(ActionErase, canErase);
  erase.styles = canErase && !model.generating ? toybox::rowStyles() : toybox::disabledStepperStyles();
  screen.button(erase, railRowRect(device, 1));

  const bool canUndo = sp::canUndo(game);
  fui::ButtonProps undo;
  undo.label = "UNDO";
  undo.action = act(ActionUndo, canUndo);
  undo.styles = canUndo && !model.generating ? toybox::rowStyles() : toybox::disabledStepperStyles();
  screen.button(undo, railRowRect(device, 2));

  fui::ButtonProps menu;
  menu.label = "MENU";
  menu.action = act(ActionOpenPanel, true);
  menu.styles = model.generating ? toybox::disabledStepperStyles() : toybox::rowStyles();
  screen.button(menu, railRowRect(device, 3));
}

// The MENU panel. A framed white sheet covering the page below the header,
// holding one row per thing it offers. The toggles are drawn the way NOTES
// is: inverted while on.
void drawPanel(toybox::Screen& screen, const BoardModel& model) {
  const fui::DeviceContext& device = screen.device();
  const sp::Game& game = model.game;
  const fui::Rect sheet = panelRect(device);
  screen.target().fill(sheet, fui::Paint::solid(fui::Color::White));
  frame(screen, sheet, toybox::kFrame);

  const bool playing = game.solvedFlag == 0;
  const int16_t rowWidth = static_cast<int16_t>(sheet.width - 2 * kPanelPad);
  for (int row = 0; row < kPanelRows; ++row) {
    const fui::Rect at = fui::makeRect(static_cast<int16_t>(sheet.x + kPanelPad),
                                       static_cast<int16_t>(sheet.y + kPanelPad + row * (kPanelRow + kPanelRowGap)),
                                       rowWidth, kPanelRow);
    fui::ButtonProps props;
    props.styles = toybox::rowStyles();
    props.action = ActionPanelRow;
    props.value = static_cast<int16_t>(row);
    bool enabled = true;
    switch (static_cast<PanelRow>(row)) {
      case PanelRow::Hint:
        props.label = "HINT";
        enabled = playing;
        break;
      case PanelRow::FillNotes:
        props.label = "FILL NOTES";
        enabled = playing;
        break;
      case PanelRow::Check:
        props.label = "CHECK";
        enabled = playing;
        break;
      case PanelRow::ShowRemaining:
        props.label = game.showRemaining != 0 ? "SHOW REMAINING: ON" : "SHOW REMAINING: OFF";
        props.state = game.showRemaining != 0 ? fui::StateSelected : fui::StateNormal;
        break;
      case PanelRow::ShadePeers:
        props.label = game.shadePeers != 0 ? "SHADE PEERS: ON" : "SHADE PEERS: OFF";
        props.state = game.shadePeers != 0 ? fui::StateSelected : fui::StateNormal;
        break;
      case PanelRow::NoteStyle:
        props.label = game.noteShapes != 0 ? "NOTES AS: DOTS" : "NOTES AS: DIGITS";
        break;
      case PanelRow::Close:
        props.label = "CLOSE";
        break;
      case PanelRow::Count:
        break;
    }
    if (!enabled) {
      props.action = fui::NO_ACTION;
      props.styles = toybox::disabledStepperStyles();
    }
    screen.button(props, at);
  }
}

// The front door's ornament: the puzzle you have open, at a tenth the size.
void drawMiniature(toybox::Screen& screen, const fui::Rect& room, const sp::Game& game, const bool hasGame) {
  const int16_t reach = static_cast<int16_t>(toybox::kMargin - 4);
  const int16_t across = static_cast<int16_t>(room.width - 2 * reach);
  const int16_t down = static_cast<int16_t>(room.height - 2 * reach);
  const int16_t side = across < down ? across : down;
  const int16_t cell = static_cast<int16_t>(side / sk::kSize);
  const int16_t grid = static_cast<int16_t>(cell * sk::kSize);
  const int16_t left = static_cast<int16_t>(room.x + (room.width - grid) / 2);
  const int16_t top = static_cast<int16_t>(room.y + (room.height - grid) / 2);

  cornerMarks(screen,
              fui::makeRect(static_cast<int16_t>(left - reach), static_cast<int16_t>(top - reach),
                            static_cast<int16_t>(grid + 2 * reach), static_cast<int16_t>(grid + 2 * reach)),
              16, toybox::kRule, false);

  for (int index = 0; index <= sk::kSize; index += sk::kBoxSize) {
    const int16_t offset = static_cast<int16_t>(index * cell - (index == sk::kSize ? toybox::kHairline : 0));
    screen.target().fill(fui::makeRect(static_cast<int16_t>(left + offset), top, toybox::kHairline, grid),
                         fui::Paint::dither(fui::Color::DarkGray));
    screen.target().fill(fui::makeRect(left, static_cast<int16_t>(top + offset), grid, toybox::kHairline),
                         fui::Paint::dither(fui::Color::DarkGray));
  }
  if (!hasGame) return;

  for (int index = 0; index < sk::kCells; ++index) {
    const int16_t x = static_cast<int16_t>(left + (index % sk::kSize) * cell);
    const int16_t y = static_cast<int16_t>(top + (index / sk::kSize) * cell);
    if (sp::isGiven(game, index)) {
      const int16_t pad = 2;
      screen.target().fill(fui::makeRect(static_cast<int16_t>(x + pad), static_cast<int16_t>(y + pad),
                                         static_cast<int16_t>(cell - 2 * pad), static_cast<int16_t>(cell - 2 * pad)),
                           fui::Paint::solid(fui::Color::Black));
    } else if (game.entry[index] != 0) {
      const int16_t pad = static_cast<int16_t>(cell / 3);
      screen.target().fill(fui::makeRect(static_cast<int16_t>(x + pad), static_cast<int16_t>(y + pad),
                                         static_cast<int16_t>(cell - 2 * pad), static_cast<int16_t>(cell - 2 * pad)),
                           fui::Paint::solid(fui::Color::Black));
    }
  }
}

// A lesson: a title, a sentence, and a small board before and after the
// gesture it describes.
struct Lesson {
  const char* title;
  const char* body;
  const char* detail;
  const char* before;
  const char* after;  // nullptr when the page is a rule rather than a gesture
  uint8_t digit;      // the digit a '!' or '?' stands for
};

// Face characters:
//   '.' empty            's' empty and selected     '-' empty and shaded
//   'p' pencilled        'A'-'I' a clue, 1-9         'a'-'i' a focused clue
//   '1'-'9' your digit   '!' your `digit`, clashing  '?' the clue `digit`, clashing
// The clues are 1, 3 and 9 in the corners so no face puts two of a digit in
// one box before the player has done anything.
const Lesson kLessons[] = {
    {"THE RULE", "EVERY ROW, COLUMN AND BOX HOLDS 1 TO 9.", "NO DIGIT TWICE IN ANY OF THEM.", "123456789", nullptr, 0},
    {"WRITING", "TAP A CELL, THEN A DIGIT TO WRITE IT.", "TAP IT AGAIN, THEN THE SAME DIGIT, TO CLEAR.", "A-C-s---I",
     "A.C.5...I", 0},
    {"NOTES", "NOTES ON: A CELL, THEN A DIGIT, PER MARK.", "EACH DOT SITS WHERE ITS DIGIT IS ON THE PAD.", "A.C.s...I",
     "A.C.p...I", 0},
    {"READING", "TAP A DIGIT TO LIGHT EVERY COPY OF IT.", "KEYS COUNT HOW MANY OF EACH ARE LEFT.", "A.C.....I",
     "a.C.....I", 0},
    {"MISTAKES", "A DIGIT THAT CLASHES IS STRUCK THROUGH.", "HINT AND CHECK ARE IN THE MENU.", "A.C.s...I", "A.?.!...I",
     3},
};
constexpr int kLessonCount = static_cast<int>(sizeof(kLessons) / sizeof(kLessons[0]));

void drawFace(toybox::Screen& screen, const fui::Rect& box, const char* face, const uint8_t lessonDigit,
              const bool outline = true) {
  const int16_t cell = static_cast<int16_t>(box.width / 3);
  const int16_t grid = static_cast<int16_t>(cell * 3);
  const int16_t left = static_cast<int16_t>(box.x + (box.width - grid) / 2);
  const int16_t top = static_cast<int16_t>(box.y + (box.height - grid) / 2);

  const bool big = cell >= 64;
  const fui::FontId font = big ? toybox::kDisplayFont : toybox::kUiFont;
  const toybox::CutMetrics cut = big ? toybox::kDisplayCut : toybox::kUiCut;

  for (int index = 0; index < 9; ++index) {
    const fui::Rect at = fui::makeRect(static_cast<int16_t>(left + (index % 3) * cell),
                                       static_cast<int16_t>(top + (index / 3) * cell), cell, cell);
    const char mark = face[index];
    const bool clue = (mark >= 'A' && mark <= 'I') || mark == '?';
    const bool focused = mark >= 'a' && mark <= 'i';
    const bool clash = mark == '!' || mark == '?';
    const bool dark = focused || clue;
    if (focused) {
      screen.target().fill(at, fui::Paint::solid(fui::Color::Black));
    } else if (clue) {
      screen.target().fill(at, fui::Paint::dither(fui::Color::DarkGray));
    } else if (mark == '-') {
      screen.target().fill(at, fui::Paint::dither(fui::Color::LightGray));
    }
    frame(screen, at, toybox::kHairline);

    char text[2] = {'\0', '\0'};
    if (mark >= 'A' && mark <= 'I') text[0] = static_cast<char>('1' + (mark - 'A'));
    if (focused) text[0] = static_cast<char>('1' + (mark - 'a'));
    if (mark >= '1' && mark <= '9') text[0] = mark;
    if (clash) text[0] = static_cast<char>('0' + lessonDigit);

    if (mark == 'p') {
      drawNotes(screen, at, static_cast<sk::Mask>(sk::bitFor(2) | sk::bitFor(6) | sk::bitFor(8)), 0, true, false);
    } else if (text[0] != '\0') {
      fui::TextStyle digit;
      digit.font = font;
      digit.align = fui::TextAlign::Center;
      digit.color = dark ? fui::Color::White : fui::Color::Black;
      screen.target().text(toybox::inkCentred(at, cut), text, digit);
      if (clash) strike(screen, at, Stroke::Rising);
    }
    if (mark == 's') selectionFrame(screen, at, true);
  }
  if (outline) {
    frame(screen,
          fui::makeRect(static_cast<int16_t>(left - toybox::kRule), static_cast<int16_t>(top - toybox::kRule),
                        static_cast<int16_t>(grid + 2 * toybox::kRule), static_cast<int16_t>(grid + 2 * toybox::kRule)),
          toybox::kRule);
  }
}

void drawArrow(toybox::Screen& screen, const fui::Rect& band) {
  const fui::Paint black = fui::Paint::solid(fui::Color::Black);
  const int16_t midX = static_cast<int16_t>(band.x + band.width / 2);
  const int16_t midY = static_cast<int16_t>(band.y + band.height / 2);
  const int16_t shaft = static_cast<int16_t>(band.height / 2);
  screen.target().fill(fui::makeRect(static_cast<int16_t>(midX - 2), static_cast<int16_t>(midY - shaft), 5, shaft),
                       black);
  for (int i = 0; i < 9; ++i) {
    const int16_t half = static_cast<int16_t>(9 - i);
    screen.target().fill(fui::makeRect(static_cast<int16_t>(midX - half), static_cast<int16_t>(midY + i),
                                       static_cast<int16_t>(half * 2), 1),
                         black);
  }
}

}  // namespace

fui::Rect cellRect(const fui::DeviceContext& device, const int cell) {
  return fui::makeRect(static_cast<int16_t>(gridLeft(device) + (cell % sk::kSize) * kCell),
                       static_cast<int16_t>(gridTop() + (cell / sk::kSize) * kCell), kCell, kCell);
}

bool cellAt(const fui::DeviceContext& device, const int x, const int y, int& cell) {
  const int16_t left = gridLeft(device);
  if (x < left || y < gridTop()) return false;
  const int column = (x - left) / kCell;
  const int row = (y - gridTop()) / kCell;
  if (column >= sk::kSize || row >= sk::kSize) return false;
  cell = row * sk::kSize + column;
  return true;
}

fui::Rect padKeyRect(const fui::DeviceContext& device, const int digit) {
  const int index = digit - 1;
  return fui::makeRect(static_cast<int16_t>(boardLeft(device) + (index % 3) * kPadKey),
                       static_cast<int16_t>(kPadTop + (index / 3) * kPadKey), kPadKey, kPadKey);
}

bool padKeyAt(const fui::DeviceContext& device, const int x, const int y, int& digit) {
  const int16_t left = boardLeft(device);
  if (x < left || y < kPadTop) return false;
  const int column = (x - left) / kPadKey;
  const int row = (y - kPadTop) / kPadKey;
  if (column >= 3 || row >= 3) return false;
  digit = row * 3 + column + 1;
  return true;
}

fui::Rect railRowRect(const fui::DeviceContext& device, const int row) {
  const int16_t left = static_cast<int16_t>(boardLeft(device) + kPadSide + kRailGap);
  const int16_t width = static_cast<int16_t>(boardLeft(device) + kBoardOuter - left);
  const int16_t top = static_cast<int16_t>(kPadTop + row * (kRailRow + kRailRowGap));
  return fui::makeRect(left, top, width, kRailRow);
}

fui::Rect panelRect(const fui::DeviceContext& device) {
  return fui::makeRect(boardLeft(device), kBoardTop, kPanelWidth, kPanelHeight);
}

void formatClock(const uint32_t ms, char* out, const int size) {
  const unsigned long seconds = static_cast<unsigned long>(ms / 1000);
  const unsigned long hours = seconds / 3600;
  if (hours > 0) {
    std::snprintf(out, static_cast<size_t>(size), "%lu:%02lu:%02lu", hours, (seconds / 60) % 60, seconds % 60);
    return;
  }
  std::snprintf(out, static_cast<size_t>(size), "%lu:%02lu", seconds / 60, seconds % 60);
}

void formatMinutes(const uint32_t ms, char* out, const int size) {
  const unsigned long minutes = static_cast<unsigned long>(ms / 60000);
  if (minutes >= 60) {
    std::snprintf(out, static_cast<size_t>(size), "%luH %02luM", minutes / 60, minutes % 60);
    return;
  }
  std::snprintf(out, static_cast<size_t>(size), "%lu MIN", minutes);
}

int howToPages() { return kLessonCount; }

fui::Rect menuDoors(toybox::Screen& screen, const MenuModel& model, char* levelRow, const int levelRowSize) {
  std::snprintf(levelRow, static_cast<size_t>(levelRowSize), "%s", sk::levelName(model.level));
  fui::ListItem rows[static_cast<int>(MenuRow::Count)] = {};
  rows[static_cast<int>(MenuRow::Level)].label = "DIFFICULTY";
  rows[static_cast<int>(MenuRow::Level)].value = levelRow;
  rows[static_cast<int>(MenuRow::Level)].actionValue = static_cast<int16_t>(MenuRow::Level);
  rows[static_cast<int>(MenuRow::HowTo)].label = "HOW TO PLAY";
  rows[static_cast<int>(MenuRow::HowTo)].actionValue = static_cast<int16_t>(MenuRow::HowTo);

  fui::ListProps list;
  list.items = rows;
  list.count = static_cast<uint16_t>(MenuRow::Count);
  list.selectedIndex = static_cast<int16_t>(model.selected);
  list.action = ActionMenuRow;
  const int count = static_cast<int>(MenuRow::Count);
  const int16_t height =
      static_cast<int16_t>(count * toybox::kRowHeight + (count - 1) * toybox::kGutter / 2 + toybox::kGutter);
  const fui::Rect content = screen.contentRect();
  screen.list(list, height, fui::LayoutAnchor::Bottom);
  return fui::makeRect(content.x, static_cast<int16_t>(content.bottom() - height), content.width, height);
}

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  toyboxChrome(screen, "SUDOKU+", sk::levelName(model.level));

  // The button and the caption are two renderings of ONE fact, written from one
  // exhaustive switch over it.
  const sp::MenuOffer offer = sp::menuOffer(model.game, model.hasGame, model.level);
  const char* action = offer == sp::MenuOffer::Resume ? "RESUME" : "NEW PUZZLE";

  char state[48];
  switch (offer) {
    case sp::MenuOffer::OtherLevel:
      std::snprintf(state, sizeof(state), "%s, STARTING FRESH", sk::levelName(model.level));
      break;
    case sp::MenuOffer::Resume: {
      // How long you have been at it, not how much is left: SUDOKU+ keeps no
      // count of empty cells anywhere.
      char clock[16];
      formatClock(model.game.elapsedMs, clock, sizeof(clock));
      std::snprintf(state, sizeof(state), "IN PROGRESS, %s", clock);
      break;
    }
    case sp::MenuOffer::Solved:
      std::snprintf(state, sizeof(state), "LAST ONE SOLVED");
      break;
    case sp::MenuOffer::Fresh:
      std::snprintf(state, sizeof(state), "NOT STARTED");
      break;
  }

  char record[56];
  const int index = static_cast<int>(model.level);
  if (model.record.bestMs[index] != 0) {
    char best[16];
    formatClock(model.record.bestMs[index], best, sizeof(best));
    std::snprintf(record, sizeof(record), "%d SOLVED   BEST %s", sp::totalSolved(model.record), best);
  } else {
    std::snprintf(record, sizeof(record), "%d SOLVED   NO %s TIME YET", sp::totalSolved(model.record),
                  sk::levelName(model.level));
  }

  char levelRow[32];
  const fui::Rect doors = menuDoors(screen, model, levelRow, sizeof(levelRow));
  const fui::Rect content = screen.contentRect();

  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Left;
  screen.target().text(toybox::inkCentred(screen.takeTop(34), toybox::kUiCut), state, body);

  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Left;
  screen.target().text(toybox::inkCentred(screen.takeTop(textBand(1, toybox::kTileCut)), toybox::kTileCut), record,
                       small);

  fui::ButtonProps play;
  play.label = action;
  play.action = ActionPlay;
  const fui::Rect pill = fui::makeRect(content.x, static_cast<int16_t>(doors.y - toybox::kPillHeight - toybox::kGutter),
                                       content.width, toybox::kPillHeight);
  screen.button(play, pill);

  const int16_t top = static_cast<int16_t>(screen.body().y + toybox::kGutter);
  drawMiniature(screen,
                fui::makeRect(content.x, top, content.width, static_cast<int16_t>(pill.y - toybox::kGutter - top)),
                model.game, model.hasGame);
}

void buildHowTo(toybox::Screen& screen, const HowToModel& model) {
  const int page = model.page < 0 ? 0 : (model.page >= kLessonCount ? kLessonCount - 1 : model.page);
  const Lesson& lesson = kLessons[page];
  char progress[toybox::kOfCounterChars];
  std::snprintf(progress, sizeof(progress), "%d OF %d", page + 1, kLessonCount);
  toyboxChrome(screen, "HOW TO PLAY", progress);

  // Taken before the page's own drawing, so no branch can skip the way forward.
  fui::ButtonProps next;
  next.label = page + 1 < kLessonCount ? "NEXT" : "GOT IT";
  next.action = ActionHowToNext;
  screen.button(next, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  fui::TextStyle title;
  title.font = toybox::kDisplayFont;
  title.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(screen.takeTop(44), toybox::kDisplayCut), lesson.title, title);

  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Center;
  body.maxLines = 2;
  screen.target().text(screen.takeTop(textBand(2, toybox::kUiCut)), lesson.body, body);

  fui::TextStyle detail;
  detail.font = toybox::kTileFont;
  detail.align = fui::TextAlign::Center;
  const fui::Rect detailBand = screen.takeBottom(textBand(1, toybox::kTileCut), toybox::kGutter);
  screen.target().text(toybox::inkCentred(detailBand, toybox::kTileCut), lesson.detail, detail);

  screen.takeTop(toybox::kGutter);
  const fui::Rect room = screen.body();
  if (lesson.after == nullptr) {
    const int16_t side = room.width < room.height ? room.width : room.height;
    drawFace(screen,
             fui::makeRect(static_cast<int16_t>(room.x + (room.width - side) / 2),
                           static_cast<int16_t>(room.y + (room.height - side) / 2), side, side),
             lesson.before, lesson.digit);
  } else {
    const int16_t arrow = 54;
    const int16_t side = static_cast<int16_t>((room.height - arrow) / 2);
    const int16_t boardSide = side < room.width ? side : room.width;
    const int16_t top = static_cast<int16_t>(room.y + (room.height - (boardSide * 2 + arrow)) / 2);
    const int16_t left = static_cast<int16_t>(room.x + (room.width - boardSide) / 2);
    drawFace(screen, fui::makeRect(left, top, boardSide, boardSide), lesson.before, lesson.digit);
    drawArrow(screen, fui::makeRect(room.x, static_cast<int16_t>(top + boardSide), room.width, arrow));
    drawFace(screen, fui::makeRect(left, static_cast<int16_t>(top + boardSide + arrow), boardSide, boardSide),
             lesson.after, lesson.digit);
  }
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  // The header carries the clock, or the answer to the last thing asked of the
  // board (a hint's rule, ALL CORRECT) until the next edit.
  char right[32];
  const char* notice = sp::noticeText(model.game);
  if (model.generating) {
    std::snprintf(right, sizeof(right), "%s", sk::levelName(model.generatingLevel));
  } else if (notice != nullptr) {
    std::snprintf(right, sizeof(right), "%s", notice);
  } else {
    char minutes[16];
    formatMinutes(model.game.elapsedMs, minutes, sizeof(minutes));
    std::snprintf(right, sizeof(right), "%s  %s", sk::levelName(model.game.puzzle.level), minutes);
  }
  toyboxChrome(screen, "SUDOKU+", right);
  // The panel is the whole page below the header, and the board is not drawn
  // under it: nothing it would leave showing could answer a tap.
  if (model.panelOpen) {
    drawPanel(screen, model);
    return;
  }
  drawGrid(screen, model);
  drawPad(screen, model);
  drawRail(screen, model);
}

void buildResult(toybox::Screen& screen, const ResultModel& model) {
  toyboxChrome(screen, "SOLVED", sk::levelName(model.level));

  char clock[16];
  formatClock(model.elapsedMs, clock, sizeof(clock));
  const fui::Rect headline = screen.takeTop(72);
  fui::TextStyle big;
  big.font = toybox::kDisplayFont;
  big.align = fui::TextAlign::Left;
  screen.target().text(headline, clock, big);

  char under[48];
  if (model.hintsUsed > 0) {
    std::snprintf(under, sizeof(under), "%d HINT%s, SO NO TIME RECORDED", model.hintsUsed,
                  model.hintsUsed == 1 ? "" : "S");
  } else if (model.newBest) {
    std::snprintf(under, sizeof(under), "YOUR BEST %s YET", sk::levelName(model.level));
  } else if (model.bestMs != 0) {
    char best[16];
    formatClock(model.bestMs, best, sizeof(best));
    std::snprintf(under, sizeof(under), "YOUR BEST IS %s", best);
  } else {
    std::snprintf(under, sizeof(under), "UNAIDED");
  }
  fui::TextStyle body;
  body.font = toybox::kBodyFont;
  body.align = fui::TextAlign::Left;
  screen.target().text(screen.takeTop(34), under, body);

  const fui::Rect rule = screen.takeTop(toybox::kRule + 12);
  screen.target().fill(fui::makeRect(rule.x, static_cast<int16_t>(rule.y + 8), rule.width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));

  char detail[72];
  std::snprintf(detail, sizeof(detail), "%d CLUES   HARDEST %s   %d %s SOLVED", model.clues,
                sk::techniqueName(model.hardest), model.solvedAtThisLevel, sk::levelName(model.level));
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Left;
  screen.target().text(screen.takeTop(26), detail, small);

  fui::ButtonProps again;
  again.label = "ANOTHER";
  again.action = ActionAgain;
  screen.button(again, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  fui::ButtonProps done;
  done.label = "BACK TO THE GRID";
  done.action = ActionDone;
  done.styles = toybox::rowStyles();
  screen.button(done, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  drawMiniature(screen, screen.body(), model.game, true);
}

}  // namespace sudokuplusui
