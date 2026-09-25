# MiSTer-GemRB

[GemRB](https://gemrb.org), an open-source reimplementation of the Infinity Engine that powered Baldur's Gate,
Icewind Dale and Planescape: Torment, running on the ARM (HPS) side of a
[MiSTer](https://github.com/MiSTer-devel) (DE10-Nano, dual Cortex-A9). The game runs as an ordinary Linux program on
the stock MiSTer system: nothing is installed outside `/media/fat`, and the FPGA only shows the picture and plays the
sound. This project supplies the missing pieces: a cross-build for the MiSTer's old userland (including an embedded
Python), the MiSTer's SDL2 video and audio drivers, fixes and speed-ups for software rendering on this CPU, and a
launcher that fills the screen.

You supply your own copy of the game data. This project does not include it. Five classic games have launchers, all
tested with the GOG releases: Baldur's Gate, Baldur's Gate II (Shadows of Amn and Throne of Bhaal), Icewind Dale (with
Heart of Winter and Trials of the Luremaster), Icewind Dale II and Planescape: Torment. See
[Supported games](#supported-games).

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
- **Optional MiSTer-Noodles acceleration** — the parallel Noodles launchers keep textures and render targets in FPGA
  memory and accelerate sprite copies, modulation, SDL blend modes, opaque fills and blended rectangle fills. The
  renderer uses the protocol 1.5 descriptor ring so it can queue later sprite batches while earlier batches execute.
  When protocol 1.6 is loaded, it also combines up to 64 consecutive opaque fills into one ordered descriptor batch,
  releases redundant CPU shadows while texture contents are current in FPGA memory, retains a recreated shadow for
  textures that return to CPU updates, and falls back to SDL only for operations the core does not implement. On
  protocol 1.7 it uses a third display buffer, so the next frame renders while the previous flip waits for vertical
  blank. Protocol 1.4 remains compatible, with its original single in-flight descriptor table.
- **Fixes to GemRB's SDL plugins** — a lock-order deadlock in the audio plugin that froze the game while walking, a
  music conversion bug that played every other chunk as noise when the sound device does not run at the music's
  sample rate, and a fallback for SDL's software renderer, which lacks the custom blend modes GemRB's wall-occlusion
  stencil needs (without it characters were drawn as solid purple rectangles).
- **A launcher for the OSD Scripts menu, one per game** — switches the HDMI output to the game's resolution so your display scales the picture to
  fill the screen, prepares USB swap when the selected launch mode requires it, runs the game, and restores your mode afterwards.
- **The game's own videos** — GemRB includes its own Bink and MVE decoders, so no FFmpeg is needed.

## Performance

Measured on a DE10-Nano MiSTer at 800x600. The figures depend on the scene, so treat them as a guide.

| Situation | Result |
| --- | --- |
| Main menu | about 29 fps (the game caps at 30) |
| In game, busy forest scene, first build | 8 to 12 fps, depending on the scene |
| In game, after the fog-of-war and blending changes | about 20 fps in the same scenes; later speed-ups felt smoother but were not measured |
| Noodles protocol 1.4, Throne of Bhaal AR4000 combat | 13.93 fps settled, with 24.1–26.5 ms per frame in the hardware command queue |
| Noodles protocol 1.5, same AR4000 combat | 15.9–17.8 fps in active-combat samples, with 14.4–15.2 ms per frame in the hardware command queue |
| Videos | play, but stutter: the CPU cannot keep up |
| Memory | about 200 MB after loading a game, up to about 380 MB in a large fight |

What made the difference, in order: drawing the fog of war with sprites instead of blended triangles
(`SpriteFogOfWar=1`), the NEON alpha blit and fill, and the triangle-fill change.

The protocol 1.5 comparison used the same statistics-enabled path as the
protocol 1.4 baseline. Its descriptor ring eliminated measured sprite-batch
submission stalls and reduced command-queue drains from about 25–27 per frame
to about one per frame. A 15-second combat sample used 41.8% of one Cortex-A9
core, compared with 48–50.5% in the protocol 1.4 sample.

## Requirements

- A MiSTer with a DE10-Nano (Cyclone V SoC), running the Menu core, with a MiSTer Linux image that has the `MiSTer_fb`
  and `MrAudio` devices. Tested on the Buildroot image with Linux 6.18.38.
- A USB mouse and keyboard, and an HDMI display that accepts 800x600 at 60 Hz.
- The data files of the games you want to play, 1.4 to 2.6 GB each (see [Supported games](#supported-games)).
- **A USB drive for swap memory when using the software renderer**, plugged in before you start a game, formatted as
  **ext4** with at least 500 MB free. The tested Noodles BG2 launch does not require it. See "The swap file" below.
- To build: a Linux PC (Ubuntu 26.04 was used), about 2 GB of free disk space, a network connection for fetching
  sources, and a checkout of [MiSTer-VCMI](https://github.com/aquasock/MiSTer-VCMI) next to this repository: the SDL2
  driver sources are shared, not copied.

## Supported games

| Game | Folder | Launcher | GOG installer | Resolutions | Status |
| --- | --- | --- | --- | --- | --- |
| Baldur's Gate II (Shadows of Amn, Throne of Bhaal) | `games/bg2` | `gemrb-bg2` | Baldur's Gate 2 Complete | 640x480, 800x600 | Played the most. One known crash, see below |
| Baldur's Gate | `games/bg1` | `gemrb-bg1` | Baldur's Gate: The Original Saga | 640x480, 800x600 | Short test, runs well |
| Planescape: Torment | `games/pst` | `gemrb-pst` | Planescape: Torment 1.01 | 640x480, 800x600 | Works. This edition has no movie files, so cutscene videos are missing |
| Icewind Dale (with Heart of Winter and Trials of the Luremaster) | `games/iwd` | `gemrb-iwd` | Icewind Dale Complete | 640x480, 800x600 | Menus and navigation checked |
| Icewind Dale II | `games/iwd2` | `gemrb-iwd2` | Icewind Dale 2 (2.01 fixes) | 800x600 only | Works |

Notes on the GOG installers, after unpacking them with innoextract:

- **Baldur's Gate, Baldur's Gate II, Planescape and Icewind Dale:** the game is in the `app` folder. Copy its contents,
  including sub-folders such as Icewind Dale's `CD2` and `CD3` (the original game keeps its area graphics there as
  compressed `.cbf` files, which GemRB reads).
- **Planescape and Icewind Dale II keep their settings files in `__support/app`** (`Torment.ini`, `beast.ini`,
  `quests.ini`, `autonote.ini`, `Keymap.ini`, `layout.ini`, or `icewind2.ini`, `Party.ini`, `Keymap.ini`), where the GOG
  installer would copy them next to the game. GemRB reads them from the game folder, so copy the `.ini` files from
  `__support/app` into it.
- **Icewind Dale II** is unpacked straight into the output folder instead of an `app` folder: copy the folder that
  contains `CHITIN.KEY`, and leave out `__redist`, `__support` and `commonappdata`.
- Every game needs about 1.4 to 2.6 GB. The Enhanced Editions, mods and multiplayer are untested.

## Installation

### From a release

1. Download `MiSTer-GemRB-v<version>.zip` from the [Releases](https://github.com/aquasock/MiSTer-GemRB/releases) page
   and unzip it onto the root of the MiSTer's SD card, merging with what is there. This creates `/media/fat/gemrb`
   plus software-renderer launchers in `/media/fat/Scripts`, the silent `noodles-launcher.sh` watcher and Noodles game
   entries under `/media/fat/_Utility`. The location matters: the programs find their libraries under
   `/media/fat/gemrb`.
2. Copy your game files into `/media/fat/gemrb/games/<game>`, one folder per game (the table under
   [Supported games](#supported-games) says which folder is which). For the GOG releases, unpack the installer with
   [innoextract](https://constexpr.org/innoextract/) and copy the game folder it makes. The installer's programs (`.exe`,
   `.dll`), manuals and helpers are not needed. Use the classic editions (on GOG, the classic Baldur's Gate games are
   redeemable free with the Enhanced Editions), not the Enhanced Editions themselves.
3. On the MiSTer, open the OSD (F12), choose **Scripts**, and run the launcher of the game you want. Most launchers first
   ask which resolution to use, 640x480 or 800x600: type 1 or 2 and press Enter. Icewind Dale II has no 640x480 layout and
   always uses 800x600. Try both sizes on the others and keep the one your display and the game prefer. The screen then
   blinks as the output switches to that resolution. Quit from the game's own menu; the launcher switches your display
   back.

   For Noodles acceleration, run `noodles-launcher.sh` from the OSD Scripts menu. It returns immediately and waits silently
   for 30 seconds. During that interval return to the core browser and select **Utility > Baldurs Gate II (GemRB)**. The
   MGL loads the timing-qualified protocol 1.7 core; the watcher validates the selected RBF and `CHITIN.KEY`,
   releases stock Main's exclusive input grabs while preserving the core display, and starts GemRB. The same process
   supervises the game and restores Main's grabs on exit. The renderer can attach to protocol 1.4, protocol 1.5 adds the
   multi-table sprite-batch path, protocol 1.6 also batches opaque fills and protocol 1.7 adds the third display buffer.
   The matching `gemrb-<game>.sh` launcher remains
   the software-renderer fallback.

If you installed version 0.1.0, the first run moves its game folder, saves and settings into this layout for you.

The zip also holds `INSTALL.txt`, the license texts in `LICENSES/`, and `SOURCES.txt` listing the exact source versions
and checksums.

### From source

1. Build the bundle (see [Building](#building)).
2. Copy it, and the game data, to the MiSTer over SSH. `scripts/deploy.sh` logs in as `root` with the stock MiSTer
   password; edit it if yours differs.

   ```sh
   MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh code
   GAME_DATA=<extracted game folder> MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh data bg2
   ```

   `scripts/deploy.sh data <game>` also finds the game folder in `work/<game>-extract` by itself, and for Planescape and
   Icewind Dale II it copies the settings files the GOG installer keeps in `__support/app` (see the table).

3. Launch it from the OSD as above.

The SD card is mounted synchronously, so copying the game data takes a long time: about half an hour for 2.6 GB.

To remove it, delete `/media/fat/gemrb`, `/media/fat/Scripts/gemrb-*.sh`, `/media/fat/Scripts/noodles-launcher.sh` and
`/media/fat/_Utility/Baldurs Gate II (GemRB).mgl`.

## Configuration

Local settings go in `/media/fat/gemrb/env.sh`, which the launcher reads if it exists. Everything works without it.

| Setting | Meaning |
| --- | --- |
| `MISTER_RESOLUTION=640x480` (or `800x600`) | Skip the resolution question and always use this. Without it the launcher asks every time |
| `MISTER_OUTPUT_MODE=off` | Do not change the HDMI mode. Otherwise the launcher switches to the resolution you chose |
| `NOODLES_LAUNCH_WAIT=30` | Seconds the silent Noodles watcher waits for a matching game MGL; valid values are 1–300 |
| `MISTER_RESTORE_MODE="<modeline>"` | Mode to switch back to afterwards. Default is 1080p60 |
| `MISTER_USB=/media/usb0` | Which USB drive holds the swap file when several are plugged in (skips the question) |
| `MISTER_SWAP_MB=384` | Size of the swap file in MB |
| `MISTER_DEBUG=1` | Developer: run the game under gdb and write `crash.log` (call stacks of all threads, plus a raw stack dump) if it crashes. Needs the unstripped files from `scripts/debug-symbols.sh` and `scripts/deploy.sh debug`, which are not in the release zip |
| `MISTER_MALLOC_CHECK=1` | Developer, with `MISTER_DEBUG`: also use glibc's heap-checking allocator (slower) |
| `MISTER_SWAP=none` or `usb` | Disable swap, or explicitly use USB swap. Noodles BG2 defaults to `none`; software launches default to `usb` |
| `SDL_RENDER_NOODLES_RESIDENT_MB=192` | FPGA texture-residency budget used by the Noodles renderer; valid overrides are clamped to 8–220 MiB |
| `MISTER_CPUS=0` | ARM cores the game runs on, as a `taskset` list such as `0` or `0-1`. The MiSTer frontend occupies CPU 1, so the default is CPU 0 |
| `SDL_RENDER_NOODLES_BUFFERS=2` | Keep two display buffers on a protocol 1.7 Noodles core instead of the default three |

GemRB's own settings for each game are in `/media/fat/gemrb/GemRB-<game>.cfg`, which the launcher creates from
`GemRB-<game>.cfg.default` on the first run and never overwrites, so updating keeps your edits. The defaults are the SDL
audio driver, a 30 fps cap, intro videos skipped and sprite fog of war. The launcher writes the resolution you choose
into `Width` and `Height` in that file each time it starts. GemRB's documentation lists 640x480 as the only size the
original Baldur's Gate was made for and 640x480 and 800x600 as supported by Baldur's Gate II; in a short test Baldur's Gate
also ran correctly at 800x600, with the interface panels at the screen edges. Each game keeps its own files in `games/<game>`, saves in
`saves/<game>` and cache in `cache/<game>`. The options are described in GemRB's documentation.

The drivers read the `SDL_MISTER_*` environment variables described in the
[MiSTer-VCMI README](https://github.com/aquasock/MiSTer-VCMI#configuration); in `env.sh` they need `export`. The launcher
sets `SDL_MISTER_FORMAT=xrgb` (an alpha-less screen, which lets SDL use its fast blitter). Normal launchers select
`SDL_RENDER_DRIVER=software`; the parallel Noodles launchers select `SDL_RENDER_DRIVER=noodles`.
Set `SDL_RENDER_NOODLES_STATS=1` when starting a Noodles launcher to log a five-second summary of observed frame rate,
SDL command-queue time, presentation submission time, deferred presentation-wait time, and the remaining per-frame time.
Detailed timing separates fills, sprite batches and synchronization. `SDL_RenderPresent` leaves one submitted frame in
flight so the application can prepare and submit the next frame's commands while the FPGA renders and performs the
vertical-blank handoff. The Noodles SDK keeps those commands ordered after the flip and predicts the buffer they will
target. Texture updates made while a frame is in flight accumulate in their CPU shadows; their dirty rectangles are
uploaded to fresh managed surfaces when their previous surfaces are still in use, letting the SDK retire the old copies
at their fences. A second presentation, direct display readback or renderer teardown resolves the outstanding fence.
The reported presentation wait is only the completion time that could not overlap.
A pacing line samples every tenth presentation wait and polls the core's raw completion count at 0.5 ms intervals. It
splits each sample into draw commands still running before the pending `PRESENT`, the remaining flip and vertical-blank
interval, and the SDK's confirmation of the completed fence, which the core answers only between commands. The line
reports its sample count, samples whose draws had already finished when the wait began, and the rendering thread's user
and system CPU share and context switches per frame. With three display buffers the flip
part is instead the wait for the core to accept the queued flip, which happens once the previous flip has retired.
Set `SDL_RENDER_NOODLES_BUFFERS=2` to keep the two-buffer presentation path on a protocol 1.7 core for comparison.
Transient command-ring or descriptor pressure waits for one verified command of forward progress before retrying; it
does not drain through every later command or the queued presentation.
The summary
also reports accelerated opaque-fill, blended-fill, plain-draw and flagged-draw counts and pixels, fill- and sprite-batch
counts and maximum sizes, submission stalls, CPU fallback work, uploads, readbacks, evictions, drains and current FPGA
texture residency. Draw classes separate safe 64-bit copies, alignment reroutes, standard and explicit blend modes,
mirroring and modulation; draw and fill size buckets show how much traffic falls into each pixel-area range. While
statistics are enabled, a sparse sample of CPU-valid, unmodulated standard-alpha sources classifies adjacent pixel pairs
as opaque, transparent or partial. The renderer also conservatively tracks textures proven entirely transparent or
opaque by full-surface writes. Standard-alpha draws from transparent textures are skipped, while identity-modulated draws
from opaque textures use the plain copy path; partial or ambiguous writes invalidate that state. The alpha line reports
the skipped and converted work. A callback line attributes CPU time and call volume to SDL command construction, texture creation,
updates, locks, target changes, readback and destruction; its merged-fill count reports compatible calls appended to an
existing SDL command, and its rotated count reports updated textures moved away from in-flight storage. The counters and their performance-clock reads are disabled
otherwise; asynchronous presentation and its synchronization boundaries are unchanged.
Opaque points and simple lines use the FPGA fill engine. Operations that still need SDL's software rasterizer keep the
FPGA result coherent by reading and uploading only the affected target region.
Consecutive accelerated copies to the same target share the core's 64-entry sprite batches. On protocol 1.6,
consecutive opaque fills similarly share 64-entry fill batches. Switching between fills and copies, software fallbacks,
target changes and resource synchronization flushes the pending batch to preserve SDL command order. The SDL default target
is the core's current hardware back buffer, so presentation does not copy an intermediate 800x600 composition surface;
textures and explicit render targets remain managed surfaces. With protocol 1.5+ and SDK 0.10, each flushed sprite or
fill batch uses a free descriptor table with its own completion fence, allowing consecutive batches to remain queued
without overwriting each other's descriptors. Protocol 1.6 batches opaque fills; older cores retain scalar fills.
The Noodles SDL backend also appends consecutive compatible fill calls to one internal SDL command when their target,
color, blend, viewport and clip state match. Any intervening draw or state change ends that command, preserving SDL order
while avoiding thousands of command objects for engines that submit pre-rasterized horizontal spans one line at a time.

### The swap file

The MiSTer exposes about 490 MB of RAM to Linux. Software-renderer launches retain the USB swap default because large
fights and area transitions can hold enough creature and area data to exhaust that memory. The Noodles renderer keeps
texture contents in FPGA-visible DDR and releases redundant CPU shadows; BG2 completed the tested Throne of Bhaal fight,
game-over video and game-over screen with swap disabled, a 374 MiB observed RSS peak and at least 107 MiB still available.
The Noodles BG2 MGL therefore defaults to no swap. Set `MISTER_SWAP=usb` in `env.sh` to add the safety file for a mod,
longer session or workload that needs it. Set `MISTER_SWAP=none` to disable swap for a software-renderer test.

- When USB swap is selected, one file, `gemrb-swapfile` (384 MB), is added to the drive. The drive must already be formatted as ext4 (do that
  once on a Linux PC, for example with `mkfs.ext4`; FAT32 is far too slow, about 0.1 MB/s to create the file) and have at
  least 500 MB free. The launcher never erases or formats anything. The launcher only looks at drives mounted
  under `/media/usbN` and never touches anything else. The file is kept for next time; delete it by hand if you no
  longer want it.
- The file is created in the background the first time (seconds on a fast drive, minutes on a slow one) and swap is only
  available once it exists. Do not unplug the drive while playing.
- The swap is never put on the SD card: the SD card is mounted synchronously, so swapping there is very slow (an area
  load could stall the game for a minute), and constant writes wear it.
- The kernel is told to use the swap only as a last resort, so normal play barely writes to it, and it is switched off
  when the game exits. The speed of the drive matters, most of all its random 4 KB write speed: a card reader with a good
  microSD card or a USB SSD is much better than a cheap flash stick.

## Current limitations

- **Only short sessions were played in most games.** Baldur's Gate II has had the most play. The other four were checked
  for a while each (menus, walking around, saving and loading, quitting), not played through. The Enhanced Editions,
  mods and multiplayer have not been tested.
- **Known crash: one Baldur's Gate II: Throne of Bhaal fight can crash the game.** In a scripted encounter where an
  enemy starts a cutscene and then summons Kobold Commandos (ranged attackers), the game sometimes aborts a few
  seconds into the fight with a glibc heap error (`corrupted double-linked list` or `unaligned tcache chunk`), right
  after one of the kobolds' shots misses. Other summoned creatures (dogs, in the same encounter) do not trigger it. It
  is not caused by audio, by this port's SDL speedups, by the swap file or by the allocator settings (each was switched
  off in testing and it still happened), so it is most likely a bug in GemRB 0.9.5 itself, but the cause is not found
  yet. Save before scripted fights; quit from the game menu and start again if the screen freezes. Tested at 640x480
  only. `MISTER_DEBUG=1` (see the settings table) writes `crash.log` with the call stacks if you want to report it.
- **The frame rate is around 20 fps in busy scenes**, and videos stutter. Both are limits of the CPU.
- **Characters are drawn in front of walls and trees.** GemRB hides characters behind scenery with a stencil that needs
  blend modes SDL's software renderer does not have, so it is skipped. Some glow effects use plain additive blending.
- **The fog of war has the original game's blocky style**, because sprites are much cheaper here than blended geometry.
- **Intro videos are skipped** by default (they played choppily).
- **Your original HDMI mode is not saved.** The MiSTer here has no `MiSTer.ini`, so the launcher restores 1080p60. Set
  `MISTER_RESTORE_MODE` if your display uses something else.
- **GemRB ignores `SIGTERM`.** Quit from the game menu, or use `kill -KILL`; the launcher restores the display either
  way.

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
MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh code
GAME_DATA=<extracted game folder> MISTER_HOST=<your MiSTer's IP> scripts/deploy.sh data bg2
```

To profile ARM CPU time without stopping GemRB, build and copy the standalone sampler, capture the main GemRB process,
then symbolize the result on the build PC:

```sh
scripts/build-profiler.sh
scp work/profiler/noodles-perf-sampler root@<MiSTer-IP>:/tmp/
ssh root@<MiSTer-IP> '/tmp/noodles-perf-sampler <GemRB-PID> 10 499' > work/noodles-perf.capture
tools/symbolize-perf-samples.py work/noodles-perf.capture
```

To measure the loaded core's engine rates and `PRESENT` pacing without GemRB, build the diagnostic package, copy
`work/noodles-test` to the MiSTer and run its `throughput.sh` while the Noodles core is loaded and GemRB is stopped. It
times full-surface fills, copies and blends between off-screen managed surfaces, then shows how long `PRESENT` occupies
the command queue as off-screen draw work before it grows, whether a command queued behind `PRESENT` can run before the
flip completes, and how long the SDK's confirmed wait trails the core's raw completion count while later commands run. Its
`contention.sh` measures whether engine traffic slows the ARM: pinned to CPU 0, it times dependent-load latency, streaming
reads and copies while CPU 1 keeps the engine idle or busy with fills, plain draws, blends or a mix, and reports each
result relative to the idle engine. `MISTER_HOST=<ip> tools/mister-mpfe.sh show` decodes the HPS SDRAM controller's port
priorities and weights, which decide how the ARM and FPGA share DDR3; `default`, `equal`, `mpu`, `mpu-priority` and
`mpu-remap` apply a validated profile until the next boot, and `default` restores the preloader values.

The sampler uses Linux `perf_event_open`, samples only the main thread, and does not pause or trace the game process.
Keep the matching unstripped `work/gemrb-install` and `work/prefix` trees on the PC for useful function names.

Everything is created under `work/`, which is not tracked, and the pinned versions are in `scripts/env.sh`. Each
dependency step leaves a stamp, so a rerun skips finished work; changing the SDL drivers or patches rebuilds SDL
automatically.

`scripts/release.sh <version>` builds the release zip (`work/dist/`) from a fresh bundle: it adds every license text and
the source list, and writes `SHA256SUMS`. Release notes live in `docs/release-notes/`. Run it from a committed tree, so
that `SOURCES.txt` records the commit.

## Source layout

- `scripts/` — `env.sh` (versions and paths), `toolchain.cmake`, `enable-asm.cmake`, `build-deps.sh`, `build-gemrb.sh`,
  `bundle.sh` (also generates the launcher and the default `GemRB-<game>.cfg` files), `deploy.sh` and `release.sh`.
- `patches/0001-*`, `patches/0002-*` — the changes to GemRB, applied by `build-gemrb.sh`.
- `patches/sdl2/` — the changes to SDL, applied by `build-deps.sh`.
- `work/` — created by the scripts: downloads, sources, build trees, the install prefix and the bundle. Not tracked.

## Documentation

- [Release notes](docs/release-notes/0.1.0.md)
- [Source attributions](ATTRIBUTIONS.md)
- [GemRB documentation](https://gemrb.org)
- [MiSTer-VCMI](https://github.com/aquasock/MiSTer-VCMI), which supplies the SDL2 drivers

## License

Original project code is licensed GPL-2.0-or-later (see `LICENSE.txt`), the same baseline as GemRB, MiSTer-VCMI
and MiSTer-DCSS. The complete bundle is distributed under the same terms. GemRB, Python,
SDL, FreeType, glibc and the other libraries keep their own licenses; see [ATTRIBUTIONS.md](ATTRIBUTIONS.md) for the
full inventory and the redistribution checklist.
