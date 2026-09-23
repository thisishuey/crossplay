- source_spec: `_bmad-output/implementation-artifacts/spec-sudoku-plus-tweaks.md`
  summary: SUDOKU+ paints whole-screen swaps (Menu to Board, Board to Result, HowTo to Menu) with FAST_REFRESH, where Picross and others flash.
  evidence: Pre-existing. `paintFlashes` covers only the panel; other apps set flashOnNextPaint on wholesale content swaps. No user report of ghosting there yet.
