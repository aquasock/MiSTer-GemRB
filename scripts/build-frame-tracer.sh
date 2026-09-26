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
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
typedef void (*delay_fn)(uint32_t);
typedef void (*present_fn)(void*);
int main(int argc, char** argv)
{
	if (argc != 2 || !dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL)) return 1;
	present_fn present = (present_fn) dlsym(RTLD_DEFAULT, "SDL_RenderPresent");
	delay_fn delay = (delay_fn) dlsym(RTLD_DEFAULT, "SDL_Delay");
	if (!present || !delay) return 2;
	present(0);
	delay(55);
	present(0);
	return 0;
}
EOF
gcc -O2 -fPIC -shared -Wall -Wextra -Werror -o "$OUT/libfake-sdl.so" "$OUT/fake-sdl.c"
gcc -O2 -Wall -Wextra -Werror -o "$OUT/frame-trace-selftest" "$OUT/frame-trace-selftest.c" -ldl
rm -f "$OUT/selftest.log"
GEMRB_FRAME_TRACE_LOG="$OUT/selftest.log" LD_PRELOAD="$OUT/libgemrb-frame-trace-host.so" \
	"$OUT/frame-trace-selftest" "$OUT/libfake-sdl.so"
grep -q '^TRACE start ' "$OUT/selftest.log"
grep -q '^FRAME ' "$OUT/selftest.log"
grep -q '^TRACE stop ' "$OUT/selftest.log"
echo "frame tracer self-test passed"
