---
title: 'Hex: the connection game, for the game shelf'
type: 'feature'
created: '2026-09-13'
status: 'in-progress'
baseline_commit: '0a99092e29aafdcb4613837391d578e02c2ee59f'
route: 'dispatch'
review_loop_iteration: 0
context: ['{project-root}/docs/shelf.md', '{project-root}/docs/building-apps.md']
---

<frozen-after-approval reason="human-owned intent -- do not modify unless human renegotiates">

## Intent

**Problem:** The shelf has 21 games and no connection game. Hex (Hein/Nash, 1942) is a
public-domain abstract with trivial rules, no draws, and a board that suits e-ink: place a
stone, connect your two edges.

**Approach:** A new touch-only game at `src/apps_local/hex/`, built to the house split
(freestanding Core, host-testable Screens, thin Activity). An 11x11 rhombus of flat-top hexagons
drawn with the `triangle()` primitive, laid corner to corner from the screen's top left to its
bottom right so the board fills the portrait panel. Two opponent settings, COMPUTER and HUMAN, exactly as Go has
them, plus two-device play over the existing `linkplay` layer as a PLAY NEARBY row.

## Boundaries & Constraints

**Always:** Touch only -- no cursor, no button navigation; Back is the sole button, handled by
`shelf::leave(renderer, mappedInput)`. Keep changes inside `src/apps_local/`, `host-tests/`,
`tools_local/`, `docs/`, `README.md`. Raw `const char*`, not `tr()`. `freeink::Icon`, not
`UIIcon`. Screens stay free functions over a plain model, compiling with only the FreeInkUI
include path. The brain is freestanding and deterministic: no heap, no clock, no Arduino,
randomness only through an explicit `uint32_t& seed`. No bare `new` -- `makeUniqueNoThrow`,
null-checked. Free in `onExit()` what `onEnter()` allocated, deleting any FreeRTOS task first.
Record the link result in `onMatchEnded()`, never at the end of `gameLoop()`. Hit-test geometry
and draw geometry must be exact inverses.

**Never:** Do not edit core/upstream paths (anything outside the list above) -- that conflicts on
every upstream sync. Do not hand-edit `ToyboxIcons.h`; regenerate. No affiliation claimed with any
trademark holder, and no trademark-list entry. No SdFat/FsFile/SDCardManager -- SD access via
`Storage`/`HalFile` only. No board size but 11x11. No `main()` under `src/`. **No swap rule (pie rule)**: the opening move is never stealable, a decided trade -- the first player keeps the standard
Hex advantage. **The board is drawn
in one fixed orientation and never flips or rotates for either player**, so there is no
pass-and-play/face-to-face distinction to implement. Nearby is not an opponent setting:
multiplayer is an action, per `chess/ChessScreens.h:35-42`.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Place a stone | Tap an empty cell, your turn | Stone placed, turn passes, board repaints | N/A |
| Tap occupied cell | Cell already holds a stone | Ignored, no repaint (a no-op repaint is a bug here) | N/A |
| Win detected | A move joins both of a player's edges | Game over, winning chain shown, board frozen | N/A |
| Full board | Every cell filled | Exactly one winner by the Hex theorem; "no winner" is a bug | Assert in host tests |
| Brain out of time | Level budget expires before any playout | Return the most-visited or a legal centre-weighted cell, never an illegal one | Log, never abort |
| Link: adopt state | `takeOpponent()` returns a state | Merge, redraw, recompute turn from `linkPhase()` | Size mismatch: ignore packet |
| Link: opponent leaves | Phase goes Over | Layer draws its headline; final board still wins the screen | Handled by layer |
| Save round-trip | `.cfg` line shorter than current version | Accept the fields present, default the rest | Version mismatch: discard, defaults |

</frozen-after-approval>

## Code Map

- `src/apps_local/sample/` -- the `cp -r` starting point (2 files, 66 lines). Never start from a real app.
- `src/apps_local/go/` -- closest reference: board game + brain + link + save. `GoFlow.h:45`
  (`enum class Opponent { Computer, Human }`, the model to copy) and `:49` (`Level`);
  `GoScreens.cpp:210-236` (`stoneCentre`/`pointAt`, the geometry pair); `GoActivity.cpp:24-54`
  (search-task stack rationale), `:89-97` (`xTaskCreatePinnedToCore`, core 1), `:111-124`
  (teardown), `:564-567` (`onMatchEnded` = record + show result); `GoSave.h:40-78` (pack/unpack).
