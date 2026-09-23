# Sudoku+

The same puzzles as [Sudoku](sudoku.md), played the other way round: you pick a
cell, then a digit. It is a separate game on the shelf rather than a setting
inside SUDOKU, so both ways of playing stay available and neither leaks into the
other.

Four layers, as usual, plus a save module: `SudokuPlusGame` (what the player
touches, freestanding), `SudokuPlusSave` (the file format, freestanding),
`SudokuPlusScreens` (freestanding builders), `SudokuPlusActivity` (the only part
that knows about hardware). There is no `SudokuPlusCore`. Generation, grading,
solving and hints come from `../sudoku/SudokuCore.h`, so both games carve
identical puzzles from the same seed and a fix to the solver reaches both.

What the two games share is the engine and nothing else: not a class, not a
namespace (`sudokuplus`, `sudokuplusui`), not a save (`/.crosspoint/sudokuplus.sav`
against SUDOKU's `sudoku.sav`), not a record.

## Playing it

| You do                                   | What happens                                                               |
| ---------------------------------------- | -------------------------------------------------------------------------- |
| Tap a cell                               | It is selected (a black-and-white frame). A digit in it focuses it.        |
| Tap the selected cell                    | It is deselected.                                                          |
| Tap a digit, a cell of yours selected    | The digit is written and the cell's notes clear. One undo step.            |
| Tap the digit that cell already holds    | It is cleared.                                                             |
| Tap a digit, NOTES on                    | That mark toggles in the cell's 3x3 mini-grid. On a filled cell, or for a digit a peer already holds, nothing: the mark could not be seen. |
| Tap a digit, nothing or a clue selected  | The focus moves to that digit, or clears if it was already there.          |
| ERASE                                    | The selected cell's digit and notes clear. Dimmed when nothing visible would clear. |
| UNDO                                     | One cell back, or one whole FILL NOTES.                                    |
| MENU                                     | The panel below, over the grid.                                            |

**Focus is how you read the board.** Every copy of the focused digit is
inverted, the pad key for it gets a heavy frame, and a pencilled mark of it is
filled in: a solid square among the dots, or a numeral knocked out of a black
chip.

**Notes are dots by default.** A pencil mark is a small hollow dot in its
digit's place in the cell, which is its place on the pad: 1 top-left, 9
bottom-right. The numerals are still there as NOTES AS: DIGITS in the MENU, but
at note size they are hard to read on this panel, and a dot's position carries
the same fact.

**SHADE PEERS lights the selected cell's row, column and box** in LightGray:
the three units the next digit has to agree with. Clues keep their DarkGray and
the focused digit its black; with nothing selected, nothing is shaded.

Selection and focus are separate facts. Tapping an empty cell leaves the focus
where it was, so you can walk the board with the 7s still lit.

## The rail

Four rows of 45px, beside the pad and exactly as tall as it
(4 x 45 + 3 x 9 = 207): NOTES, ERASE, UNDO, MENU. NOTES is a toggle and is drawn
inverted while it is on. Once the grid is finished its slot becomes SOLVED, the
door to the result screen, and the board locks: no cell, digit, ERASE, UNDO or
panel action writes to a finished grid.

There is deliberately no "N LEFT" readout and no HINT button on the rail. The
header carries the elapsed time instead, in whole minutes, repainted when the
minute turns and at no other time.

## The MENU panel

A framed sheet over the grid. While it is up the board beneath takes no taps
and the rail answers nothing; Back or CLOSE takes it down.

| Row                 | Does                                                                                   |
| ------------------- | -------------------------------------------------------------------------------------- |
| HINT                | Selects the cell a solver can prove next and names the rule in the header.             |
| FILL NOTES          | Pencils every legal candidate into every empty cell, as one undo step. Says NOTHING TO FILL when every cell already has exactly that. |
| CHECK               | Strikes your digits that disagree with the answer, or says ALL CORRECT.                |
| SHOW REMAINING      | Toggle: a small count in each pad key's corner of how many are still to place.         |
| SHADE PEERS         | Toggle: the LightGray shading described above.                                         |
| NOTES AS            | Toggle: pencil marks as DOTS (the default) or DIGITS.                                  |
| CLOSE               | Takes the panel down.                                                                  |

The three questions close the panel, because each answer is on the board. The
three toggles leave it open, so the row itself shows the new state. They
persist in the save and survive starting a new puzzle (SHOW REMAINING and SHADE
PEERS default ON, NOTES AS defaults to DOTS); NOTES
persists in the save but a new puzzle starts with it off.

HINT, CHECK's marks and ALL CORRECT all last **until the next edit** -- a digit,
a note, ERASE, UNDO or FILL NOTES. Moving the selection is not an edit. That is
one rule in one place, `commitEdit`, rather than a promise each path keeps.

A hinted solve counts as solved and sets no best time, as in SUDOKU. A wrong
digit outranks any deduction: HINT selects it and says WRONG DIGIT, because
anything proved past a wrong digit is a lie.

## Undo, and the one thing that is not one cell wide

Undo is SUDOKU's: one cell per step, and placing a digit never rewrites the
notes of its peers -- `visibleNotes` hides the marks a peer has taken at draw
time, so undoing the placement brings them back by itself.

FILL NOTES is the exception, and the only one. It writes up to 64 cells at once
and undoes as one step. Each `Change` in the ring carries a `batch` bit that
says "same step as the change before me", and `undoOnce` pops until it restores
a change without it. The ring is 96 slots of four bytes, so a whole fill fits
with room behind it. When the ring wraps past the first change of a fill, the
rest of that fill is dropped with it: half a fill undone would be a board
nobody ever saw.

## Drawing

- A clue is a DarkGray dither with a WHITE numeral.
- The focused digit is solid black with a white numeral, clue or not.
- A shaded peer (a cell in the selected cell's row, column or box, not a clue)
  is a LightGray dither, the lightest ground on the board.
- Everything else -- empty cells and your own digits -- is plain paper with a
  black numeral. No underline, no mark: your digits are simply the ones not on
  grey.
- The selection is a black frame and a white frame, inverted by the ground. On
  paper and LightGray it is 3px white outside a 2px black line, so it never
  merges with the board frame at the grid's edge; on a clue's DarkGray and on
  the focus it is 2px black outside a 3px white band.
- A clash and a CHECK mark are one stroke in two directions: a 3px black
  diagonal in a 2px white halo, which reads over a numeral of either colour. A
  clash rises (/), a digit CHECK found wrong falls (\), and a wrong digit that
  also clashes wears both. Neither is a frame, because the selection is one,
  nor a black ground, because black belongs to the focus.

## The save

`SudokuPlusSave.h` packs and unpacks the file, freestanding, so the round trip
is host-tested. It is SUDOKU's text format with NOTES and the three toggles
added to the header. Version 2 added NOTES AS at the end of the header; a
version 1 file still loads, with notes as dots. The answer is re-derived from the clues on load; the selection,
the focus, a hint and CHECK's marks are never saved, so a reopened board has
nothing selected. Nor is the undo history: it lives only for as long as the app
is open, so UNDO on a reopened board has nothing to give back.

## Tests

- `host-tests/sudokuplus/run.sh` -- every row of the interaction table above,
  undo of a note, ERASE, FILL NOTES as one step (and never undone by halves when
  the ring wraps), CHECK clearing on the next edit, HINT, the solved lock, and
  the save round trip including a truncated file. Needs no SDK.
- `host-tests/ui` -- the grid and pad hit tests are exact inverses of their
  rects, the rail is exactly as tall as the pad, the panel and every rail
  control stay on the 480x800 panel inside 24 interactions, and the panel makes
  the rail inert.
