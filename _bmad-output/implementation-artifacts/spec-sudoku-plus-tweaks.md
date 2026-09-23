---
title: 'Sudoku+ post-deploy tweaks: deselect on entry, full-page MENU with a clean refresh, grey notes'
type: 'feature'
created: '2026-09-23'
status: 'done'
baseline_commit: 'f6d1ee388b7245d31e4724916164205930a7bf72'
route: 'dispatch'
review_loop_iteration: 0
context:
  - '{project-root}/docs/apps/sudokuplus.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** Four frictions with SUDOKU+ now that it's on devices. After you write a digit or a note, the cell stays selected. Closing the MENU panel leaves ghosts of it on the board. The panel covers only the grid, yet it makes the pad and rail below it dead until you tap CLOSE. Pencil marks are drawn black, and the focused digit's mark gets a black box or chip.

**Approach:**
- A successful digit or note write deselects the cell.
- The panel becomes a full-page sheet below the header, and the board isn't drawn under it.
- Opening or closing the panel paints with FULL_REFRESH.
- Pencil marks are drawn grey. The focused digit's mark is plain black instead of boxed.

**Decisions (human, 2026-09-23):**
- Deselect applies to both a digit write and a note write, including clearing the digit a cell already holds.
- The MENU covers the whole page below the header, not just the grid.
- Unfocused notes are `dither(DarkGray)` (50%): LightGray's 25% leaves a 10px ring or note numeral faint and broken on the 1-bit panel. The focused digit's note is solid black with no box or chip: a black numeral in DIGITS, a solid black round dot in DOTS.

## Boundaries & Constraints

**Always:**
- Stay inside `src/apps_local/sudokuplus/`, `host-tests/` and `docs/apps/sudokuplus.md`.
- The pure rules keep living in `SudokuPlusGame.h`, and the screens stay free functions over `BoardModel`.
- Back still closes the panel first (`backAction`).
- The panel's toggle rows still keep it open, and its question rows still close it.

**Never:**
- Change the save format, the shared `sudoku::` engine, or the original SUDOKU.
- Deselect after a refused write: a note on a filled cell, or a note for a digit a peer holds.
- Flash on toggle taps while the panel is open.
- Flash on ordinary grid or pad taps.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|---|---|---|---|
| Digit write | Writable cell selected, NOTES off, tap 6 | 6 is written, the focus becomes 6, `selected == kNoCell` | N/A |
| Note write | Writable cell selected, NOTES on, tap a legal digit | The mark toggles, `selected == kNoCell`, the focus is unchanged | N/A |
| Refused note | Filled cell, or a digit a peer holds, NOTES on | Returns false, and the selection stays | N/A |
| Pad with no target | Nothing or a clue selected | Only the focus changes (unchanged behaviour) | N/A |
| Panel open/close | MENU tapped, or CLOSE/HINT/CHECK/FILL/Back | Those paints use FULL_REFRESH | N/A |
| Panel toggle | SHADE PEERS etc. | Panel stays open and repaints with FAST_REFRESH | N/A |

</frozen-after-approval>

## Code Map

