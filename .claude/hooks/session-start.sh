#!/bin/bash
#
# Sessions run in fresh, ephemeral containers, so everything a clone normally
# does once has to happen every time. Without this the first build fails on
# missing freeink-sdk headers, the first commit is rejected by a clang-format
# version check, and neither failure names its real cause.
#
# Ordered cheapest-and-most-essential first, so a session that dies partway
# still has a working tree and working hooks. Failures warn rather than abort:
# a slow mirror should cost a capability, not the session.
set -uo pipefail

cd "${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel)}" || exit 0

# Local clones are set up already and carry their own toolchains; only the
# throwaway containers need any of this.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

failed=()
step() {
  local name="$1"; shift
  if "$@" >/tmp/crossplay-setup.log 2>&1; then
    echo "  ok    $name"
  else
    echo "  WARN  $name"
    tail -4 /tmp/crossplay-setup.log | sed 's/^/        /'
    failed+=("$name")
  fi
}

echo "crossplay session setup"

# freeink-sdk is the platform layer every build compiles against, and it tracks
# a branch rather than a default, so take the commit the superproject records.
step "submodules" git submodule update --init --recursive

# pre-commit formats, pre-push guards the release train. Never set by default.
step "git hooks" git config core.hooksPath .githooks
chmod +x .githooks/* 2>/dev/null || true

# The image ships stale package lists, and every apt install 404s without this.
step "apt lists" bash -c "$SUDO apt-get update -qq"

# clang-format 21 exactly: .clang-format uses keys that 18 rejects, and the
# pre-commit hook refuses to run against an older one. Add the LLVM repository
# by hand rather than with apt.llvm.org's llvm.sh, which shells out to
# add-apt-repository and dies here on a python3-apt that does not match the
# interpreter.
if [ ! -x /usr/lib/llvm-21/bin/clang-format ]; then
  step "llvm 21 repo" bash -c "
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key |
      $SUDO gpg --dearmor -o /usr/share/keyrings/llvm-archive.gpg &&
    echo 'deb [signed-by=/usr/share/keyrings/llvm-archive.gpg] https://apt.llvm.org/noble/ llvm-toolchain-noble-21 main' |
      $SUDO tee /etc/apt/sources.list.d/llvm21.list >/dev/null &&
    $SUDO apt-get update -qq"
  step "clang-format-21" bash -c "$SUDO apt-get install -y -qq clang-format-21"
else
  echo "  ok    clang-format-21 (present)"
fi
# bin/clang-format-fix looks for the binary on PATH, and /usr/bin holds 18.
if [ -d /usr/lib/llvm-21/bin ] && [ -n "${CLAUDE_ENV_FILE:-}" ]; then
  echo 'export PATH="/usr/lib/llvm-21/bin:$PATH"' >> "$CLAUDE_ENV_FILE"
fi

# The simulator links SDL2, and MD5Builder wraps OpenSSL on Linux.
step "simulator libs" bash -c "$SUDO apt-get install -y -qq libsdl2-dev libssl-dev"

# PlatformIO here is the pioarduino fork CI pins, not upstream platformio: the
# distribution is named pioarduino-core, and it installs from git because the
# GitHub archive URL CI uses is refused through this network.
if ! command -v pio >/dev/null; then
  step "platformio" uv pip install --system -q \
    "pioarduino-core @ git+https://github.com/pioarduino/platformio-core@v6.1.19"
else
  echo "  ok    platformio (present)"
fi

# Several host suites, plus the icon and font tooling, are Python.
step "python deps" uv pip install --system -q -r requirements.txt

if [ ${#failed[@]} -eq 0 ]; then
  echo "ready. gate: ./scripts_local/check.sh --tests"
else
  echo "ready, but these need a look: ${failed[*]}"
fi
exit 0
