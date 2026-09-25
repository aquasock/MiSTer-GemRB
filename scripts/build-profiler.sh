#!/usr/bin/env bash
# Builds the standalone, non-stopping MiSTer ARM instruction sampler.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

OUT="$WORK/profiler"
mkdir -p "$OUT"

"$CROSS-gcc" -O2 -Wall -Wextra -static $ARCH_FLAGS \
	"$ROOT/tools/noodles-perf-sampler.c" -o "$OUT/noodles-perf-sampler"

"$CROSS-readelf" -h "$OUT/noodles-perf-sampler" | grep -E 'Class:|Machine:'
sha256sum "$OUT/noodles-perf-sampler"