- `src/apps_local/chess/ChessActivity.cpp:367-405` -- settings load/save tolerating a short line;
  `:1018-1046` tap cycles a row value in place; `:1050-1078` touch-only routing.
- `src/apps_local/link/LinkPlay.h:62-80` -- `GameId`; `0x0B01` is free. `:92-95` `kAllGameIds`
  (a `static_assert` catches collisions). `LinkActivity.h:70-104` -- the pure virtuals to implement;
  `:117-119` `enterLink`/`leaveLink`. Payload cap 192 bytes (`LinkProtocol.h:40`).
- `src/apps_local/ui/ToyboxScreen.h:458-480` -- `disc`/`ring` composed from scanline `fill()`s, the
  model for composing a hexagon. Stones reuse Go's idiom verbatim (`GoScreens.cpp:53-54`): a black
  `disc` at `r`, then a `disc` at `r-3` in the stone colour, so Hex stones match Go's on the shelf. `ToyboxMetrics.h:14-47` -- `kChromeHeight` 83, `kMargin` 16.
- `freeink-sdk/.../FreeInkUICore.h:702-727` -- the complete `DrawTarget` set. `triangle(a,b,c,paint)`
  is the only arbitrary-shape fill screens may use. No circle, no n-gon.
- `src/apps_local/Shelf.cpp:51` (`kGames`), `Shelf.h:54` (`Item{title, icon, create}`).
- `host-tests/ui/run.sh` -- by-hand compile list; `test_ui.cpp:5744+`
  (`testThePointYouTapIsThePointTheRulesGet`, the geometry-inverse test to mirror).
- `host-tests/docsclaims/run.sh` -- re-derives README counts, tables and the PLAY NEARBY sentence
  from the registry, plus the `docs/buttons.md` census.

**Do not change:** anything under `src/` outside `src/apps_local/`, or under `freeink-sdk/`, `lib/`.

## Tasks & Acceptance

**Execution:**
- [ ] `src/apps_local/hex/HexCore.h/.cpp` -- board model, 6-neighbour adjacency, union-find win
      detection over 121 cells + 4 virtual edge nodes. Freestanding, fixed arrays, no heap.
- [ ] `src/apps_local/hex/HexBrain.h/.cpp` -- UCT/MCTS. `Level` selects both the playout policy
      (plain / bridge-aware / bridge+AMAF) and the budget, per Design Notes. Root-level immediate
      win/block check, precomputed bridge table, caller-owned node pool, lent clock.
- [ ] `src/apps_local/hex/HexFlow.h` -- `Screen`/`Tap`/`back()` enums, header-only constexpr.
- [ ] `src/apps_local/hex/HexScreens.h/.cpp` -- `buildMenu`/`buildSettings`/`buildBoard`/`buildResult`,
      plus `cellCentre()`/`cellAt()` as exact inverses, hexagons as triangle fans. Settings rows:
      OPPONENT (COMPUTER/HUMAN), LEVEL, PLAY AS.
- [ ] `src/apps_local/hex/HexSave.h/.cpp` -- versioned pack/unpack of settings + position.
- [ ] `src/apps_local/hex/HexActivity.h/.cpp` -- thin device layer: `LinkActivity` subclass, search task
      pinned to core 1, `onMatchEnded()` recording the result, `preventAutoSleep()` while thinking.
- [ ] `src/apps_local/link/LinkPlay.h` -- add `Hex = 0x0B01` to `GameId` and `kAllGameIds`.
- [ ] `src/apps_local/Shelf.cpp` -- alphabetical `#include` plus one `kGames` row.
- [ ] `tools_local/toybox/icons.txt` -- add `hex = hexagon`; regenerate to scratch and splice, per the
      `icons.txt:1-6` warning that a full regen drops `icon_yahtzee_32` and `icon_connectfour_32`.
- [ ] `README.md` -- 21 -> 22 games, a Games table row, and Hex in the PLAY NEARBY sentence (Ten -> Eleven).
- [ ] `docs/buttons.md` -- bump the `Back` census 27 -> 28.
- [ ] `docs/apps/hex.md` -- rules, modes, brain behaviour.
- [ ] `host-tests/hex/run.sh` + `test_hex.cpp` -- rules, win detection, the full-board no-draw invariant,
      save round-trip including a short line, brain determinism for a fixed seed.
- [ ] `host-tests/ui/run.sh` + `test_ui.cpp` -- add `HexScreens.cpp`/`HexCore.cpp` to the compile list and
      a screens block, including the tap/draw geometry-inverse test over all 121 cells.
- [ ] `host-tests/link/test_hexlink.cpp` + `run.sh` stanza -- wire round-trip.

