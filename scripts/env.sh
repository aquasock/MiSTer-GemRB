# Shared settings for the MiSTer-GemRB cross build. Source this file.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$ROOT/work"
DL="$WORK/dl"
SRC="$WORK/src"
BUILD="$WORK/build"
LOGS="$WORK/logs"
PREFIX="$WORK/prefix"              # armhf install prefix for all dependencies
HOSTPREFIX="$WORK/hostprefix"      # x86 Python, needed to cross-build the armhf one
STAMPS="$WORK/stamps"
CROSS=arm-linux-gnueabihf
SYSROOT=/usr/arm-linux-gnueabihf
TOOLCHAIN="$ROOT/scripts/toolchain.cmake"
JOBS="${JOBS:-$(nproc)}"

GEMRB_TAG=v0.9.5
PYTHON_VER=3.13.15
ZLIB_VER=1.3.1
LIBPNG_VER=1.6.44
FREETYPE_VER=2.13.3
LIBOGG_VER=1.3.5
LIBVORBIS_VER=1.3.7
SDL2_VER=2.32.10
SDL2_MIXER_VER=2.8.1
NOODLES_COMMIT=513f2182b3b0c156c0bd8644e06678cdef0b5f14
NOODLES_ARCHIVE_SHA256=cd8bf87cec9e7168364dab2dbd5fd3c0df43373c324a4dff368f982fa56b8814

ARCH_FLAGS="-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
# The MiSTer SDL2 video and audio drivers and their patches come from MiSTer-VCMI.
SDL_DRIVER_DIR="${SDL_DRIVER_DIR:-$ROOT/../MiSTer-VCMI/sdl-driver}"

export GEMRB_PREFIX="$PREFIX"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""
mkdir -p "$DL" "$SRC" "$BUILD" "$LOGS" "$PREFIX" "$STAMPS"
