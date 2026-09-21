#!/usr/bin/env bash
# Builds the user-facing release archive from the bundle:  scripts/release.sh 0.1.0
#
#   work/dist/MiSTer-GemRB-v<version>.zip   unpack onto the root of the MiSTer's SD card
#   work/dist/SHA256SUMS
#   work/dist/RELEASE_NOTES-v<version>.md   (only if docs/release-notes/<version>.md exists)
#
# The archive holds gemrb/ (engine, libraries, Python, launcher, license texts, source list) and Scripts/gemrb.sh. It
# contains no game data. Run build-deps.sh and build-gemrb.sh first; bundle.sh is run here to get a fresh bundle.
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/env.sh"

VERSION="${1:?usage: scripts/release.sh <version, e.g. 0.1.0>}"
NAME="MiSTer-GemRB-v$VERSION"
DIST="$WORK/dist"
STAGE="$DIST/stage/$NAME"
G="$SRC/gemrb"

# 1. Fresh bundle.
"$ROOT/scripts/bundle.sh" >/dev/null
rm -rf "$DIST/stage" "$DIST/$NAME.zip"
mkdir -p "$STAGE/gemrb" "$STAGE/Scripts"

# 2. The game: everything in the bundle. The config ships as a default that the launcher copies on first run, so an
#    update never overwrites the user's edits.
cp -a "$WORK/bundle/gemrb/." "$STAGE/gemrb/"
mv "$STAGE/gemrb/GemRB.cfg" "$STAGE/gemrb/GemRB.cfg.default"
cp -a "$WORK/bundle/Scripts/gemrb.sh" "$STAGE/Scripts/"
echo "$VERSION" > "$STAGE/gemrb/VERSION"
mkdir -p "$STAGE/gemrb/game"
cat > "$STAGE/gemrb/game/README.txt" <<'TXT'
Put your Baldur's Gate II game files here: the folder that contains CHITIN.KEY, dialog.tlk, data/, override/, music/,
sounds/ and scripts/. See ../INSTALL.txt.
TXT

# 3. License texts of everything that ships, plus an index.
L="$STAGE/gemrb/LICENSES"; mkdir -p "$L"
cp "$ROOT/LICENSE.txt" "$L/GPL-2.0.txt"
cp "$G/COPYING" "$L/GemRB-GPL-2.0.txt"
sed -n '1,/\*\//p' "$G/gemrb/includes/fmt/format.h" > "$L/fmt-MIT.txt"
grep -q "Permission is hereby granted" "$L/fmt-MIT.txt" || { echo "could not extract the fmt license" >&2; exit 1; }
cp /usr/share/common-licenses/LGPL-2.1 "$L/LGPL-2.1.txt"
cp /usr/share/common-licenses/GPL-3 "$L/GPL-3.0.txt"
cp "$ROOT/COPYING.ZLIB" "$L/SDL2-zlib.txt"
cp "$SRC/SDL2_mixer-$SDL2_MIXER_VER/LICENSE.txt" "$L/SDL2_mixer-zlib.txt"
cp "$SRC/zlib-$ZLIB_VER/LICENSE" "$L/zlib.txt"
cp "$SRC/libpng-$LIBPNG_VER/LICENSE" "$L/libpng.txt"
cp "$SRC/freetype-$FREETYPE_VER/docs/GPLv2.TXT" "$L/FreeType-GPL-2.0.txt"
cp "$SRC/freetype-$FREETYPE_VER/docs/FTL.TXT" "$L/FreeType-FTL.txt"
cp "$SRC/libogg-$LIBOGG_VER/COPYING" "$L/libogg-BSD.txt"
cp "$SRC/libvorbis-$LIBVORBIS_VER/COPYING" "$L/libvorbis-BSD.txt"
cp "$SRC/Python-$PYTHON_VER/LICENSE" "$L/Python-PSF.txt"
cp /usr/share/doc/libc6-armhf-cross/copyright "$L/glibc-copyright.txt"
cp /usr/share/doc/libstdc++6/copyright "$L/GCC-runtime-libstdc++-copyright.txt"   # includes the GCC Runtime Library Exception
grep -q "GCC RUNTIME LIBRARY EXCEPTION" "$L/GCC-runtime-libstdc++-copyright.txt" || { echo "runtime exception text missing" >&2; exit 1; }
cat > "$L/README.txt" <<'TXT'
Licenses of the software in this package. See ATTRIBUTIONS.md for what each component is used for.

  GPL-2.0.txt                            MiSTer-GemRB's own code (GPL-2.0-or-later); the whole package is GPL-2.0-or-later
  GemRB-GPL-2.0.txt                      GemRB (GPL-2.0-or-later)
  fmt-MIT.txt                            the {fmt} library bundled in GemRB
  LGPL-2.1.txt                           code derived from FFmpeg in GemRB's Bink player, and glibc
  SDL2-zlib.txt, SDL2_mixer-zlib.txt     SDL2 (with the MiSTer drivers and patches) and SDL2_mixer
  zlib.txt, libpng.txt                   zlib and libpng
  FreeType-GPL-2.0.txt, FreeType-FTL.txt FreeType (used under its GPL-2.0 option)
  libogg-BSD.txt, libvorbis-BSD.txt      libogg and libvorbis
  Python-PSF.txt                         the embedded Python and its standard library
  glibc-copyright.txt                    the C library shipped in libs/ (ld-linux-armhf.so.3, libc.so.6, libm.so.6, libs/gconv)
  GCC-runtime-libstdc++-copyright.txt,
  GPL-3.0.txt                            libstdc++ and libgcc_s in libs/ (GPL-3.0 with the GCC Runtime Library Exception)

