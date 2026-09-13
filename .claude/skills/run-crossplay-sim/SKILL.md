---
name: run-crossplay-sim
description: Build, run, drive and screenshot the CrossPlay firmware in the X4 Pro desktop simulator. Use when asked to run the app, launch the simulator, take a screenshot of a game or app, reproduce a UI bug, verify a screen renders, drive a tap sequence, or test local multiplayer between two devices.
---

# Run the CrossPlay simulator

The firmware runs on the desktop as `simulator_x4_pro`, an SDL build of the
whole image. There is no device in the loop and no window to click: agents drive
it with a **scripted input string** and get **BMP/PNG screenshots** back.

`scripts_local/sim-shot.sh` is the launcher. **`.claude/skills/run-crossplay-sim/drive.py`
is what you should call** — it wraps sim-shot.sh with the autostart title list,
a verdict on every screenshot, the landscape tap arithmetic, and the `xvfb-run`
wrapper that landscape screens do not render correctly without.

All paths below are relative to the repo root.

## Prerequisites

```bash
apt-get install -y libsdl2-dev xvfb
```

`.claude/hooks/session-start.sh` installs `libsdl2-dev` (with `pio`, `uv` and
Pillow) on every session start, but **not `xvfb`** — that is in the web
container's base image by luck, not by the hook. `xvfb` is not optional; see
Gotchas. Run the line above if `xvfb-run` is missing, and `drive.py` will warn
rather than quietly clip if it is.

## Build

No separate step: `drive.py shot` builds before every run, under a per-tree
lock. Cold, in a fresh container, the first run took **~2.5 minutes** (most of
it fetching the pinned `crosspoint-simulator` library). After that every
iteration — edit, rebuild, run, screenshot — is **~19 seconds**, and it is 19
seconds whether you changed one file or none: the fixed cost is PlatformIO's
dependency scan and the link, not the compile.

