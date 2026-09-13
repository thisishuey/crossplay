<!-- bmad:context -->
<!-- Verified 2026-09-13 against 50651d4. Managed by bmad-project-context; edits inside
     this block are replaced on refresh. Keep anything you want preserved outside the markers. -->

## crossplay (thisishuey fork)

E-reader firmware for the Xteink X4 Pro and Seeed reTerminal Sticky, both ESP32-S3
(dual-core Xtensa, 16MB flash, 8MB PSRAM). C++20, PlatformIO, no exceptions, no RTTI.
This fork exists to add games and small tools under `src/apps_local/`, to be sent to
`ma-r-s/crossplay`. Contributor guide in `docs/contributing/`, app docs in `docs/apps/`.

## Policy

- Games and interactive apps are in scope here. `LOCAL_SCOPE.md` overrides `SCOPE.md`,
  which is upstream's and forbids them -- cite LOCAL_SCOPE.md, never SCOPE.md.
- Keep changes inside `src/apps_local/`, `host-tests/`, `tools_local/`, `scripts_local/`,
  `docs/`. Before editing anything else, stop and ask: upstream owns it, and edits there
  conflict on every sync from `ma-r-s/crossplay`.
- Upstream PRs carry only those paths, never core. A change needing a core edit is a
  separate conversation, not a bigger PR.
- Never push or open a PR without explicit approval.
- Never add Claude or Codex self-attribution as a commit co-author or generated-by
  trailer. When superseding someone's PR, add that human as `Co-Authored-By`; skip bots.
- Never hand-edit generated files, regenerate them: `src/apps_local/ui/ToyboxIcons.h` and
  `src/components/icons/shelfIcons.h` (`tools_local/toybox/gen_toybox_icons.sh`),
  `lib/I18n/I18n*`, `src/network/html/*.generated.h`.
- A game's name may be someone's trademark: implement the game, claim no affiliation, and
  do not add it to a trademark list -- `THIRD-PARTY-NOTICES.md:107-118` explains why that
  list is deliberately not enumerated. Licensed artwork ships in `assets_local/` with its
  licence text.

## Where things are

- Adding a game or app: `docs/shelf.md:359` is the mechanical recipe,
  `docs/building-apps.md` is the method. Read both first.
- Start from `cp -r src/apps_local/sample src/apps_local/<name>`, never from a real app --
  the smallest real one is solitaire at ~1900 lines.
- Registration is one row in `kGames` or `kApps` at `src/apps_local/Shelf.cpp:51`, plus an
  icon line in `tools_local/toybox/icons.txt`. No ActivityManager method, no UIIcon
  variant, no i18n key, no platformio.ini entry.
- `docs/contributing/development-workflow.md` has the PR rules. `docs/workflow/` is the
  maintainer's internal board and orchestrator machinery, not a contributor process -- do
  not follow it.

## Running and verifying

- `./scripts_local/check.sh` is the gate: `--tests` for host suites only, `--committed` to
  verify HEAD instead of the working tree. A full run is 15-25 minutes.
- Read its verdict with `grep -o 'CHECKSH-VERDICT: [a-z-]*'`, never `tail -1` or `$?`.
  `green` and `host-green-device-skipped` pass; `withheld`, `failed`, `flashed`, and no
  token at all do not.
- Never run a bare `pio run` or `pio check`: `default_envs = default` is upstream's
  ESP32-C3 target. Name an env -- `-e x4pro`, `-e sticky`, `-e simulator_x4_pro`.
- Build `-e x4pro` before believing an app is done. `Arduino.h` defines `word()` and
  `bit()` as macros, so a method with either name compiles everywhere and fails only at
  device link.
- One suite: `bash host-tests/<name>/run.sh`. check.sh discovers them, and fails a suite
  whose directory holds a `test_*` file its `run.sh` never invokes.
- Run `pio check --fail-on-defect high` before a PR -- CI runs cppcheck, check.sh does not.
- `./bin/clang-format-fix -g` formats git-modified files and needs clang-format 21 or
  newer. Never invoke clang-format directly.
- Every session runs in a fresh, ephemeral environment. `.claude/hooks/session-start.sh`
  rebuilds it (submodules, hooksPath, clang-format 21, SDL2, PlatformIO, Python deps) and
  prints what it could not do. Outside a web session, run at least
  `git submodule update --init --recursive` and `git config core.hooksPath .githooks`
  yourself; builds fail on missing `freeink-sdk/` headers and commits fail the format gate.

## Conventions that differ from defaults

- Inside `src/apps_local/` only: raw `const char*` strings rather than `tr()`,
  `freeink::Icon` rather than `UIIcon`, and a function-pointer factory rather than
  `ActivityManager::goTo<App>()`. Every other rule still applies.
- Games here are touch-only by design. Do not add button navigation or a cursor.
- An app never names its own Back destination; call `shelf::leave(renderer, mappedInput)`.
- Screens are free functions over a model, not methods on the Activity -- that is what
  makes them host-testable.
- All SD access goes through `Storage` and `HalFile`; never SdFat, `FsFile`, `SdSpiCard`,
  or `SDCardManager`. Bypassing that mutex races the SPI state machine and panics FreeRTOS.
- Never write a bare `new`: under `-fno-exceptions` it calls `abort()` on OOM instead of
  returning null. Use `makeUniqueNoThrow` from `lib/Memory/Memory.h` and null-check it.
- Free in `onExit()` whatever `onEnter()` allocated, and delete FreeRTOS tasks there
  before the activity is destroyed.
- `std::string_view` is not null-terminated. At any C API boundary convert it:
  `std::string(v).c_str()`, or `snprintf(buf, n, "%.*s", (int)v.size(), v.data())`.
- Never place a file with `main()` under `src/` -- PlatformIO's `+<*>` filter picks it up
  and it replaces the firmware's.

## Known pitfalls

- Editing `Shelf.cpp` without updating `README.md` fails `host-tests/docsclaims`: it
  re-derives the "21 games and 7 apps" headline (`README.md:29`) and both tables from the
  registry, and checks row counts.
- A new multiplayer game needs its id in the `GameId` enum and in `kAllGameIds`
  (`src/apps_local/link/LinkPlay.h:85`). Two games once took `0x0901` on separate branches
  and both passed.
- Record match results in `onMatchEnded()`, never at the end of `gameLoop()`: multiplayer
  returns before reaching it, and five games shipped counting zero matches.
- A new `<App>Screens.cpp` has to be added to the compile list in `host-tests/ui/run.sh`
  by hand, or the `-Werror` gate never sees it.
- Seed `fs_agent/.crosspoint/` before screenshotting, and set `CROSSPLAY_AUTOSTART=<title>`
  rather than relying on a tap sequence -- the same taps land on a different game on
  another machine.
- Keep upstream's `ci.yml`, `pr-formatting-check.yml` and `pr-firmware-links.yml` switched
  off in the Actions tab. They are upstream's, and they draw ESP32-C3 builds plus a
  semantic-title check CrossPlay abandoned. `crossplay-ci.yml` is the one that matters.
- The site deploys through Vercel, configured in `site/vercel.json`, not through any
  workflow. It cannot move to GitHub Pages: `site/api/` holds six serverless functions,
  and the emulator needs COOP/COEP headers and pre-brotli assets that Pages cannot serve.
- This clone has no remote for `ma-r-s/crossplay`, so every script resolving
  `origin/xteink` (`wt.sh`, `check.sh`, `device-build-needed.sh`) measures against this
  fork, not the upstream one.

<!-- /bmad:context -->
