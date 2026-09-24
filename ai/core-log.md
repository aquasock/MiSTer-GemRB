## 1 COMMIT Unreleased 35f1d20 2026-09-24T00:38:39-07:00

#### Coming From:

Unreleased e2e47b7

#### Purpose:

Add the first SDL2 Noodles renderer integration and an isolated hardware diagnostic without changing the accepted GemRB launcher path.

#### Outcome:

Source `35f1d20` pins MiSTer-Noodles `2075eb13f5a70bb4ebc1bf127cac014fb134f860` with archive SHA256 `5cfa00c6d56ad4b974ad239502c39dadced2c86b2bd070fd18b45bf5145e4e71`, cross-builds its SDK as position-independent code and links it directly into SDL 2.32.10. The new SDL renderer named `noodles` requires protocol 1.3 and implements native `SDL_PIXELFORMAT_ABGR8888`, managed textures and render targets, texture update and streaming locks, clear and opaque fill, clipped unscaled copy, horizontal and vertical mirroring, colour and alpha modulation, SDL BLEND, ADD, MOD and MUL modes, arbitrary supported custom blend factors, readback and presentation through a managed 800x600 composition surface. Unsupported scaling, rotation and primitives return errors rather than rendering incorrectly. The deterministic diagnostic checks readback pixels for the accelerated operations and produces a centered visual result, while the GemRB launchers continue to select the accepted software renderer. A clean SDK and SDL rebuild, the diagnostic build and the GemRB cross-build passed; the diagnostic SHA256 is `faf00dc6b3ba6be4fddf9aab9b261372751565b87b2e429dda39a1a0da1f1f05`, its SDL library SHA256 is `557109a18bc2ed65220aaadca98abd4152589f6aaf2b36f79fe670fac7bd4e5b`, and the hash-verified package is staged separately on the MiSTer under `/media/fat/gemrb-noodles-test` without replacing the installed GemRB bundle.

#### Next Steps:

Run `/media/fat/gemrb-noodles-test/run.sh` while the timing-qualified Noodles protocol-1.3 core is loaded and record its pixel-readback result and visual output. If it passes, add synchronized CPU fallbacks and texture-residency management in the next cycle before selecting the renderer in GemRB.

#### Files Modified:

- patches/sdl2/noodles-renderer.patch
- scripts/build-deps.sh
- scripts/env.sh
- scripts/build-noodles-test.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- sdl-renderer/noodles/SDL_render_noodles.h
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [ ] Passed

---

## 3 COMMIT Unreleased e2fb908 2026-09-24T00:52:47-07:00

#### Coming From:

Unreleased 35f1d20

#### Purpose:

Add coherent CPU fallbacks for the SDL operations GemRB needs but Noodles protocol 1.3 does not accelerate.

#### Outcome:

Source `e2fb908` gives every managed surface a CPU shadow with explicit CPU-valid and FPGA-valid state, reads current FPGA contents before a CPU fallback, and uploads current CPU contents before later accelerated access. SDL's software raster helpers now handle scaled, mirrored and self copies, blended fills, points, lines and textured or untextured geometry while preserving the accelerated clear, opaque fill, unscaled copy, blend and present paths. Rotation and custom blend modes on CPU fallback operations return errors rather than rendering incorrectly. The expanded diagnostic interleaves FPGA and CPU operations on the same target and checks every transition. A fresh SDL configuration and build, diagnostic build and complete GemRB cross-build passed. The hash-verified diagnostic package, with binary SHA256 `89936cd1436be1e8269ecf76e66b10c580ee28b9dee0631c2d167412cf381ad9` and SDL library SHA256 `87732b631e56a1a30f0a0cf09275138c7bdaebca841caa8ccbd4735179865c20`, ran twice on the MiSTer and reported `Noodles renderer diagnostic: PASS (renderer=noodles, hash=99b2815b)` both times.

#### Next Steps:

Add bounded FPGA texture residency with deterministic eviction and restoration, then select the Noodles renderer in an isolated GemRB launcher and run a real game smoke test without changing the accepted software-renderer launcher.

#### Files Modified:

- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 2 COMMIT Unreleased 35f1d20 2026-09-24T00:51:40-07:00

#### Coming From:

Unreleased 35f1d20

#### Purpose:

Record hardware acceptance of the first SDL2 Noodles renderer and its isolated diagnostic.

#### Outcome:

The staged diagnostic ran twice consecutively on the standard MiSTer at 10.10.0.22 with the timing-qualified protocol-1.3 Noodles core, reporting `Noodles renderer diagnostic: PASS (renderer=noodles, hash=91df6ac5)` both times. The matching exact readback hash validates clear, opaque fill, clipping, unscaled copy, both mirror axes, colour and alpha modulation, SDL BLEND, ADD, MOD and MUL modes, GemRB's custom stencil blend and managed render targets; the second clean open validates session release and recovery. The user observed the centered colour and palette map on screen, accepting the presentation path visually. The installed GemRB bundle and its software-renderer launchers remained unchanged.

#### Next Steps:

