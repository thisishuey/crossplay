---
title: 'Sudoku+: a copy of Sudoku with cell-first input, notes mode and peer shading'
type: 'feature'
created: '2026-09-23'
status: 'done'
baseline_commit: '0b4ac0091f5c9b0e0f1b632d8db9d8f384025dcb'
route: 'dispatch'
review_loop_iteration: 0
context:
  - '{project-root}/docs/shelf.md'
  - '{project-root}/docs/building-apps.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** In Sudoku you pick a digit and then tap cells. Notes are a hidden 400ms hold. The rail spends its space on an "N LEFT" readout and a HINT the user doesn't want. Arming a digit only brackets its copies, and doesn't show where that digit can't go.

**Approach:** Copy Sudoku into a new, separately registered game and leave the original untouched. The copy works cell-first: you select a cell, then tap a digit. The rail has four rows: NOTES (a toggle), ERASE, UNDO and MENU. Every copy of the focused digit is highlighted clearly, and the empty cells in each copy's row, column and box are dimmed.

**Decisions (human, 2026-09-23):**
- Shelf title `SUDOKU+`.
- MENU replaces HINT. It opens a modal panel drawn over the board with these rows:
  - HINT: select the hint cell and put the technique name in the header until the next edit. A hinted solve sets no best time.
  - FILL NOTES: pencil every legal candidate into every empty cell. It is a single undo step.
  - CHECK: mark your digits that disagree with the solution until the next edit, or show "ALL CORRECT".
  - A toggle for SHOW REMAINING: a small count in each pad key's corner.
  - A toggle for SHADE PEERS: dimming of the row, column and box.
  - CLOSE.
  - The toggles default to ON and persist in the save.
- The header shows the elapsed time, repainted at most once a minute.

## Boundaries & Constraints

**Always:**
- The copy lives in `src/apps_local/sudokuplus/` with its own class, its own namespaces (`sudokuplus`, `sudokuplusui`) and its own save file, `/.crosspoint/sudokuplus.sav`.
- It reuses the shared puzzle engine `sudoku::` from `../sudoku/SudokuCore.h` (generator and solver) and does not duplicate it.
- Touch-only. Back leaves via the existing flow and `shelf::leave`.
- Undo stays one cell wide, except FILL NOTES, which is one step that restores every note it changed. Placing a digit hides that digit's notes in its peers through `visibleNotes` at draw time; the stored notes are not rewritten.
- The pad is still hit-tested arithmetically, and the grid and pad hit tests stay exact inverses.
- The rail is still exactly as tall as the pad.

**Never:**
- Edit `src/apps_local/sudoku/`, or core paths outside the CLAUDE.md allowlist. The exceptions are the registration touch-points in `Shelf.cpp`, `README.md`, `site/index.html` and the icons list.
- A "N LEFT" counter, a HINT button on the rail, hold-to-pencil, or button or cursor navigation.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior |
|---|---|---|
| Select | Tap any cell | That cell becomes `selected` and is shown with corner brackets. If it holds a digit, `focus` becomes that digit. Tapping the selected cell again deselects it. |
| Enter | Editable cell selected, notes off, tap digit d | The entry becomes d and the cell's notes clear. `focus` becomes d. One undo step. |
| Same digit | Selected cell already holds d, tap d | The entry clears. |
| Note | Editable empty cell selected, notes on, tap d | Mark d toggles in the 3x3 mini-grid position. One undo step. |
| Note on a filled cell | Notes on, the selected cell holds an entry, tap d | Nothing happens. |
| No target | No cell selected, or a given selected, tap d | `focus` toggles to d (or clears if it was already d). The board is not written. |
| Erase | Editable cell selected, tap ERASE | The entry and the notes clear. One undo step. ERASE is disabled when there is nothing to clear. |
| Focus shading | `focus` = d | Every cell holding d is inverted. Every empty cell sharing a row, column or box with any d is dimmed (only if SHADE PEERS is on). The pad key for d gets a heavy frame. A note mark d is drawn emphasised. |
| Menu | Tap MENU | The modal opens and the board beneath takes no taps. Back or CLOSE dismisses it. |
| Solved | Last correct digit placed | The board locks. The NOTES slot becomes the "SOLVED" door to the result screen. |

