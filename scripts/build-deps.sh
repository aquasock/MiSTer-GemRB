#!/usr/bin/env bash
# Cross-builds GemRB's dependencies for the MiSTer into work/prefix.
# Usage: scripts/build-deps.sh [step...]   (default: all steps, skipping finished ones)
# Steps: zlib libpng freetype ogg vorbis sdl2 sdl2_mixer python
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

say() { printf '\n=== %s ===\n' "$*"; }
fetch() {  # fetch <url> <file>
	[ -s "$DL/$2" ] || curl -fsSL --retry 3 --connect-timeout 20 --max-time 600 -o "$DL/$2" "$1"
}
extract() {  # extract <archive> <expected top dir>
	[ -d "$SRC/$2" ] || tar xf "$DL/$1" -C "$SRC"
}

# cm <name> <source dir> [cmake args...]: configure, build, install with a log.
cm() {
	local name=$1 src=$2; shift 2
	[ -e "$STAMPS/$name" ] && { echo "$name: already built"; return; }
	say "$name"
	(
		cmake -S "$src" -B "$BUILD/$name" -G Ninja \
			-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
			-DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
			-DBUILD_SHARED_LIBS=OFF "$@"
		cmake --build "$BUILD/$name" -j"$JOBS"
		cmake --install "$BUILD/$name"
	) >"$LOGS/$name.log" 2>&1 || { echo "$name FAILED, see $LOGS/$name.log"; tail -25 "$LOGS/$name.log"; exit 1; }
	touch "$STAMPS/$name"
}

step_zlib() {
	fetch "https://github.com/madler/zlib/releases/download/v$ZLIB_VER/zlib-$ZLIB_VER.tar.gz" "zlib-$ZLIB_VER.tar.gz"
	extract "zlib-$ZLIB_VER.tar.gz" "zlib-$ZLIB_VER"
	# Shared: GemRB's plugins are separate modules, and it ignores static libraries unless built with STATIC_LINK.
	cm zlib "$SRC/zlib-$ZLIB_VER" -DBUILD_SHARED_LIBS=ON -DZLIB_BUILD_EXAMPLES=OFF
}

step_libpng() {
	fetch "https://github.com/pnggroup/libpng/archive/refs/tags/v$LIBPNG_VER.tar.gz" "libpng-$LIBPNG_VER.tar.gz"
	extract "libpng-$LIBPNG_VER.tar.gz" "libpng-$LIBPNG_VER"
	cm libpng "$SRC/libpng-$LIBPNG_VER" -DBUILD_SHARED_LIBS=ON -DPNG_SHARED=ON -DPNG_STATIC=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF \
		-DPNG_ARM_NEON=off -DZLIB_ROOT="$PREFIX"
}

step_freetype() {
	fetch "https://sourceforge.net/projects/freetype/files/freetype2/$FREETYPE_VER/freetype-$FREETYPE_VER.tar.xz/download" "freetype-$FREETYPE_VER.tar.xz"
	extract "freetype-$FREETYPE_VER.tar.xz" "freetype-$FREETYPE_VER"
	cm freetype "$SRC/freetype-$FREETYPE_VER" -DBUILD_SHARED_LIBS=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_HARFBUZZ=ON \
		-DFT_DISABLE_PNG=ON -DFT_REQUIRE_ZLIB=ON -DZLIB_ROOT="$PREFIX"
}

step_ogg() {
	fetch "https://downloads.xiph.org/releases/ogg/libogg-$LIBOGG_VER.tar.xz" "libogg-$LIBOGG_VER.tar.xz"
	extract "libogg-$LIBOGG_VER.tar.xz" "libogg-$LIBOGG_VER"
	cm ogg "$SRC/libogg-$LIBOGG_VER" -DBUILD_SHARED_LIBS=ON -DINSTALL_DOCS=OFF
}

step_vorbis() {
	fetch "https://downloads.xiph.org/releases/vorbis/libvorbis-$LIBVORBIS_VER.tar.xz" "libvorbis-$LIBVORBIS_VER.tar.xz"
	extract "libvorbis-$LIBVORBIS_VER.tar.xz" "libvorbis-$LIBVORBIS_VER"
	cm vorbis "$SRC/libvorbis-$LIBVORBIS_VER" -DBUILD_SHARED_LIBS=ON -DOGG_ROOT="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
}