**Acceptance Criteria:**
- Given a finished game, when the winning stone is placed, then the connected chain is shown and further taps do nothing.
- Given opponent COMPUTER at any level, when the brain moves, then the UI stayed responsive and never tripped the watchdog.
- Given Easy, Normal and Hard, when each plays the others over a fixed seeded series, then Hard beats Normal and Normal beats Easy.
- Given the same position, level and seed, when the brain runs twice, then it returns the identical move.
- Given opponent HUMAN on one device, when the turn passes, then the board is drawn in the same orientation for both players.
- Given the board is drawn, then it runs corner to corner from top left to bottom right and all 121 cells sit clear of the chrome.
- Given a link match ends, when the result is recorded, then it happened in `onMatchEnded()` and the tally incremented exactly once.

## Implementation Notes

## Spec Change Log

## Review Triage Log

## Design Notes

Axial coordinates, `idx = r * 11 + c`. Six neighbours: `(r,c-1) (r,c+1) (r-1,c) (r-1,c+1)
(r+1,c-1) (r+1,c)`. Black connects top/bottom, White left/right (1-bit panel, so Go's black and
white stones, not Red/Blue).

Win detection is union-find over 121 cells plus 4 virtual edge nodes: on placing, union with
same-colour neighbours and with the edge node when the cell sits on that player's border;
`find(TOP) == find(BOTTOM)` wins. ~250 bytes, incremental, no per-move rescan.

The brain is UCT/MCTS. It exploits the Hex theorem: a full board has exactly one winner, so a
playout needs no legality checks and no mid-playout terminal test -- play out every empty cell,
then read the winner off one union-find pass.

Difficulty scales the *policy*, not just the budget. Cazenave and Saffidine measured the bridge
pattern in the playout policy at +105 Elo over naive UCT, and AMAF/RAVE on top of it (with UCT
exploration off) at a further +181 Elo -- together about what a 250-fold compute increase buys.
That gain is affordable here and 250x compute is not, so:

- Easy: plain UCT, no bridge, no AMAF, ~1-2k playouts. Playout is a single shuffle-and-fill pass.
  Still takes an immediate win and blocks an immediate loss at the root, so it never looks broken.
- Normal: bridge-aware playouts, ~8k.
- Hard: bridge + AMAF/RAVE, exploration off, ~30k or time-budgeted.

Consequence to plan for: bridge response is reactive, so from Normal up the playout cannot be one
shuffle-and-fill pass. Walk the shuffled order instead, and when the opponent intrudes into a
bridge play the saving cell rather than the next cell in the queue -- O(1) per move against a
precomputed bridge table. Budget by level mirroring `GoMichi.h:28-38` (`simulations` + `budgetMs`,
clock lent so host tests stay deterministic). Virtual connections / H-search are the next tier up
(MoHex territory) and are deliberately out of scope.

Geometry is **flat-top**, `cx = s*1.5*c`, `cy = s*sqrt(3)*(r + c/2)`, which runs the rhombus
corner to corner from the panel's top left to its bottom right. The bounding box is `17s` by
`27.7s`, so this layout is height-bound where the conventional pointy-top drawing is width-bound
and wastes most of the panel: `s ~= 24` gives roughly a 423 x 690 board with a 50px cell, against
28px for the same board drawn wide. That also puts the touch targets comfortably above Go's 33px
pitch. Keep hexagons axis-aligned -- the true best fit is an arbitrary ~57 degree rotation, but it
buys only ~12% and costs clean edges on a 1-bit panel. Take both extents from `device`, never
hardcode 480 or 800. Each hexagon is a 4-triangle fan; outlines are 6 `line()` calls.

This is purely a pixel mapping: the axial neighbour set above is unchanged by it, so `HexCore`
and the brain are unaffected and only `HexScreens` carries the layout.

## Verification

**Commands:**
- `bash host-tests/hex/run.sh` -- expected: all checks pass, zero failures.
- `bash host-tests/ui/run.sh` -- expected: passes, including the geometry-inverse test.
- `bash host-tests/link/run.sh` and `bash host-tests/docsclaims/run.sh` -- expected: pass.
- `pio run -e x4pro` -- expected: links. Catches `word()`/`bit()` macro collisions that only fail at device link.
- `pio check --fail-on-defect high` -- expected: no high-severity defects (CI runs cppcheck; check.sh does not).
- `./bin/clang-format-fix -g` then `./scripts_local/check.sh` -- expected:
  `grep -o 'CHECKSH-VERDICT: [a-z-]*'` yields `green` or `host-green-device-skipped`.