</frozen-after-approval>

## Code Map

- `src/apps_local/sudoku/SudokuGame.h` -- the model to copy.
  - Drop `armed`, `holdCell` and `emptyCount`.
  - Add `selected`, `focus`, `notesMode`, `checkShown`, `showRemaining` and `shadePeers`.
  - Keep the hint fields, `firstWrong`, `visibleNotes`, `isClashing`, `placedCount`, undo, `menuOffer` and `Record`.
  - FILL NOTES undo needs a grouped change, for example a `batch` tag on each `Change` so `undoOnce` pops the whole group.
- `src/apps_local/sudoku/SudokuScreens.cpp:166-303` -- `drawGrid`, `drawPad` and `drawRail` are the pieces to rework. Layout constants are at :31-72.
- `src/apps_local/sudoku/SudokuActivity.cpp` -- touch dispatch at :333-451 (remove the hold timer at :32 and :387-421), `takeHint` at :111 (moves behind the modal's HINT row), save and load at :20-306 (new path; the text format adds the new flags, and `elapsedMs` already lives in `Game`).
- `src/apps_local/sudoku/SudokuCore.{h,cpp}` -- the shared engine. Include it; do not copy it.
- `src/apps_local/Shelf.cpp:37,67` -- the include and a new `kGames` row. The title must be 24 characters or fewer and unique.
- `tools_local/toybox/icons.txt:247` and `src/apps_local/ui/ToyboxIcons.h` -- the icon line, regenerated with `tools_local/toybox/gen_toybox_icons.sh` and never hand-edited.
- `README.md:29,66` and `site/index.html` -- the game count and table row. `host-tests/docsclaims` and `host-tests/site/shelf_coverage.py` enforce these.
- `docs/apps/sudoku.md` and `docs/apps/README.md:36` -- the pattern for the new app doc and its index row.
- `host-tests/sudoku/`, and `host-tests/ui/run.sh:75-78` plus `test_ui.cpp:7815-8316` -- the test patterns to mirror.

## Tasks & Acceptance

**Execution:**
- [x] `src/apps_local/sudokuplus/SudokuPlus{Game.h,Flow.h,Screens.h,Screens.cpp,Activity.h,Activity.cpp}` -- copy from sudoku, rename, and implement the matrix. The rail has four rows of 45px each (4 x 45 + 3 x 9 = 207): NOTES (a toggle, drawn in the selected style when on), ERASE, UNDO and MENU. The modal is a board sub-state, `menuOpen`, drawn as a framed panel of list rows over the grid.
- [x] `src/apps_local/sudokuplus/SudokuPlusScreens.cpp` -- rendering:
  - givens on plain paper; player digits marked with a short bar under the numeral
  - `focus` cells inverted, and dimmed peers dithered LightGray
  - a clash drawn as a heavy inset frame, so it doesn't share the black ground with `focus`
  - the selected cell shown with corner brackets
- [x] Registration: `Shelf.cpp` row, icon line and regeneration, `README.md`, `site/index.html`, and a `docs/apps/sudokuplus.md` page with its index row.
- [x] `host-tests/sudokuplus/{run.sh,test_sudokuplus.cpp}` -- cover every matrix row, plus: undo of a note; erase; FILL NOTES as one undo step; CHECK clearing on the next edit; and the solved lock.
- [x] `host-tests/ui/run.sh` and `test_ui.cpp` -- compile the new screens; the pad and grid hit tests are inverses; every rail control stays on the panel.

**Acceptance Criteria:**
- Given the original SUDOKU on the shelf, when SUDOKU+ is played and saved, then SUDOKU's save and records are unchanged.
- Given a partly solved SUDOKU+ board, when the app is left and reopened, then the entries, notes, notes mode, both toggles and the elapsed time are restored, and no cell is selected.
- Given the modal is open, when the panel is rendered, then every control fits within 24 interactions and stays on the 480x800 panel.

## Design Notes

Why not rewrite stored notes on placement: the original's header comment holds. Draw-time `visibleNotes` already gives "auto-remove notes from peers" visually, and keeps undo one cell wide.

## Implementation Notes

- **Rendering (human, 2026-09-23, final; overrides the Tasks rendering bullets and the earlier stipple note).** Givens: `fui::Paint::dither(fui::Color::DarkGray)` ground with a WHITE numeral. Dimmed peers (empty cells only): `fui::Paint::dither(fui::Color::LightGray)`; the hand-drawn stipple helper was removed. Empty cells and the player's own digits: plain white, black numeral, no underline bar. Focused digit: solid black with a white numeral, the same on a focused given.
- **Selection (human, 2026-09-23, final).** No corner brackets. One style on every ground, no branching: a 2px black outer frame with a 3px white frame just inside it (`selectionFrame()` in `SudokuPlusScreens.cpp`).
- **Clash (human, 2026-09-23).** A 3px diagonal slash through the cell (bottom-left to top-right, 6px in from the corners), white on a dark ground (DarkGray given or black focus) and black otherwise -- chosen over a double frame so it can never read as the selection, which is also a frame. CHECK's mark stays a horizontal bar through the numeral.
- **Review fixes (2026-09-23).** Panel rows and Back are freestanding and host-tested: `PanelRow` and `backAction()` live in `SudokuPlusFlow.h`, and `applyPanelRow()` in `SudokuPlusGame.h` (it returns whether the panel stays open). FILL NOTES that changes nothing says NOTHING TO FILL, a new `Notice`; header notices are budgeted at `kMaxHeaderNoticeChars = 15` rather than SUDOKU's 11-character capsule limit. `hintsUsed` saturates at 255. NOTES refuses a digit a peer already holds, and ERASE is live only when something visible would clear. The rail's actions are ignored while the panel is up, and the header names the level being carved while generating.
- **Save module.** `SudokuPlusSave.h` (freestanding `packState`/`unpackState`, plus `kStatePath`) was added beside the listed files so the reopen acceptance criterion is host-tested rather than trusted. The selection, focus, hint and CHECK marks are never saved.
- **Undo ring.** `Change` is four bytes with a 1-bit `batch` tag (`cell : 7, batch : 1`); the ring is 96 slots so a FILL NOTES (at most 64 cells) fits with room behind it. A wrap that overwrites a fill's first slot drops the rest of that fill, so a fill is never undone by halves.
- **Header.** Level plus whole minutes (`EASY  12 MIN`, `1H 05M` past the hour); a HINT rule, WRONG DIGIT, NOTHING YET or ALL CORRECT replaces it until the next edit. The activity requests a repaint only when the minute turns.
- **Panel behaviour.** HINT, FILL NOTES and CHECK close the panel (their answer is on the board); the two toggles keep it open. On a solved board HINT, FILL NOTES and CHECK are disabled. The rail is drawn but registers no actions while the panel is up, and the activity skips grid/pad hit-testing.
- **Icon.** The SDK (and so `gen_toybox_icons.sh`) was unavailable. `sudokuplus = hash` was added to `icons.txt`, and the `icon_sudokuplus_{24,32}` entries were spliced into `ToyboxIcons.h` as byte copies of `icon_sudoku_*` -- the same Lucide source through the same generator, so they are exactly what a regeneration would emit (the splice route `icons.txt:1-6` already prescribes). A distinct glyph can replace it with one manifest edit and a real regeneration.
- **Site.** No new card: a card needs exactly one real screenshot (`host-tests/site/page_structure.py`) and the simulator cannot be built here. SUDOKU+ is described in the Sudoku card's paragraph instead.
- **Not verifiable in this session:** `host-tests/ui` (needs `freeink-sdk`), `check.sh` (refuses to run without the submodule), `pio run -e x4pro`, `pio check`. The screens were syntax-checked against a local stub of the FreeInkUI surface they use, alongside SUDOKU's own screens as a control.

## Verification

**Commands:**
- `bash host-tests/sudokuplus/run.sh` -- all pass
- `./scripts_local/check.sh --tests 2>&1 | grep -o 'CHECKSH-VERDICT: [a-z-]*'` -- `host-green-device-skipped` (web session)
- `./bin/clang-format-fix -g` -- no diff afterwards
- Environment caveat: this session could not clone the `freeink-sdk` submodule, which is private. Any suite needing it, `host-tests/ui` included, cannot run here: write those tests carefully and compile what you can. The game-logic suite must not depend on the SDK, so it runs locally.

## Review Triage Log

| # | Finding | Verdict | Evidence / route |
|---|---|---|---|
| 1 | Rail actions route from a stale interaction table right after MENU opens | medium | `ActionErase/Undo/Notes/OpenPanel` do not check `panelOpen`, and the table rebuilds only on render. patch |
| 2 | `hintsUsed` uint8 wraps after 255 hints, so a hinted solve sets a best time | low | bare `++` in `takeHint`; a saturating increment is a direct fix. patch |
| 3 | Header shows the old puzzle's level while carving a new one | low | `buildBoard` prints `model.game.puzzle.level` when `generating`. patch (the old grid showing underneath is inherited from SUDOKU and out of scope) |
| 4 | ERASE live, and a NOTES toggle accepted, for marks `visibleNotes` hides | low | `canErase` reads the stored `note`; `tapDigit` in NOTES toggles digits a peer holds. patch |
| 5 | Panel row effects and Back-closes-panel are untested; Menu matrix row has no test that ran | medium | only in `SudokuPlusActivity.cpp`, which no suite compiles. patch: freestanding `applyPanelRow`, tested |
| 6 | Save not round-tripped when solved, when the solved flag is forced on, or with a different menuLevel | medium | pre-verified gap. patch (tests) |
| 7 | SHOW REMAINING, the focused key frame, the focused note chip and the past-the-hour clock are not asserted | medium | pre-verified gap. patch (ui tests) |
| 8 | `shelf_coverage.py` flattens `SUDOKU+` to `sudoku` and cannot see whether the site names it | medium | pre-verified gap. patch |
| 9 | FILL NOTES with nothing to change closes the panel silently | low | `fillNotes` returns false and the panel closes. patch: a "NOTHING TO FILL" notice |
| 10 | FILL NOTES replaces notes the player had narrowed | false | the frozen intent says "pencil every legal candidate"; one undo restores |
| 11 | README row padding; HOW TO lacks ERASE/UNDO/SHOW REMAINING; doc omits that undo is not saved | low | direct text fixes. patch |
| 12 | Save header values not range-checked; the note hex accepts signs | low | reject: the app writes its own saves, and the guards add complexity |
| 13 | Spec Tasks and Verification are stale | low | reject: the fix is a spec edit (overrides are in Implementation Notes) |
| 14 | UI tests, screens and activity never compiled against the real SDK | medium | environment: the submodule cannot be cloned here. Surface to the user; CI (`crossplay-ci.yml`) is the first real compile |
| 15 | `ToyboxIcons.h` hand-spliced; same icon as SUDOKU | medium | a CLAUDE.md violation that cannot be fixed here (the generator needs the SDK). Surface to the user |
| 16 | Spec file under `_bmad-output` is in the diff | false | `spec-hex-game.md` is already tracked there; this is the repo's convention |
