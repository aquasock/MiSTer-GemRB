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
int SDL_SetRenderTarget(void* renderer, void* texture) { (void) renderer; (void) texture; usleep(1000); return 0; }
int SDL_UpdateTexture(void* texture, const void* rect, const void* pixels, int pitch) { (void) texture; (void) rect; (void) pixels; (void) pitch; usleep(5000); return 0; }
int SDL_LockTexture(void* texture, const void* rect, void** pixels, int* pitch) { static int pixel; (void) texture; (void) rect; *pixels = &pixel; *pitch = 4; usleep(1000); return 0; }
void SDL_UnlockTexture(void* texture) { (void) texture; usleep(1000); }
EOF
cat > "$OUT/frame-trace-selftest.c" <<'EOF'
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
typedef void (*delay_fn)(uint32_t);
typedef void (*present_fn)(void*);
typedef int (*target_fn)(void*, void*);
typedef int (*update_fn)(void*, const void*, const void*, int);
typedef int (*lock_fn)(void*, const void*, void**, int*);
typedef void (*unlock_fn)(void*);
int main(int argc, char** argv)
{
	if (argc != 2 || !dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL)) return 1;
	present_fn present = (present_fn) dlsym(RTLD_DEFAULT, "SDL_RenderPresent");
	delay_fn delay = (delay_fn) dlsym(RTLD_DEFAULT, "SDL_Delay");
	target_fn target = (target_fn) dlsym(RTLD_DEFAULT, "SDL_SetRenderTarget");
	update_fn update = (update_fn) dlsym(RTLD_DEFAULT, "SDL_UpdateTexture");
	lock_fn lock = (lock_fn) dlsym(RTLD_DEFAULT, "SDL_LockTexture");
	unlock_fn unlock = (unlock_fn) dlsym(RTLD_DEFAULT, "SDL_UnlockTexture");
	if (!present || !delay || !target || !update || !lock || !unlock) return 2;
	present(0);
	delay(55);
	target(0, (void*) 1);
	update(0, 0, 0, 0);
	void* pixels = 0;
	int pitch = 0;
	lock(0, 0, &pixels, &pitch);
	unlock(0);
	target(0, 0);
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
grep -q ' engine_ms=' "$OUT/selftest.log"
grep -q ' update_phase_ms=' "$OUT/selftest.log"
grep -q ' draw_phase_ms=' "$OUT/selftest.log"
grep -q ' update_ms=' "$OUT/selftest.log"
grep -q '^SDL .* op=update ' "$OUT/selftest.log"
grep -q '^TRACE stop ' "$OUT/selftest.log"
echo "frame tracer self-test passed"
