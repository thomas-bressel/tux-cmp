#!/usr/bin/env bash
# Runs TuxCMP's loaders in TuxAPE's CPC (see cpccheck.cpp). It needs TuxAPE
# built in ../TuxAPE/build (or TUXAPE_DIR), with its ROMs, and TuxCMP built
# in build/.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tuxape="${TUXAPE_DIR:-$root/../TuxAPE}"
core="$tuxape/build/src/core/libtuxape_core.a"
[ -f "$core" ] || { echo "Build TuxAPE first: $core is missing." >&2; exit 1; }
cmake --build "$root/build" -j"$(nproc)" --target tuxcmp_cpc >/dev/null

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
g++ -std=c++20 -O2 -I"$tuxape/src" -I"$root/src" "$root/tools/cpc-check/cpccheck.cpp" \
    "$root/build/src/cpc/libtuxcmp_cpc.a" "$root/build/src/zx0/libtuxcmp_zx0.a" "$root/build/src/base/libtuxcmp_base.a" \
    "$core" -o "$work/cpccheck"
"$work/cpccheck" "$root/tests/golden"