Baldur's Gate II and other Infinity Engine game data is not included.
TXT
cp "$ROOT/README.md" "$ROOT/ATTRIBUTIONS.md" "$STAGE/gemrb/"
cp "$ROOT/LICENSE.txt" "$ROOT/COPYING.ZLIB" "$STAGE/gemrb/"

# 4. Where the corresponding source comes from, with checksums of the exact archives used.
DRIVER_REPO="$(git -C "$SDL_DRIVER_DIR" rev-parse --show-toplevel 2>/dev/null || true)"
{
	echo "MiSTer-GemRB v$VERSION: corresponding source"
	echo
	echo "This package's own source: https://github.com/aquasock/MiSTer-GemRB"
	if git -C "$ROOT" rev-parse HEAD >/dev/null 2>&1; then
		echo "  commit $(git -C "$ROOT" rev-parse HEAD)$(git -C "$ROOT" diff --quiet HEAD -- 2>/dev/null || echo ' (built with uncommitted changes)')"
	fi
	echo "  scripts/env.sh pins every version below; scripts/build-deps.sh and build-gemrb.sh fetch and build them."
	echo
	echo "SDL2 video and audio drivers: https://github.com/aquasock/MiSTer-VCMI (sdl-driver/)"
	[ -n "$DRIVER_REPO" ] && echo "  commit $(git -C "$DRIVER_REPO" rev-parse HEAD)$(git -C "$DRIVER_REPO" diff --quiet HEAD -- sdl-driver 2>/dev/null || echo ' (built with uncommitted changes)')"
	echo
	echo "GemRB $GEMRB_TAG: https://github.com/gemrb/gemrb (tag $GEMRB_TAG)"
	echo "  commit $(git -C "$G" rev-parse HEAD)"
	echo "  plus the patches in patches/ of this project"
	echo
	echo "Libraries (sha256 of the archives that were built):"
	for f in "SDL2-$SDL2_VER.tar.gz" "SDL2_mixer-$SDL2_MIXER_VER.tar.gz" "Python-$PYTHON_VER.tar.xz" "zlib-$ZLIB_VER.tar.gz" \
	         "libpng-$LIBPNG_VER.tar.gz" "freetype-$FREETYPE_VER.tar.xz" "libogg-$LIBOGG_VER.tar.xz" "libvorbis-$LIBVORBIS_VER.tar.xz"; do
		printf '  %s  %s\n' "$(sha256sum "$DL/$f" | cut -d' ' -f1)" "$f"
	done
	echo "  (SDL2 and SDL2_mixer: https://github.com/libsdl-org, with the sdl-driver patches and drivers and patches/sdl2/ applied;"
	echo "   Python: https://www.python.org/ftp/python; zlib: https://github.com/madler/zlib; libpng: https://github.com/pnggroup/libpng;"
	echo "   FreeType: https://freetype.org; libogg and libvorbis: https://xiph.org/downloads)"
	echo
	echo "C runtime in libs/: glibc $(dpkg-query -W -f='${Version}' libc6-armhf-cross), Ubuntu (source package glibc; the cross packages come"
	echo "  from source package cross-toolchain-base). libstdc++ and libgcc_s: GCC $(arm-linux-gnueabihf-gcc -dumpfullversion), Ubuntu"
	echo "  (source package gcc-15-cross)."
	echo
	echo "LGPL/GPL note: the C and C++ runtime libraries are dynamically linked and are replaceable: the programs start"
	echo "through libs/ld-linux-armhf.so.3 with libs/ on their library path. On request, the source for any GPL or LGPL"
	echo "component listed here can also be obtained from the project page above."
} > "$STAGE/gemrb/SOURCES.txt"