- `src/apps_local/sudokuplus/SudokuPlusGame.h:tapDigit` -- the two write branches (note, entry). Add `game.selected = kNoCell` after the `commitEdit` in each. Leave `hasTarget`-false focus-only path and refusals untouched. `takeHint` still selects.
- `src/apps_local/sudokuplus/SudokuPlusActivity.{h,cpp}` -- add `bool flashOnNextPaint`, set on every panel open/close transition (ActionOpenPanel, `choosePanelRow` when it closes, BackAction::ClosePanel); `render()` ends `renderer.displayBuffer(flashOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH); flashOnNextPaint = false;` -- the Picross pattern (`src/apps_local/picross/PicrossActivity.cpp:454`). Grid/pad hit-test is already gated on `!panelOpen`.
- `src/apps_local/sudokuplus/SudokuPlusScreens.cpp` -- `buildBoard`: when `panelOpen`, draw chrome + `drawPanel` only (no grid/pad/rail, so no dead rail actions). `panelRect`: page content area (x = `boardLeft`, width `kBoardOuter`, y = `kBoardTop`, bottom = 800 − `toybox::kMargin`). Panel rows use `toybox::kRowHeight` (62). Drop the `kPanelHeight <= kGridSide` assert and the "over the grid" rationale; add a static_assert the rows fit. `drawNotes` (line ~167): unfocused marks grey (ring stroke and numeral colour), focused mark black — digits: black numeral, no chip; dots: solid black round dot (radius `kNoteShape/2`), no square. Knock out paper under each grey mark on a shaded cell (dots already fill white; digits need a paper patch) so grey never sits on LightGray dither. The rail's MENU `StateSelected` while open becomes moot.
- `src/apps_local/sudokuplus/SudokuPlusFlow.h`, `SudokuPlusScreens.h` -- comments claim the panel is drawn over the grid with the board in view; update.
- `host-tests/sudokuplus/test_sudokuplus.cpp` -- `testEnterAndSameDigit` (line 136 asserts the cell stays selected), `testNotes`, and later sequences calling `tapDigit` repeatedly on one cell must re-`tapCell` first.
- `host-tests/ui/test_ui.cpp:8971` `testTheSudokuPlusMenuPanelIsModalAndFits` -- asserts the sheet is inside the 450px grid; change to: the sheet covers the page below the header, and there are no grid/pad/rail texts or actions. Note-drawing tests near 9084+ may pin black note colours.
- `docs/apps/sudokuplus.md` -- lines 25-31, 38-46, 63-78 describe selection-after-write, the panel over the grid, and black notes.

## Tasks & Acceptance

**Execution:**
- [x] `src/apps_local/sudokuplus/SudokuPlusGame.h` -- deselect after both successful writes in `tapDigit`; update the header comment on selection.
- [x] `host-tests/sudokuplus/test_sudokuplus.cpp` -- assert deselect for digit and note writes and keep-selection for refused notes; fix sequences that relied on the selection persisting.
- [x] `src/apps_local/sudokuplus/SudokuPlusScreens.cpp` + `.h` -- full-page panel geometry, board skipped under the panel, grey/black notes with paper knock-out.
- [x] `src/apps_local/sudokuplus/SudokuPlusActivity.{h,cpp}` + `SudokuPlusFlow.h` -- `flashOnNextPaint` on panel open/close; comment updates.
- [x] `host-tests/ui/test_ui.cpp` -- panel covers the page, no rail/grid/pad under it; unfocused note drawn grey, focused note black with no chip or square.
- [x] `docs/apps/sudokuplus.md` -- match the new behaviour.

**Acceptance Criteria:**
- Given the panel is open, when the player taps anywhere below the header, then only panel rows answer, and there's no inert area.
- Given the panel was open, when it closes by any path, then the next paint is a full refresh, and the board has no leftover ghost.
- Given notes on a shaded cell, when drawn, then each grey mark sits on paper and stays legible.

## Implementation Notes

- The renderer has no dithered text (`lib/GfxRenderer` lacks `drawTextDither`, so FreeInkUI falls back to black), and `stroke()` has no dithered ink either. A grey numeral is drawn black and then greyed by `greyOut()`, which lays 1px paper lines on the odd anti-diagonals, the pixels the DarkGray dither leaves white. A grey ring is a grey disc with a paper disc inside it. No core edit.
- The flash is not set at each open or close site. `render()` compares the panel it draws against the last one it drew through `sudokuplus::paintFlashes` (in `SudokuPlusFlow.h`, host-tested). That covers every path, including merged updates, so the I/O matrix's two flash rows have a test.
- Verified with both host suites (597 and 92627 checks, 0 failed) and a `simulator_x4_pro` build and screenshots. No device build: a web session can't make one, so CI does.

## Spec Change Log

## Review Triage Log

