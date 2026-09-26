#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

OUT="$WORK/frame-tracer"
mkdir -p "$OUT"

"$CROSS-gcc" $ARCH_FLAGS -O2 -g -fPIC -shared -Wall -Wextra -Werror \
	-Wl,-z,defs -o "$OUT/libgemrb-frame-trace.so" \
	"$ROOT/tools/noodles-frame-trace.c" -ldl

"$CROSS-readelf" -h "$OUT/libgemrb-frame-trace.so" | grep -E 'Class:|Machine:'
"$CROSS-readelf" -d "$OUT/libgemrb-frame-trace.so" | grep NEEDED
sha256sum "$OUT/libgemrb-frame-trace.so"

# Exercise constructor/destructor and frame interception with a native twin.
gcc -O2 -g -fPIC -shared -Wall -Wextra -Werror -Wl,-z,defs \
	-o "$OUT/libgemrb-frame-trace-host.so" "$ROOT/tools/noodles-frame-trace.c" -ldl
cat > "$OUT/fake-sdl.c" <<'EOF'
#include <stdint.h>
#include <unistd.h>
void SDL_Delay(uint32_t ms) { usleep((useconds_t) ms * 1000); }
void SDL_RenderPresent(void* renderer) { (void) renderer; usleep(1000); }
EOF
cat > "$OUT/frame-trace-selftest.c" <<'EOF'
#include <stdint.h>
void SDL_Delay(uint32_t);
void SDL_RenderPresent(void*);
int main(void)
{
	SDL_RenderPresent(0);
	SDL_Delay(55);
	SDL_RenderPresent(0);
	return 0;
}
EOF
gcc -O2 -fPIC -shared -Wall -Wextra -Werror -o "$OUT/libfake-sdl.so" "$OUT/fake-sdl.c"
gcc -O2 -Wall -Wextra -Werror -o "$OUT/frame-trace-selftest" "$OUT/frame-trace-selftest.c" \
	-L"$OUT" -lfake-sdl -Wl,-rpath,'$ORIGIN'
rm -f "$OUT/selftest.log"
GEMRB_FRAME_TRACE_LOG="$OUT/selftest.log" LD_PRELOAD="$OUT/libgemrb-frame-trace-host.so" \
	"$OUT/frame-trace-selftest"
grep -q '^TRACE start ' "$OUT/selftest.log"
grep -q '^FRAME ' "$OUT/selftest.log"
grep -q '^TRACE stop ' "$OUT/selftest.log"
echo "frame tracer self-test passed"