# 5. Instructions for people who only download the zip.
cat > "$STAGE/gemrb/INSTALL.txt" <<TXT
MiSTer-GemRB v$VERSION: installing

1. Copy the two folders from this zip onto the root of your MiSTer's SD card, merging with what is there:
       gemrb/    ->  /media/fat/gemrb
       Scripts/  ->  /media/fat/Scripts
   The location matters: the programs look for their libraries in /media/fat/gemrb.

2. Add your own Baldur's Gate II game files into /media/fat/gemrb/game: the folder that contains CHITIN.KEY,
   dialog.tlk, data/, override/, music/, sounds/ and scripts/ (about 2.6 GB). For the GOG release, unpack the
   installer with innoextract, https://constexpr.org/innoextract/ , and copy the contents of its "app" folder; the
   .exe and .dll files and the manuals are not needed. Only the GOG "Complete" release has been tested.

3. On the MiSTer press F12, choose Scripts, and run "gemrb".
   The screen blinks as the HDMI output switches to 800x600 (your display scales it to fill the screen).
   Quit from the game's own menu; the display is switched back to 1080p60.

The first run creates a 384 MB swap file (swapfile) on the SD card in the background, about 40 seconds. The MiSTer has
little RAM, and a big fight can use nearly all of it: the swap file is only used as a last resort, so that the game is
not killed. Set MISTER_SWAP_MB=0 in /media/fat/gemrb/env.sh to turn it off.

Needs: a MiSTer with a DE10-Nano, a USB mouse and keyboard, and an HDMI display that accepts 800x600 at 60 Hz.
GemRB's settings are in /media/fat/gemrb/GemRB.cfg (created from GemRB.cfg.default on the first run and never
overwritten); saves are in /media/fat/gemrb/save.

To remove: delete /media/fat/gemrb and /media/fat/Scripts/gemrb.sh.
More: README.md, and the license texts in LICENSES/ (see ATTRIBUTIONS.md).
TXT

# 6. Pack it, keeping the layout at the top level.
python3 - "$STAGE" "$DIST/$NAME.zip" <<'PY'
import os, sys, zipfile
stage, out = sys.argv[1:3]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk(stage):
        dirs.sort()
        for f in sorted(files):
            full = os.path.join(root, f)
            arc = os.path.relpath(full, stage)
            info = zipfile.ZipInfo.from_file(full, arc)
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(full, "rb") as fh:
                z.writestr(info, fh.read(), zipfile.ZIP_DEFLATED, 9)
PY
( cd "$DIST" && sha256sum "$NAME.zip" > SHA256SUMS )
[ -f "$ROOT/docs/release-notes/$VERSION.md" ] && cp "$ROOT/docs/release-notes/$VERSION.md" "$DIST/RELEASE_NOTES-v$VERSION.md"
echo "built $DIST/$NAME.zip ($(du -h "$DIST/$NAME.zip" | cut -f1))"
