#!/usr/bin/env bash
# Cross-builds GemRB (client, plugins, GUI scripts) for the MiSTer into work/gemrb-install.
# Run scripts/build-deps.sh first.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

G="$SRC/gemrb"
INSTALL_ROOT="${DEVICE_DIR:-/media/fat/gemrb}"     # where the bundle lives on the MiSTer
PYV="${PYTHON_VER%.*}"

[ -d "$G/.git" ] || git clone --depth 1 --branch "$GEMRB_TAG" https://github.com/gemrb/gemrb.git "$G"
# Patches to GemRB (idempotent).
for p in "$ROOT"/patches/*.patch; do
	[ -e "$p" ] || continue
	git -C "$G" apply --reverse --check "$p" 2>/dev/null || git -C "$G" apply "$p"
done

rm -rf "$WORK/gemrb-install"
cmake -S "$G" -B "$BUILD/gemrb" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_INSTALL_PREFIX="$INSTALL_ROOT" -DLAYOUT=home \
	-DSDL_BACKEND=SDL2 -DOPENGL_BACKEND=None -DSTATIC_LINK=OFF \
	-DUSE_OPENAL=OFF -DUSE_LIBVLC=OFF -DUSE_TESTS=OFF -DUSE_TRACY=OFF -DDISABLE_WERROR=ON \
	-DPython_INCLUDE_DIR="$PREFIX/include/python$PYV" -DPython_LIBRARY="$PREFIX/lib/libpython$PYV.so" \
	-DPython_ROOT_DIR="$PREFIX" \
	-DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,$PREFIX/lib" \
	>"$LOGS/gemrb-configure.log" 2>&1 || { tail -30 "$LOGS/gemrb-configure.log"; exit 1; }
cmake --build "$BUILD/gemrb" -j"$JOBS" >"$LOGS/gemrb-build.log" 2>&1 \
	|| { grep -n -E "error|Error" "$LOGS/gemrb-build.log" | head -20; exit 1; }
DESTDIR="$WORK/gemrb-install" cmake --install "$BUILD/gemrb" >"$LOGS/gemrb-install.log" 2>&1
find "$WORK/gemrb-install" -maxdepth 4 | head -30
