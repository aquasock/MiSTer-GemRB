#!/usr/bin/env bash
# Collects the unstripped GemRB and SDL binaries into work/debugfiles/.build-id/xx/yyyy.debug, the layout gdb uses to find
# separate debug files. `scripts/deploy.sh debug` copies it to the MiSTer; the launcher's MISTER_DEBUG=1 mode then runs the
# game under gdb and writes a readable call stack to crash.log if it crashes. Developer use only; it is not in the release.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

OUT="$WORK/debugfiles/.build-id"
rm -rf "$WORK/debugfiles"; mkdir -p "$OUT"
n=0
while IFS= read -r f; do
	id=$(arm-linux-gnueabihf-readelf -n "$f" 2>/dev/null | sed -n 's/.*Build ID: \([0-9a-f]*\).*/\1/p' | head -1)
	[ -n "$id" ] || continue
	mkdir -p "$OUT/${id:0:2}"
	cp "$f" "$OUT/${id:0:2}/${id:2}.debug"
	n=$((n + 1))
done < <(find "$BUILD/gemrb" -type f \( -name gemrb -o -name '*.so' -o -name 'libgemrb_core.so*' \) ! -path '*/CMakeFiles/*'; \
         ls "$PREFIX"/lib/libSDL2-2.0.so.0.* "$PREFIX"/lib/libSDL2_mixer-2.0.so.0.* 2>/dev/null)
echo "$n unstripped files in $WORK/debugfiles ($(du -sh "$WORK/debugfiles" | cut -f1))"
