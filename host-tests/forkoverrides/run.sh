#!/bin/bash
# Fork changes that live inside UPSTREAM-owned files, and are still there.
#
# On 2026-08-25 the merge 315eef0d ("Merge tag '1.6.0rc' into app/upsync") took
# upstream's copy of src/activities/ActivityManager.cpp wholesale. Our side of
# that merge carried the fork's render-task stack override; upstream's side did
# not; the merge result did not. There was no conflict to resolve and nothing
# in the diff to review -- the line simply stopped existing. It shipped in
# v1.3.0 and in every release for six weeks, until Study and xkcd began
# panicking on open because the render task was back on upstream's 8192 while
# the fork's platformio.ini still said 16384. See board card #398.
#
# That is not a one-off. 83 of the ~384 upstream-owned files under src/ and
# lib/ carry fork modifications today, and every one of them can be reverted
# by the next sync in exactly the same silent way.
#
# So: an override whose loss would break something gets an entry here, and this
# suite fails if the pattern stops matching. The registry is deliberately SMALL
# -- it is for changes whose disappearance is silent and expensive, not for
# every diff against upstream. A whole-file diff guard would go red on every
# sync and be disabled within a week.
#
# Adding an entry: path, a regex that must still match, and why its loss hurts.
#
#   host-tests/forkoverrides/run.sh
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

python3 - "$ROOT" <<'PY'
import os
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(sys.argv[1])
checks = 0
failed = 0


def check(ok, label, detail=""):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1
        print(f"  FAIL {label}")
        if detail:
            for line in detail.splitlines():
                print(f"       {line}")


# (path, pattern that must still match, what its loss costs)
REGISTRY = [
    (
        "src/activities/ActivityManager.cpp",
        r"CROSSPOINT_RENDER_TASK_STACK",
        "The render task falls back to upstream's 8192. The fork's deepest\n"
        "screen needs 9520, so Study and xkcd panic on open with an empty\n"
        "panic message and no backtrace. This is card #398, and it shipped.",
    ),
    (
        "src/network/OtaUpdater.cpp",
        r"CROSSPOINT_RELEASE_ASSET",
        "The updater goes back to upstream's per-board asset name. Every x4pro\n"
        "in the field since v1.0.0 asks for the literal firmware.bin, so they\n"
        "would stop finding any update at all.",
    ),
]


REF = "crosspoint/develop"


# Whether the ref is here at all, asked once and kept apart from whether it
# holds a given file. Conflating the two is what this suite used to do: a clone
# with no crosspoint remote failed every cat-file below, and both entries were
# then reported as "not upstream-owned, so this entry guards nothing" -- an
# accusation that the registry had rotted, on a tree where nothing had. That is
# every web session, on every commit. crossplay-ci.yml fetches the ref in a step
# of its own and .claude/hooks/session-start.sh now does the same, but a clone
# with neither must skip the ownership question rather than answer it wrongly.
HAVE_REF = (
    subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", REF],
        cwd=root,
        capture_output=True,
    ).returncode
    == 0
)


def upstream_has(path):
    return (
        subprocess.run(
            ["git", "cat-file", "-e", f"{REF}:{path}"],
            cwd=root,
            capture_output=True,
        ).returncode
        == 0
    )


if not HAVE_REF:
    # In CI this is a failure, not a skip, for the reason docsclaims gives at
    # the same fork in the road: the workflow fetches the ref on purpose, and if
    # that step is dropped the suite must say so rather than pass with half of
    # itself switched off. Locally it is a loud skip -- the pattern checks need
    # no upstream at all, and they are the half that catches a sync having eaten
    # an override, which is what this suite is for.
    if os.environ.get("CI"):
        check(
            False,
            f"no {REF} ref in CI",
            "the 'Fetch upstream's tip' step in crossplay-ci.yml is gone, so no\n"
            "entry here can be checked for still pointing at a file upstream owns.",
        )
    else:
        print(
            f"  SKIP forkoverrides  no {REF} ref, so entries were NOT checked for "
            f"still pointing at upstream-owned files (git fetch crosspoint "
            f"develop). The patterns below still ran."
        )


registered = set()
for path, pattern, why in REGISTRY:
    registered.add(path)
    f = root / path
    if not f.exists():
        check(False, f"{path} is gone", why)
        continue
    # An entry on a file upstream does not own is pointing at nothing: the sync
    # hazard it guards cannot happen. Fail rather than carry a comforting entry.
    if HAVE_REF and not upstream_has(path):
        check(
            False,
            f"{path} is not upstream-owned, so this entry guards nothing",
            "Delete the entry, or correct the path. A registry that cannot rot\n"
            "is the only kind worth having.",
        )
        continue
    body = f.read_text(errors="replace")
    check(
        re.search(pattern, body) is not None,
        f"{path} no longer matches /{pattern}/",
        why + "\n\nA sync almost certainly took upstream's copy of this file.\n"
        "Restore the fork's change; do not adjust this pattern to match.",
    )

# PUBLISH WHAT IS NOT GUARDED. A clean list hides absence: the registry covers
# two files and the exposure is the whole modified set, so the size of the gap
# is printed rather than left to be discovered by the next crash.
if not HAVE_REF:
    # Measured against nothing, the gap reads as zero: the old code printed
    # "0 of 0 fork-modified upstream-owned files have NO entry here", which is
    # the same sentence a fully guarded tree prints and the most reassuring
    # possible way to say the measurement did not happen.
    print(
        f"  SKIP forkoverrides  no {REF} ref, so the exposure set could not be "
        f"measured. Registered: {len(registered)}."
    )
else:
    try:
        tracked = subprocess.run(
            ["git", "ls-files", "src", "lib"],
            cwd=root,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.split()
        modified = []
        for path in tracked:
            if not upstream_has(path):
                continue
            d = subprocess.run(
                ["git", "diff", "--quiet", REF, "--", path],
                cwd=root,
                capture_output=True,
            )
            if d.returncode == 1:
                modified.append(path)
        unguarded = [p for p in modified if p not in registered]
        print(
            f"  SKIP forkoverrides  {len(unguarded)} of {len(modified)} fork-modified "
            f"upstream-owned files have NO entry here, so a sync can revert them "
            f"silently and this suite will not notice. Registered: "
            f"{len(registered)}."
        )
    except subprocess.CalledProcessError:
        print("  SKIP forkoverrides  git ls-files failed, so the exposure set "
              "could not be measured")

# The phrase check.sh counts sub-suites by (it greps "checks, 0 failed").
# Printing anything else reports "ok (0 sub-suite(s))", which is exactly what
# a suite that asserts nothing prints -- card #334.
print(f"{checks} checks, {failed} failed")
sys.exit(1 if failed else 0)
PY