To build without running (1m47s measured, from a tree whose `.pio` was already
populated — the driver's own build path is consistently faster):

```bash
pio run -e simulator_x4_pro
```

Never `pio run` bare — `default_envs = default` is upstream's ESP32-C3 target.

## Run (agent path)

### See what you can open

```bash
./.claude/skills/run-crossplay-sim/drive.py titles
```

29 rows. Any of them is a `--app` argument, which sets `CROSSPLAY_AUTOSTART` and
opens that item **directly, skipping the shelf**. Always prefer this to tapping
your way in: a tap sequence through the shelf lands on a different game the
moment a row is added.

### Drive it and shoot it

```bash
./.claude/skills/run-crossplay-sim/drive.py shot --app SOLITAIRE \
  '3000:TAP:123,729;7000:QUIT' \
  '2500:qa-artifacts/sol-menu.bmp;5500:qa-artifacts/sol-board.bmp'
```

- **Input script**: `<ms>:<action>` joined by `;`.
  `TAP:x,y[,hold]`, `SWIPE:x1,y1,x2,y2[,ms]`, and the keys
  `BACK ENTER LEFT RIGHT UP DOWN POWER SLEEP HOME QUIT` (append `:<hold-ms>`).
  Coordinates are logical pixels; the panel is 480x800 portrait.
- **Screenshot script**: `<ms>:<path>` joined by `;`. Write `.bmp`; the PNG is
  made for you next to it. Read the PNG.
- Everything lands in `qa-artifacts/` (gitignored), with the full log at
  `qa-artifacts/sim.log`.

It prints the activity trace, then whether `--app` landed, then a line per
screenshot:

```
activity trace:
  [80] [DBG] [ACT] Entering activity: Boot
  [95] [DBG] [ACT] Entering activity: Home
  [95] [INF] [SHELF] Autostart into SOLITAIRE

autostart: opened SOLITAIRE

screenshot check:
  ok       qa-artifacts/sol-menu.png  ink=21.08%
  ok       qa-artifacts/sol-board.png  ink=18.52%
```

A `--app` title that matches nothing **exits 1** and says so. That check exists
because the firmware treats a miss as routine: it logs, stays on Home, and the
run otherwise succeeds. `--app SOLITARE` photographed Home at 4.11% ink — above
the blank threshold, so only the autostart line catches it.

`ink` is the fraction of non-background pixels. **`MISSING` or `BLANK` exits 1** —
both are silent in sim-shot.sh and both look exactly like an app that failed to
draw. Measured range on this tree: a shelf page or app menu is 17-21%, a dealt
Solitaire board 18%, and the sparsest real frame the firmware draws — the boot
splash, a logo and two words — is 2.03%. The threshold is 0.5%.

Useful flags: `--trace 'REGEX'` replaces the log filter (`--trace .` for the
whole log; the default is `Entering activity|[ERR]`, plus `Autostart` whenever
`--app` is set), `--out DIR` moves the artifacts, `--allow-blank` reports bad
shots without failing, `--no-xvfb` opts out of the wrapper.

**Then open the PNG and look at it.** The ink check catches a blank panel, not a
wrong one.

### Landscape screens

Solitaire is landscape for its whole life (`onEnter` turns the panel, `onExit`
turns it back); Forehead turns per screen, so the same app is portrait on some
views and 800x480 on others. The input script does **not** follow either of
them: it is normalised once at startup against the portrait size, so a tap read
off a landscape screenshot lands somewhere else. Convert it:

```bash
./.claude/skills/run-crossplay-sim/drive.py land 205 438   # -> 123,729
```

Verified against Solitaire's DEAL button: `TAP:123,729` deals the board,
`TAP:205,438` misses it and leaves the menu up. Screenshots come back correctly
rotated, so only the input needs this.

### Hand a reviewer a sequence, not three frames

```bash
./.claude/skills/run-crossplay-sim/drive.py sheet qa-artifacts/review.png \
  qa-artifacts/sol-menu.png qa-artifacts/sol-board.png
```

One labelled strip. Bugs in the relationship between two states are invisible in
unrelated stills — render the awkward ones on purpose (something selected, a
list at its longest, a container empty).

### Sleep and wake

Deep sleep is a chip reset, so the simulator re-execs the process. Drive the
whole cycle, never the halves:

```bash
CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE='5000:QUIT' \
CROSSPOINT_SIM_SCREENSHOTS_AFTER_WAKE='3000:qa-artifacts/after-wake.bmp' \
CROSSPLAY_AUTOSTART=SUDOKU \
xvfb-run -a ./scripts_local/sim-shot.sh '3000:SLEEP;6000:POWER' ''
```

The trace must show the game entered again on the second process, with no Boot
and no Home between:

```
[3409] [DBG] [ACT] Entering activity: Sleep
[93]   [INF] [SHELF] Wake: resuming SUDOKU
[93]   [DBG] [ACT] Entering activity: Sudoku
```

(The timestamps restarting at 93 are the re-exec, not a misread.)

### Two devices, for multiplayer

```bash
SIM_LINK_FRESH=1 CROSSPLAY_AUTOSTART=CHESS xvfb-run -a ./scripts_local/sim-link.sh \
  '3000:TAP:240,700;25000:QUIT' '18000:./qa-artifacts/link.bmp'
```

Two processes, two SD cards (`fs_link_a`, `fs_link_b`), discovering each other
over UDP. Screenshots get `-a`/`-b` before the extension. `SIM_LINK_INPUT_B`
and `SIM_LINK_SHOTS_B` make the run asymmetric. Verified here:

```
device a: [3310] [INF] [LINK] matched with 'SPIKY GRIM TONGUE', we move first
device b: [3321] [INF] [LINK] matched with 'CURLY BEADY POUT', they move first
```

The two found each other **~3.3s into the run**, about 300ms after the radios
came up; the shot at 18s was well clear. The device names are random per run, so
match on `matched with`, not on a name. `SIM_LINK_FRESH=1` wipes both cards
first, which you want whenever a stale save would move the taps.

## Run (human path)

`./scripts_local/dev.sh` opens a window and rebuilds on every change;
`./scripts_local/sim.sh` is a one-shot launch. Both need a real display and are
useless headless — the window opens into the offscreen driver and nobody sees
it. Not verified here.

## Test

```bash
bash host-tests/simcatchup/run.sh    # 0s, 7 checks — the sim patch suite
```

`./scripts_local/check.sh --tests` is the full host gate; read its verdict with
`grep -o 'CHECKSH-VERDICT: [a-z-]*'`, never `tail -1` or `$?`. Not run here — it
is 15-25 minutes.

## Gotchas

- **Landscape screens are clipped without `xvfb-run`.** With no `DISPLAY`, SDL
  falls back to its `offscreen` video driver, whose window does not honour
  `SDL_SetWindowSize`. An app that turns the panel landscape renders into a
  surface still 480 wide, and the screenshot comes back as the left 480 columns
  with the remaining 320 flat black — which reads as the app failing to draw
  half of itself. `drive.py shot` adds `xvfb-run -a` for you when `DISPLAY` is
  unset; `sim-shot.sh` and `sim-link.sh` called directly do not, so wrap them
  yourself. Portrait shots are byte-identical either way — same MD5, verified —
  which is why this goes unnoticed until the first landscape app.

- **`fs_agent/` persists between runs.** It is the agent's SD card and every run
  writes its saves there. The *same input script* gives a different screen on the
  second run: Solitaire's front door said `FRESH DECK` on the first run and
  `DEAL IN PLAY / 0 MOVES DEEP` on the next. `rm -rf fs_agent` before anything
  whose starting state matters. (It is gitignored, along with `qa-artifacts/`.)

- **The first run leaves an untracked 105MB `.pio-cache/` in the repo root.**
  `lib-sim.sh` points PlatformIO's shared object cache at the *workspace* — the
  directory above the trees, found by walking up for a `.xteink-workspace`
  marker. A single clone with no marker falls back to the repo's own parent
  resolution and lands inside the checkout. `.gitignore` covers `.pio`, not
  `.pio-cache`, so it shows up in `git status`. Leave it (it makes later builds
  cheap) but never `git add -A` it.

- **A screenshot scheduled after `QUIT` is never written**, and sim-shot.sh says
  nothing about it — it only converts the BMPs that happen to exist. `drive.py`
  fails on it instead. Put `QUIT` after your last shot, with a margin.

- **A screenshot path outside the out-dir silently produces nothing.** The
  simulator will not create a missing directory, and the BMP→PNG conversion only
  looks in the out-dir. `2100:strays/x.bmp` wrote no file and created no
  `strays/`.

- **An empty `fs_agent/books` changes Home's layout.** With no recent books
  there are no cover tiles above the menu, so every row index shifts. A
  shelf-row offset bug shipped because every scripted run used an empty card.
  Irrelevant if you use `--app`, which is the reason to use `--app`.

- **`--app` matching is exact and case-insensitive**, against the title in
  `src/apps_local/Shelf.cpp`. A miss is not an error to the firmware: it logs
  `Autostart: no item titled '...'` and leaves you on Home, so the run "works"
  and photographs the wrong screen. `drive.py` fails on it. Quote the ones with
  a space (`--app 'TOY BATTLE'`), and run `drive.py titles` rather than guessing.

- **Five apps never appear in the activity trace.** `Entering activity: X` comes
  from `Activity::onEnter`, and an app whose `onEnter` override does not call the
  base never logs it: **D&DIAGRAMS, INSIDER, MURDLE, PICROSS and SOLITAIRE**. So
  "drive some taps, then grep the trace for `Entering activity: X`" — the usual
  regression check — silently never matches for those five, and reads as the app
  failing to open. `[STACK] X left ...` is not a fallback either: it prints only
  on a new worst watermark, so an app that does not go deeper than an earlier one
  stays silent too. For `--app` runs the reliable line is
  `[SHELF] Autostart into <TITLE>`, which `drive.py` filters in and checks for
  you. (`sim-shot.sh` called directly hides it — its default filter is
  `Entering activity|[ERR]`.)

- **`No glyph for codepoint` in the trace is a layout bug, and sim-shot.sh exits
  1 on it.** Codepoint 8230 is the ellipsis the SDK truncates with, which the
  Toybox cuts above 10px do not carry — so an overflowing line does not clip, it
  just stops at a plausible place and the screenshot looks fine. Anything else is
  unsanitised text reaching the rasteriser.

- **The scripts refuse to run against another tree.** `require_same_tree` exits 2
  if your cwd is outside the tree the script belongs to. `drive.py` sets the cwd
  for you, so it works from any subdirectory.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Right ~320px of a landscape shot is solid black | No `DISPLAY`; SDL used `offscreen`. Use `drive.py shot`, or `xvfb-run -a` the script yourself. |
| `MISSING` on a screenshot | `QUIT` fires before that timestamp, or the path is outside the out-dir. |
| `BLANK` on a screenshot | The taps never reached that screen, or the shot lands mid-refresh. Check the activity trace. |
| Trace shows `Home` where you expected the app | `--app` title did not match. `drive.py titles`. |
| Screen differs from the last identical run | `fs_agent/` carried a save over. `rm -rf fs_agent`. |
| `error: this script drives <path> but you are in <other>` | Wrong tree. Use that tree's own `./scripts_local/`, not the workspace-root `./scripts/` symlinks. |
| `error: XDG_RUNTIME_DIR is invalid or not set` in sim.log | Harmless. SDL's Wayland probe failing before it falls through to a driver that works. |
| `build failed; see /tmp/xteink-build-*.log` | Read that log. `Arduino.h` macro-defines `word()` and `bit()`, so a method with either name compiles here and fails only at device link. |
