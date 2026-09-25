#!/usr/bin/env bash
# Builds a self-contained MiSTer package for the SDL Noodles renderer diagnostic.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

OUT="$WORK/noodles-test"
[ -e "$STAMPS/sdl2" ] || { echo "SDL2 is not built; run scripts/build-deps.sh noodles sdl2"; exit 1; }
mkdir -p "$OUT/libs"

"$CROSS-gcc" -O2 -Wall -Wextra $ARCH_FLAGS \
	-I"$PREFIX/include/SDL2" -D_REENTRANT \
	"$ROOT/tools/noodles-render-test.c" -L"$PREFIX/lib" -lSDL2 -lm \
	-Wl,-rpath,'$ORIGIN/libs' -o "$OUT/noodles-render-test"
"$CROSS-gcc" -O2 -Wall -Wextra $ARCH_FLAGS -I"$PREFIX/include" \
	"$ROOT/tools/noodles-throughput.c" -L"$PREFIX/lib" -lnoodles \
	-Wl,-rpath,'$ORIGIN/libs' -o "$OUT/noodles-throughput"
"$CROSS-gcc" -O2 -Wall -Wextra $ARCH_FLAGS -I"$PREFIX/include" \
	"$ROOT/tools/noodles-contention.c" -L"$PREFIX/lib" -lnoodles -lpthread \
	-Wl,-rpath,'$ORIGIN/libs' -o "$OUT/noodles-contention"

cp -L "$PREFIX/lib/libSDL2-2.0.so.0" "$OUT/libs/libSDL2-2.0.so.0"
cp -L /usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 "$OUT/libs/ld-linux-armhf.so.3"
cp -L /usr/arm-linux-gnueabihf/lib/libc.so.6 "$OUT/libs/libc.so.6"
cp -L /usr/arm-linux-gnueabihf/lib/libm.so.6 "$OUT/libs/libm.so.6"

cat >"$OUT/run.sh" <<'EOF'
#!/bin/bash
set -e
D="$(cd "$(dirname "$0")" && pwd)"
export SDL_VIDEODRIVER=mister SDL_AUDIODRIVER=mister SDL_RENDER_DRIVER=noodles SDL_MISTER_FORMAT=xrgb
exec "$D/libs/ld-linux-armhf.so.3" --library-path "$D/libs" "$D/noodles-render-test" "$@"
EOF
cat >"$OUT/throughput.sh" <<'EOF2'
#!/bin/bash
set -e
D="$(cd "$(dirname "$0")" && pwd)"
exec "$D/libs/ld-linux-armhf.so.3" --library-path "$D/libs" "$D/noodles-throughput" "$@"
EOF2
cat >"$OUT/contention.sh" <<'EOF2'
#!/bin/bash
set -e
D="$(cd "$(dirname "$0")" && pwd)"
exec "$D/libs/ld-linux-armhf.so.3" --library-path "$D/libs" "$D/noodles-contention" "$@"
EOF2
chmod +x "$OUT/run.sh" "$OUT/throughput.sh" "$OUT/contention.sh" "$OUT/noodles-render-test" \
	"$OUT/noodles-throughput" "$OUT/noodles-contention"

"$CROSS-readelf" -d "$OUT/noodles-render-test" | grep NEEDED
sha256sum "$OUT/noodles-render-test" "$OUT/noodles-throughput" "$OUT/noodles-contention" \
	"$OUT/libs/libSDL2-2.0.so.0"
echo "built: $OUT"