Add synchronized CPU fallbacks for the SDL operations that protocol 1.3 does not accelerate, validate mixed CPU and FPGA command ordering independently, and defer texture eviction to a following boundary so coherence and memory pressure remain separately diagnosable.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 4 COMMIT Unreleased 28e1a83 2026-09-24T01:12:00-07:00

#### Coming From:

Unreleased e2fb908

#### Purpose:

Bound SDL texture residency in the Noodles managed-surface arena so GemRB can retain more decoded textures than fit in FPGA-addressable memory.

#### Outcome:

Source `28e1a83` creates CPU texture shadows without immediately allocating managed Noodles surfaces, tracks resident bytes and least-recently-used hardware access, preserves GPU-only contents before eviction, and restores textures from their CPU shadows when hardware next needs them. The 800x600 composition surface remains pinned, the active render target stays resident while a copy source is restored, and the default 192 MiB budget leaves 32 MiB of deterministic headroom in the SDK's 224 MiB arena; `SDL_RENDER_NOODLES_RESIDENT_MB` can lower the budget for testing. Allocation first drains and collects deferred frees before evicting a live surface. The diagnostic uses a 12 MiB budget, cycles three 4 MiB textures to force deterministic eviction and revisits the first texture to verify restored pixels. A fresh SDL configuration and build, diagnostic build and complete GemRB cross-build passed. The hash-verified diagnostic package, with binary SHA256 `301012988ea8329ae5da329d2fba162a474a8f8f5819e40e88d9f79a24b686d7` and SDL library SHA256 `383017eb51e5c75e24c6597b0712f14c9997a1a7da11c92f0d509f5b1123f11a`, ran twice on the MiSTer and reported `Noodles renderer diagnostic: PASS (renderer=noodles, hash=b656c299)` both times.

#### Next Steps:

Add an isolated Noodles GemRB launcher and run a real installed-game smoke test with renderer selection and frame behavior recorded before changing any default launcher.

#### Files Modified:

- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 5 COMMIT Unreleased 3aff3e6 2026-09-24T01:30:00-07:00

#### Coming From:

Unreleased 28e1a83

#### Purpose:

Add isolated Noodles launchers for the installed GemRB games without changing the accepted software-renderer launchers.

#### Outcome:

Source `547756c` adds `run-noodles.sh` and explicit `gemrb-noodles-*` OSD launchers for BG1, BG2, Planescape: Torment, Icewind Dale and Icewind Dale II while retaining the existing launchers and their software default. Source `3aff3e6` also sets SDL's framebuffer-acceleration hint for the software path, preventing SDL's window-surface helper from opening a hidden Noodles renderer. The deployed SDL library SHA256 is `49bf1fcab8ca03b2a1f8cd7be8287ae23d4c50e6f89f57065df492079dea5e57`; the deployed wrapper SHA256 values are `ae9cefdcddbc012e8018a9f98de45ad241d78e328a83c9c062a0ad8e86897492` for `run.sh` and `fafefafc71d26d489e92ffeb112c894080655135397f5899ec4577fcbf11be43` for `run-noodles.sh`.

All ten OSD launchers were present after deployment. Baldur's Gate II selected `Renderer: software`, completed core initialization and loaded its start script through the normal launcher; after its deliberate SIGKILL, the Noodles diagnostic opened on the same core boot and passed with hash `b656c299`, proving the software path did not claim the hardware session. The parallel launcher selected `Renderer: noodles`, completed core initialization, loaded the start GUI and title assets, began menu music and remained alive for the 25-second smoke window until deliberate SIGKILL, with no renderer error, out-of-memory event or kernel fault. Reloading the timing-qualified core recovered the intentionally dirty session, and two consecutive diagnostics passed with hash `b656c299` and left the session clean.

#### Next Steps:

Move GemRB's null-renderer check immediately after `SDL_CreateRenderer` so a busy or stale Noodles session reports a controlled startup error, then add frame-timing measurements and exercise an actual BG2 gameplay area before considering broader game qualification or any default change.

#### Files Modified:

- README.md
- scripts/bundle.sh
- scripts/deploy.sh

#### Status:

- [x] Built
- [x] Passed

---

## 6 COMMIT Unreleased ??? 2026-09-24T01:35:00-07:00

#### Coming From:

Unreleased 3aff3e6

#### Purpose:

Harden GemRB renderer startup and add opt-in Noodles frame timing for qualification in a real Baldur's Gate II gameplay area.

#### Outcome:

The planned GemRB patch will check the result of `SDL_CreateRenderer` before any renderer API is called so an unavailable or stale Noodles session produces a controlled startup failure. The Noodles SDL backend will gain disabled-by-default periodic statistics covering observed frame rate, command-queue time and presentation time, allowing the remaining frame interval to be attributed to GemRB and other SDL work without changing normal runtime behavior.

#### Next Steps:

Apply the GemRB error-path patch during reproducible builds, add and document the statistics control, run clean cross-builds, verify the diagnostic on hardware, deliberately test a refused Noodles open, and collect measurements from an installed BG2 gameplay area through both Noodles and software launchers.

#### Files Modified:

- README.md
- patches/0003-sdlvideo-check-renderer-creation.patch
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [ ] Built
- [ ] Passed

---
