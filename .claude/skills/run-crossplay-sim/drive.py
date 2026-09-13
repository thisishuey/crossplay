#!/usr/bin/env python3
"""Drive the X4 Pro simulator headlessly and check what came back.

scripts_local/sim-shot.sh is the launcher; this wraps it with the three things
an agent needs and it does not do:

  * the autostart titles, read out of the registry instead of guessed at;
  * a verdict on every screenshot -- scheduled-but-never-written and
    written-but-blank are both silent in sim-shot.sh, and both look exactly
    like a feature that does not render;
  * landscape tap arithmetic, which the input script does not do for you.

Run `drive.py <command> --help` for the arguments of one command.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
SHELF = REPO / "src/apps_local/Shelf.cpp"
SIM_SHOT = REPO / "scripts_local/sim-shot.sh"

# The panel the input script is normalised against, always -- see `land`.
PORTRAIT_W, PORTRAIT_H = 480, 800

# Below this fraction of non-background pixels a shot is a flat panel. Measured
# on this tree: an app menu or a shelf page is 17-21% ink, and the sparsest real
# frame there is -- the boot splash, a logo and two words -- is 2.03%. So the
# threshold sits an order of magnitude under the emptiest screen the firmware
# actually draws, and only a panel with nothing on it trips it.
BLANK_INK = 0.005


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(2)


# --- titles ----------------------------------------------------------------

def read_titles() -> list[tuple[str, str]]:
    """(folder, title) for every shelf row, in registry order."""
    if not SHELF.exists():
        fail(f"{SHELF} is missing; this is not a crossplay tree")
    source = SHELF.read_text()
    out: list[tuple[str, str]] = []
    for table, folder in (("kGames", "Games"), ("kApps", "Apps")):
        match = re.search(
            rf"constexpr shelf::Item {table}\[\] = \{{(.*?)\n\}};", source, re.S
        )
        if not match:
            fail(f"could not find {table} in {SHELF}")
        for row in re.finditer(r'\{"([^"]+)"', match.group(1)):
            out.append((folder, row.group(1)))
    return out


def cmd_titles(args: argparse.Namespace) -> int:
    rows = read_titles()
    width = max(len(t) for _, t in rows)
    for folder, title in rows:
        print(f"  {title.ljust(width)}  {folder}")
    print(f"\n{len(rows)} rows. Pass one as --app; matching is case-insensitive")
    print("and exact, so quote the ones with a space: --app 'TOY BATTLE'.")
    return 0


# --- land ------------------------------------------------------------------

def cmd_land(args: argparse.Namespace) -> int:
    """Landscape logical coords -> the portrait ones the input script wants.

    The input script is normalised ONCE at startup against the portrait logical
    size, and only then does tapToLogical rotate it. So a tap aimed at a screen
    that called setOrientation(LandscapeCounterClockwise) has to be converted or
    it lands somewhere else entirely.
    """
    x = round(args.x * (PORTRAIT_W - 1) / 800)
    y = round(args.y * (PORTRAIT_H - 1) / 480)
    print(f"{x},{y}")
    return 0


# --- shot ------------------------------------------------------------------

def shot_paths(spec: str, out_dir: Path) -> list[Path]:
    """The PNG each '<ms>:<path>' entry should leave behind."""
    paths = []
    for entry in spec.split(";"):
        entry = entry.strip()
        if not entry or ":" not in entry:
            continue
        raw = entry.split(":", 1)[1]
        path = Path(raw)
        if not path.is_absolute():
            path = REPO / path
        paths.append(path.with_suffix(".png"))
    return paths


def ink_fraction(path: Path) -> float:
    """Fraction of pixels that are not the dominant (background) shade."""
    from PIL import Image

    with Image.open(path) as image:
        grey = image.convert("L")
        histogram = grey.histogram()
    total = sum(histogram)
    if total == 0:
        return 0.0
    return 1.0 - (max(histogram) / total)


def check_autostart(app: str, log: Path) -> int:
    """Say plainly whether --app opened the thing it named.

    A title that does not match is not an error to the firmware: it logs and
    leaves you on Home, so the run succeeds, the screenshots are of Home, and
    nothing in the default trace says so.

    The activity trace cannot answer this either. `Entering activity: X` comes
    from Activity::onEnter, and five apps_local activities override onEnter
    without calling the base -- D&DIAGRAMS, INSIDER, MURDLE, PICROSS and
    SOLITAIRE never appear in the trace at all. `[STACK] X left ...` only prints
    on a new worst watermark, so it cannot be relied on either. The shelf's own
    line is the one that always tells the truth.
    """
    if not log.exists():
        return 0
    text = log.read_text(errors="replace")
    if "Autostart: no item titled" in text:
        print(
            f"\nerror: no shelf item is titled {app!r}, so the run stayed on Home"
            "\n       and every screenshot above is of Home. `drive.py titles`"
            "\n       lists what you can pass; matching is exact.",
            file=sys.stderr,
        )
        return 1
    for line in text.splitlines():
        if "Autostart into" in line:
            print(f"\nautostart: opened {line.split('Autostart into')[1].strip()}")
            return 0
    print(
        f"\nwarning: nothing in the log says {app!r} was opened. The run may have"
        "\n         quit before the shelf routed anywhere.",
        file=sys.stderr,
    )
    return 0


def cmd_shot(args: argparse.Namespace) -> int:
    if not SIM_SHOT.exists():
        fail(f"{SIM_SHOT} is missing")
    out_dir = Path(args.out) if args.out else REPO / "qa-artifacts"
    if not out_dir.is_absolute():
        out_dir = REPO / out_dir

    expected = shot_paths(args.shots, out_dir) if args.shots else []
    for path in expected:
        # A shot left over from the last run reads as a pass for this one.
        path.unlink(missing_ok=True)
        path.with_suffix(".bmp").unlink(missing_ok=True)

    env = dict(os.environ)
    if args.app:
        env["CROSSPLAY_AUTOSTART"] = args.app
    if args.trace:
        env["SIM_LOG_GREP"] = args.trace
    elif args.app:
        # sim-shot.sh's default filter is `Entering activity|[ERR]`, which hides
        # the one line that says whether --app landed. Five apps do not even
        # emit an Entering line (see check_autostart), so on those the default
        # trace is Boot, Home, and nothing at all.
        env["SIM_LOG_GREP"] = "Entering activity|Autostart|\\[ERR\\]"

    command = [str(SIM_SHOT), args.input, args.shots or "", str(out_dir)]

    # With no DISPLAY, SDL picks its `offscreen` driver, whose window never
    # honours SDL_SetWindowSize. An app that turns the panel landscape
    # (setOrientation(LandscapeCounterClockwise), 800x480) therefore renders
    # into a surface still 480 wide, and the screenshot comes back as the left
    # 480 columns with the remaining 320 flat black -- which reads as the app
    # failing to draw half of itself. Under a real X server it resizes and the
    # whole frame is there. Portrait shots are identical either way, so the
    # wrapper goes on unconditionally rather than per-app.
    if not args.no_xvfb and not os.environ.get("DISPLAY"):
        xvfb = shutil.which("xvfb-run")
        if xvfb:
            command = [xvfb, "-a"] + command
        else:
            print(
                "warning: no DISPLAY and no xvfb-run. SDL will use its offscreen"
                "\n         driver, and any landscape screen will be clipped at"
                "\n         x=480 with black beyond. apt-get install xvfb",
                file=sys.stderr,
            )

    # cwd is REPO because sim-shot.sh refuses to drive a tree it is not in, and
    # because relative screenshot paths resolve against the simulator's cwd.
    run = subprocess.run(command, cwd=REPO, env=env)

    status = run.returncode
    if args.app:
        status = check_autostart(args.app, out_dir / "sim.log") or status

    if not expected:
        return status

    print("\nscreenshot check:")
    missing = blank = 0
    for path in expected:
        if not path.exists():
            print(f"  MISSING  {path.relative_to(REPO)}")
            missing += 1
            continue
        ink = ink_fraction(path)
        if ink < BLANK_INK:
            print(f"  BLANK    {path.relative_to(REPO)}  ink={ink * 100:.2f}%")
            blank += 1
        else:
            print(f"  ok       {path.relative_to(REPO)}  ink={ink * 100:.2f}%")

    if missing:
        print(
            f"\n{missing} screenshot(s) never written. Either the run quit before"
            "\nthat timestamp -- QUIT must come after the last shot -- or the path"
            f"\nis outside the out-dir ({out_dir.relative_to(REPO)}), where the"
            "\nBMP-to-PNG conversion cannot see it."
        )
    if blank:
        print(
            f"\n{blank} screenshot(s) are a flat panel. The app drew nothing at that"
            "\nmoment: usually the tap sequence never reached it, or the shot lands"
            "\nduring a full-screen refresh. Check the activity trace above."
        )
    if (missing or blank) and not args.allow_blank:
        return 1
    return status


# --- sheet -----------------------------------------------------------------

def cmd_sheet(args: argparse.Namespace) -> int:
    """One labelled strip from several shots, for handing to a reviewer.

    A reviewer given three unrelated frames finds geometry defects and misses
    every bug that lives between two states, so hand over the sequence.
    """
    from PIL import Image, ImageDraw

    shots = [Path(p) for p in args.shots]
    for path in shots:
        if not path.exists():
            fail(f"{path} does not exist")

    images = [Image.open(p).convert("RGB") for p in shots]
    label_h, pad = 22, 8
    height = max(i.height for i in images) + label_h + pad * 2
    width = sum(i.width for i in images) + pad * (len(images) + 1)

    sheet = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(sheet)
    x = pad
    for image, path in zip(images, shots):
        sheet.paste(image, (x, pad + label_h))
        draw.rectangle(
            [x - 1, pad + label_h - 1, x + image.width, pad + label_h + image.height],
            outline="black",
        )
        draw.text((x, pad), path.stem, fill="black")
        x += image.width + pad

    out = Path(args.out)
    if not out.is_absolute():
        out = REPO / out
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(f"wrote {out} ({width}x{height}, {len(images)} frames)")
    return 0


# --- entry point -----------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        prog="drive.py", description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("titles", help="list every CROSSPLAY_AUTOSTART title").set_defaults(
        func=cmd_titles
    )

    p_shot = sub.add_parser("shot", help="build, drive headless, verify the shots")
    p_shot.add_argument("input", help="'<ms>:<action>' entries joined by ';'")
    p_shot.add_argument("shots", nargs="?", default="", help="'<ms>:<path>' joined by ';'")
    p_shot.add_argument("--app", help="CROSSPLAY_AUTOSTART title; skips the shelf")
    p_shot.add_argument("--out", help="out-dir (default qa-artifacts/)")
    p_shot.add_argument("--trace", help="SIM_LOG_GREP regex; '.' for the whole log")
    p_shot.add_argument(
        "--no-xvfb", action="store_true",
        help="do not wrap in xvfb-run; landscape screens will be clipped",
    )
    p_shot.add_argument(
        "--allow-blank", action="store_true",
        help="report missing/blank shots but still exit on sim-shot.sh's status",
    )
    p_shot.set_defaults(func=cmd_shot)

    p_land = sub.add_parser("land", help="landscape x y -> portrait tap coords")
    p_land.add_argument("x", type=float)
    p_land.add_argument("y", type=float)
    p_land.set_defaults(func=cmd_land)

    p_sheet = sub.add_parser("sheet", help="label and join shots into one strip")
    p_sheet.add_argument("out")
    p_sheet.add_argument("shots", nargs="+")
    p_sheet.set_defaults(func=cmd_sheet)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