| # | Source | Finding | Verdict | Evidence | Route |
|---|---|---|---|---|---|
| 1 | verification-gap | The activity's `panelShown` handoff to `paintFlashes` is untested; dropping `panelShown = drewPanel` or the `onEnter` reset keeps the suites green | medium | The only test feeds literal bools; no suite compiles `SudokuPlusActivity.cpp`. The ghosting fix could regress silently | patch |
| 2 | verification-gap, blind, edge-case | The HOW TO lessons teach the old flow: "TAP DIGITS TO PENCIL", "SAME DIGIT CLEARS IT", and a WRITING after-face that is still shaded | medium | `kLessons` at `SudokuPlusScreens.cpp:522-526`. After a write nothing is selected, so `isShadedPeer` is false and the same digit only toggles the focus | patch |
| 3 | verification-gap | ERASE is dimmed right after a write (`canErase` needs a target); the docs don't say so | low | A direct consequence of the decided deselect, and a one-line doc fix | patch |
| 4 | blind | Pencilling three marks now takes 3 cell taps | false | The human decision says deselect applies to note writes | reject |
| 5 | blind, edge-case | ERASE keeps the selection, but the header and docs say "clearing the digit a cell holds" deselects | low | The intent covers number entry only. ERASE keeping the selection is right, but the wording implies otherwise; reword to "tapping the digit the cell holds" | patch |
| 6 | blind | The save round-trip's `selected == kNoCell` check is vacuous now that the last write deselects before packing | low | Verified at `test_sudokuplus.cpp:~760`; tapping a cell before packing restores the check's meaning | patch |
| 7 | blind | Toggles now flip with the board out of sight | false | The full-page panel and keeping it open on toggles are both human-decided | reject |
| 8 | blind | Other whole-screen changes (Menu to Board, Board to Result) still paint FAST | low | Pre-existing, not caused by this change, and not in the intent | defer |
| 9 | blind | `SudokuPlusFlow.h` says "navigation, and nothing else", yet now holds the refresh rule | low | Direct wording fix | patch |
| 10 | blind | The new test sits under `testBackClosesThePanelFirst`'s comment | low | Direct move | patch |
| 11 | blind, edge-case | The `noteShapes` comment still says "hollow dot" | low | `SudokuPlusGame.h:~100` | patch |
| 12 | blind | `FakeTarget::Stroke::paint` is filled but never read | low | Direct deletion if nothing reads it | patch |
| 13 | blind | The grey rendering was never looked at on a panel | false | Simulator screenshots were taken (qa-artifacts/sp-review.png); a device look is inherent to e-ink and is listed as a manual check | reject |
| 14 | blind | `kNotePatch = 11` rests on an unchecked glyph width | low | Unlikely (the font is fixed), and a guard would need a new measuring test | reject |
| 15 | blind | The docs' Tests section omits the new tests | low | Direct doc edit | patch |
| 16 | edge-case | The `paperUnder` test lambda matches x only, so a grey patch in the same pad column counts under the focused numeral | low | `test_ui.cpp:9475-9486`. Latent on another seed; add a y-overlap check | patch |
| 17 | edge-case | About 207px of sheet under CLOSE answers nothing, against the AC "no inert area" | medium | Rows are 62px from the top of a 689px sheet. Stretch the rows evenly to fill the sheet | patch |
| 18 | edge-case | The spec names `flashOnNextPaint`, and the code has no such flag | false | Implementation Notes record the deliberate move to `paintFlashes` | reject |

## Design Notes

**Why the flash is on open as well as close:** the full-page sheet replaces a dense board wholesale. A fast refresh would leave the grid ghosting under the menu, just as the menu ghosts on the board now. Toggles don't flash, so browsing the menu stays quiet.

## Verification

**Commands:**
- `bash host-tests/sudokuplus/run.sh` -- expected: all pass
- `bash host-tests/ui/run.sh` -- expected: all pass
- `./scripts_local/check.sh --tests 2>&1 | grep -o 'CHECKSH-VERDICT: [a-z-]*'` -- expected: `green` or `host-green-device-skipped`

**Manual checks:**
- Screenshot the board with notes, and the open panel, in `simulator_x4_pro` (run-crossplay-sim skill).