step_noodles() {
	local archive="MiSTer-Noodles-$NOODLES_COMMIT.tar.gz"
	local source="MiSTer-Noodles-$NOODLES_COMMIT"
	local stamp="$STAMPS/noodles.$NOODLES_COMMIT"
	fetch "https://github.com/aquasock/MiSTer-Noodles/archive/$NOODLES_COMMIT.tar.gz" "$archive"
	printf '%s  %s\n' "$NOODLES_ARCHIVE_SHA256" "$DL/$archive" | sha256sum -c -
	extract "$archive" "$source"
	[ -e "$stamp" ] && { echo "noodles: already built"; return; }
	say "noodles SDK"
	(
		make -C "$SRC/$source" sdk CROSS="$CROSS-" \
			CFLAGS="-std=c99 -O2 -fPIC -Wall -Wextra -Wno-unused-parameter"
		make -C "$SRC/$source" install-sdk CROSS="$CROSS-" SDK_TARGET=arm PREFIX="$PREFIX"
	) >"$LOGS/noodles.log" 2>&1 || { echo "noodles FAILED, see $LOGS/noodles.log"; tail -25 "$LOGS/noodles.log"; exit 1; }
	touch "$stamp"
}

step_sdl2() {
	fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-$SDL2_VER.tar.gz" "SDL2-$SDL2_VER.tar.gz"
	extract "SDL2-$SDL2_VER.tar.gz" "SDL2-$SDL2_VER"
	local d="$SRC/SDL2-$SDL2_VER" dd="$SDL_DRIVER_DIR"
	[ -d "$dd/mister" ] || { echo "SDL driver sources not found in $dd (set SDL_DRIVER_DIR)"; exit 1; }
	# Register the MiSTer drivers in SDL (fresh tree when the hooks change), then add their sources.
	if ! grep -q SDL_MISTERAUDIO "$d/CMakeLists.txt"; then
		rm -rf "$d"; extract "SDL2-$SDL2_VER.tar.gz" "SDL2-$SDL2_VER"
		patch -s -d "$d" -p1 <"$dd/sdl2-mister-hooks.patch"
	fi
	grep -q SDL_VIDEO_RENDER_NOODLES "$d/CMakeLists.txt" || patch -s -d "$d" -p1 <"$ROOT/patches/sdl2/noodles-renderer.patch"
	grep -q "division-free resampler" "$d/src/audio/SDL_audiocvt.c" || patch -s -d "$d" -p1 <"$dd/sdl2-resampler.patch"
	# NEON alpha blitter only for destinations without alpha (see the patch).
	grep -q "opaque destination" "$d/src/video/SDL_blit_A.c" || patch -s -d "$d" -p1 <"$ROOT/patches/sdl2/neon-blit-opaque-dst.patch"
	# Triangle fill: no 64-bit division per pixel (see the patch).
	grep -q "triangle_div" "$d/src/render/software/SDL_triangle.c" || patch -s -d "$d" -p1 <"$ROOT/patches/sdl2/triangle-no-int64-divide.patch"
	# NEON version of the translucent rectangle fill (see the patch).
	grep -q "BlendFillRect_ARGB8888_BlendNEON" "$d/src/render/software/SDL_blendfillrect.c" || patch -s -d "$d" -p1 <"$ROOT/patches/sdl2/blendfillrect-neon.patch"
	mkdir -p "$d/src/video/mister" "$d/src/audio/mister" "$d/src/render/noodles"
	cp "$dd"/mister/* "$d/src/video/mister/"; cp "$dd"/mister-audio/* "$d/src/audio/mister/"
	cp "$ROOT"/sdl-renderer/noodles/* "$d/src/render/noodles/"
	local h; h=$({ printf '%s\n' "$NOODLES_COMMIT"; cat "$dd"/mister/* "$dd"/mister-audio/* "$dd"/*.patch "$ROOT/patches/sdl2/neon-blit-opaque-dst.patch" "$ROOT/patches/sdl2/triangle-no-int64-divide.patch" "$ROOT/patches/sdl2/blendfillrect-neon.patch" "$ROOT/patches/sdl2/noodles-renderer.patch" "$ROOT"/sdl-renderer/noodles/*; } | md5sum | cut -d' ' -f1)
	[ "$(cat "$STAMPS/sdl2.driver" 2>/dev/null)" = "$h" ] || { rm -f "$STAMPS/sdl2"; echo "$h" >"$STAMPS/sdl2.driver"; }
	# Shared, so the drivers can be updated without relinking GemRB. Other audio backends stay off.
	cm sdl2 "$d" -DSDL_MISTER=ON -DSDL_MISTERAUDIO=ON -DSDL_RENDER_NOODLES=ON -DNOODLES_ROOT="$PREFIX" -DSDL_ARMNEON=ON -DARMNEON_FOUND=1 -DCMAKE_PROJECT_SDL2_INCLUDE="$ROOT/scripts/enable-asm.cmake" \
		-DCMAKE_ASM_COMPILER=$CROSS-gcc \
		-DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_TEST=OFF \
		-DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_VULKAN=OFF -DSDL_OPENGL=OFF \
		-DSDL_OPENGLES=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF -DSDL_OFFSCREEN=ON \
		-DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF \
		-DSDL_OSS=OFF -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF \
		-DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_LIBUDEV=OFF -DSDL_HIDAPI=OFF -DSDL_HIDAPI_LIBUSB=OFF \
		-DSDL_LIBSAMPLERATE=OFF -DSDL_CCACHE=OFF
}

step_sdl2_mixer() {
	fetch "https://github.com/libsdl-org/SDL_mixer/releases/download/release-$SDL2_MIXER_VER/SDL2_mixer-$SDL2_MIXER_VER.tar.gz" "SDL2_mixer-$SDL2_MIXER_VER.tar.gz"
	extract "SDL2_mixer-$SDL2_MIXER_VER.tar.gz" "SDL2_mixer-$SDL2_MIXER_VER"
	# GemRB decodes its own audio formats and only hands PCM to SDL_mixer, so no codecs are needed.
	cm sdl2_mixer "$SRC/SDL2_mixer-$SDL2_MIXER_VER" -DBUILD_SHARED_LIBS=ON -DSDL2_DIR="$PREFIX/lib/cmake/SDL2" \
		-DSDL2MIXER_VENDORED=OFF -DSDL2MIXER_DEPS_SHARED=OFF \
		-DSDL2MIXER_MP3=OFF -DSDL2MIXER_VORBIS=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_MOD=OFF \
		-DSDL2MIXER_MIDI=OFF -DSDL2MIXER_OPUS=OFF -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_WAV=ON \
		-DSDL2MIXER_SAMPLES=OFF -DSDL2MIXER_CMD=OFF
}

# Python is embedded by GemRB's GUIScript plugin. Cross-building it needs a native Python of the
# same version first.
step_python() {
	[ -e "$STAMPS/python" ] && { echo "python: already built"; return; }
	fetch "https://www.python.org/ftp/python/$PYTHON_VER/Python-$PYTHON_VER.tar.xz" "Python-$PYTHON_VER.tar.xz"
	extract "Python-$PYTHON_VER.tar.xz" "Python-$PYTHON_VER"
	local s="$SRC/Python-$PYTHON_VER" pyv=${PYTHON_VER%.*}
	if [ ! -e "$STAMPS/python-host" ]; then
		say "python (native, for the cross build)"
		( mkdir -p "$BUILD/python-host" && cd "$BUILD/python-host" \
			&& "$s/configure" --prefix="$HOSTPREFIX" --without-ensurepip \
			&& make -j"$JOBS" && make install ) >"$LOGS/python-host.log" 2>&1 \
			|| { echo "native python FAILED, see $LOGS/python-host.log"; tail -25 "$LOGS/python-host.log"; exit 1; }
		touch "$STAMPS/python-host"
	fi
	say "python (armhf)"
	( mkdir -p "$BUILD/python" && cd "$BUILD/python" \
		&& CC="$CROSS-gcc" CXX="$CROSS-g++" AR="$CROSS-ar" RANLIB="$CROSS-ranlib" READELF="$CROSS-readelf" \
		   CFLAGS="-O2 $ARCH_FLAGS -I$PREFIX/include" CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" \
		   "$s/configure" --host=$CROSS --build="$(gcc -dumpmachine)" --prefix="$PREFIX" \
			--with-build-python="$HOSTPREFIX/bin/python$pyv" --enable-shared --without-ensurepip --disable-ipv6 \
			--without-doc-strings --disable-test-modules \
			ac_cv_file__dev_ptmx=yes ac_cv_file__dev_ptc=no \
		&& make -j"$JOBS" && make install ) >"$LOGS/python.log" 2>&1 \
		|| { echo "python FAILED, see $LOGS/python.log"; tail -25 "$LOGS/python.log"; exit 1; }
	touch "$STAMPS/python"
}

ALL=(zlib libpng freetype ogg vorbis noodles sdl2 sdl2_mixer python)
for s in "${@:-${ALL[@]}}"; do "step_$s"; done
say "done: $PREFIX"
