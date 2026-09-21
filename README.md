# MiSTer-GemRB

[GemRB](https://gemrb.org), an open-source reimplementation of the Infinity Engine that powered Baldur's Gate,
Icewind Dale and Planescape: Torment, running on the ARM (HPS) side of a
[MiSTer](https://github.com/MiSTer-devel) (DE10-Nano, dual Cortex-A9). The game runs as an ordinary Linux program on
the stock MiSTer system: nothing is installed outside `/media/fat`, and the FPGA only shows the picture and plays the
sound. This project supplies the missing pieces: a cross-build for the MiSTer's old userland (including an embedded
Python), the MiSTer's SDL2 video and audio drivers, fixes and speed-ups for software rendering on this CPU, and a
launcher that fills the screen. It is software, not an FPGA core, and sits beside
[MiSTer-VCMI](https://github.com/aquasock/MiSTer-VCMI), [MiSTer-DCSS](https://github.com/aquasock/MiSTer-DCSS),
[MiSTer-Raster](https://github.com/aquasock/MiSTer-Raster) and
[MiSTer-Phosphor](https://github.com/aquasock/MiSTer-Phosphor).

You supply your own copy of the game data. This project does not include it. Baldur's Gate II (the GOG "Complete"
release, Shadows of Amn and Throne of Bhaal) is what was tested.

## What it does

- **GemRB 0.9.5, cross-built for the Cortex-A9** — the engine and all its plugins, built on a PC with the Ubuntu ARM
  cross toolchain, with an embedded Python 3.13 for the game's interface scripts. It ships its own glibc and loader,
  so it runs on the MiSTer's much older root filesystem without changing it.
- **SDL2 `mister` video and audio drivers** — shared with MiSTer-VCMI. Video goes through `/dev/fb0` with a presenter
  thread; audio goes to the `/dev/MrAudio` ring buffer the FPGA plays out. Mouse and keyboard are read from the Linux
  input devices, with USB hotplug.
- **Software-rendering speed-ups in SDL** — the game is drawn by SDL's software renderer, which is slow on this CPU.
  The bundled SDL enables its NEON alpha blitter (restricted to destinations without alpha, where it is correct),
  fills translucent rectangles with NEON, and draws gradient triangles without per-pixel 64-bit divisions. See
  [Performance](#performance).
- **Fixes to GemRB's SDL plugins** — a lock-order deadlock in the audio plugin that froze the game while walking, a
  music conversion bug that played every other chunk as noise when the sound device does not run at the music's
  sample rate, and a fallback for SDL's software renderer, which lacks the custom blend modes GemRB's wall-occlusion
  stencil needs (without it characters were drawn as solid purple rectangles).
- **A launcher for the OSD Scripts menu** — switches the HDMI output to 800x600 so your display scales the picture to
  fill the screen, sets up an emergency swap file (see below), runs the game, and restores your mode afterwards.
- **The game's own videos** — GemRB includes its own Bink and MVE decoders, so no FFmpeg is needed.

## Performance

Measured on a DE10-Nano MiSTer at 800x600. The figures depend on the scene, so treat them as a guide.

| Situation | Result |
| --- | --- |
| Main menu | about 29 fps (the game caps at 30) |
| In game, busy forest scene, first build | 8 to 12 fps, depending on the scene |
| In game, after the fog-of-war and blending changes | about 20 fps in the same scenes; later speed-ups felt smoother but were not measured |
| Videos | play, but stutter: the CPU cannot keep up |
| Memory | about 200 MB after loading a game, up to about 380 MB in a large fight |

What made the difference, in order: drawing the fog of war with sprites instead of blended triangles
(`SpriteFogOfWar=1`), the NEON alpha blit and fill, and the triangle-fill change.

## Requirements

- A MiSTer with a DE10-Nano (Cyclone V SoC), running the Menu core, with a MiSTer Linux image that has the `MiSTer_fb`
  and `MrAudio` devices. Tested on the Buildroot image with Linux 6.18.38.
- A USB mouse and keyboard, and an HDMI display that accepts 800x600 at 60 Hz.
- Baldur's Gate II data files, about 2.6 GB, plus about 400 MB for the swap file.
- To build: a Linux PC (Ubuntu 26.04 was used), about 2 GB of free disk space, a network connection for fetching
  sources, and a checkout of [MiSTer-VCMI](https://github.com/aquasock/MiSTer-VCMI) next to this repository: the SDL2
  driver sources are shared, not copied.

## Installation

### From a release

1. Download `MiSTer-GemRB-v<version>.zip` from the [Releases](https://github.com/aquasock/MiSTer-GemRB/releases) page
   and unzip it onto the root of the MiSTer's SD card, merging with what is there. This creates `/media/fat/gemrb`
   and `/media/fat/Scripts/gemrb.sh`. The location matters: the programs find their libraries under `/media/fat/gemrb`.
2. Copy your game files into `/media/fat/gemrb/game`: the folder that contains `CHITIN.KEY`, `dialog.tlk`, `data/`,
   `override/`, `music/`, `sounds/` and `scripts/`. For the GOG release, unpack the installer with
   [innoextract](https://constexpr.org/innoextract/) and copy the contents of its `app` folder. The installer's
   programs (`.exe`, `.dll`), manuals and helpers are not needed.
3. On the MiSTer, open the OSD (F12), choose **Scripts**, and run **gemrb**. The screen blinks as the output switches
   to 800x600. Quit from the game's own menu; the launcher switches your display back.

The zip also holds `INSTALL.txt`, the license texts in `LICENSES/`, and `SOURCES.txt` listing the exact source versions
and checksums.

### From source

1. Build the bundle (see [Building](#building)).
2. Copy it, and the game data, to the MiSTer over SSH. `scripts/deploy.sh` logs in as `root` with the stock MiSTer
   password; edit it if yours differs.

   ```sh
   MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh code
   BG2_DATA=<extracted game folder> MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh data
   ```

3. Launch it from the OSD as above.

The SD card is mounted synchronously, so copying the game data takes a long time: about half an hour for 2.6 GB.

To remove it, delete `/media/fat/gemrb` and `/media/fat/Scripts/gemrb.sh`.

## Configuration

Local settings go in `/media/fat/gemrb/env.sh`, which the launcher reads if it exists. Everything works without it.

| Setting | Meaning |
| --- | --- |
| `MISTER_OUTPUT_MODE=800x600` | The default: switch the HDMI output to 800x600 while playing |
| `MISTER_OUTPUT_MODE=off` | Do not change the HDMI mode |
| `MISTER_RESTORE_MODE="<modeline>"` | Mode to switch back to afterwards. Default is 1080p60 |
| `MISTER_SWAP_MB=384` | Size of the emergency swap file in MB; `0` turns it off |

GemRB's own settings are in `/media/fat/gemrb/GemRB.cfg`, which the launcher creates from `GemRB.cfg.default` on the
first run and never overwrites, so updating keeps your edits. The defaults are 800x600, the SDL audio driver, a 30 fps cap, intro videos skipped and sprite fog of war. The
options are described in GemRB's documentation. To try another game, change `GameType` and `GamePath`; only Baldur's
Gate II has been tested.

The drivers read the `SDL_MISTER_*` environment variables described in the
[MiSTer-VCMI README](https://github.com/aquasock/MiSTer-VCMI#configuration); in `env.sh` they need `export`. The launcher
sets `SDL_MISTER_FORMAT=xrgb` (an alpha-less screen, which lets SDL use its fast blitter) and
`SDL_RENDER_DRIVER=software`.

### The emergency swap file

The MiSTer has about 490 MB of RAM and no swap. A big fight (all the creature animations) plus the rest of the game can
need nearly all of it, and when memory ran out the kernel killed the game. So the launcher creates a 384 MB swap file
on the SD card (in the background the first time, which takes about 40 seconds), turns it on while the game runs, and
turns it off when it exits. The kernel is told to use it only as a last resort, so normal play does not write to the
card. In testing, allocating 160 MB beyond free memory put 131 MB into swap with no kills.

## Current limitations

- **Only Baldur's Gate II has been tested.** Other games GemRB supports, the Enhanced Editions, mods and multiplayer
  have not.
- **The frame rate is around 20 fps in busy scenes**, and videos stutter. Both are limits of the CPU.
- **Characters are drawn in front of walls and trees.** GemRB hides characters behind scenery with a stencil that needs
  blend modes SDL's software renderer does not have, so it is skipped. Some glow effects use plain additive blending.
- **The fog of war has the original game's blocky style**, because sprites are much cheaper here than blended geometry.
- **Intro videos are skipped** by default (they played choppily).
- **Your original HDMI mode is not saved.** The MiSTer here has no `MiSTer.ini`, so the launcher restores 1080p60. Set
  `MISTER_RESTORE_MODE` if your display uses something else.
- **GemRB ignores `SIGTERM`.** Quit from the game menu, or use `kill -KILL`; the launcher restores the display either
  way.
- You cannot save during combat: that is the game's rule.

## Building

Install the prerequisites on an Ubuntu (or Debian) PC:

```sh
sudo apt install gcc g++ make gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf libc6-dev-armhf-cross \
    cmake ninja-build pkg-config patchelf patch qemu-user-binfmt git curl python3 dpkg innoextract
```

Then, from the repository root, with MiSTer-VCMI checked out beside it (or `SDL_DRIVER_DIR` pointing at its
`sdl-driver` directory):

```sh
scripts/build-deps.sh    # zlib, libpng, FreeType, Ogg/Vorbis, SDL2 with the MiSTer drivers, SDL2_mixer, Python
scripts/build-gemrb.sh   # fetches GemRB 0.9.5, applies patches/, builds the engine and plugins
scripts/bundle.sh        # assembles work/bundle: binaries, libraries, glibc, Python, launcher
MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh all
```

Everything is created under `work/`, which is not tracked, and the pinned versions are in `scripts/env.sh`. Each
dependency step leaves a stamp, so a rerun skips finished work; changing the SDL drivers or patches rebuilds SDL
automatically.

`scripts/release.sh <version>` builds the release zip (`work/dist/`) from a fresh bundle: it adds every license text and
the source list, and writes `SHA256SUMS`. Release notes live in `docs/release-notes/`. Run it from a committed tree, so
that `SOURCES.txt` records the commit.

## Source layout

- `scripts/` — `env.sh` (versions and paths), `toolchain.cmake`, `enable-asm.cmake`, `build-deps.sh`, `build-gemrb.sh`,
  `bundle.sh` (also generates the launcher and the default `GemRB.cfg`), `deploy.sh` and `release.sh`.
- `patches/0001-*`, `patches/0002-*` — the changes to GemRB, applied by `build-gemrb.sh`.
- `patches/sdl2/` — the changes to SDL, applied by `build-deps.sh`.
- `work/` — created by the scripts: downloads, sources, build trees, the install prefix and the bundle. Not tracked.

## Documentation

- [Release notes](docs/release-notes/0.1.0.md)
- [Source attributions](ATTRIBUTIONS.md)
- [GemRB documentation](https://gemrb.org)
- [MiSTer-VCMI](https://github.com/aquasock/MiSTer-VCMI), which supplies the SDL2 drivers

## License

Original project code is licensed GPL-2.0-or-later (see `LICENSE.txt`), the same baseline as GemRB, MiSTer-VCMI,
MiSTer-DCSS, MiSTer-Raster and MiSTer-Phosphor. The complete bundle is distributed under the same terms. GemRB, Python,
SDL, FreeType, glibc and the other libraries keep their own licenses; see [ATTRIBUTIONS.md](ATTRIBUTIONS.md) for the
full inventory and the redistribution checklist.
