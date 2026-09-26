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

## 6 COMMIT Unreleased cfb7985 2026-09-24T07:15:38-07:00

#### Coming From:

Unreleased 3aff3e6

#### Purpose:

Harden GemRB renderer startup and add opt-in Noodles frame timing for qualification in a real Baldur's Gate II gameplay area.

#### Outcome:

Source `466325b` adds disabled-by-default five-second Noodles statistics for observed frame rate, SDL command-queue execution, presentation and remaining frame work, and moves GemRB's renderer-null check immediately after `SDL_CreateRenderer`. A busy-session hardware test exposed a second caller that continued after `CreateSDLDisplay` returned `GEM_ERROR`; source `cfb7985` propagates that error from `CreateDriverDisplay`. Fresh SDL and GemRB cross-builds passed, and a deliberately refused Noodles open then exited with status 255 and a controlled `Cannot initialize shaders` fatal error instead of dereferencing the absent renderer. The deployed diagnostic retained SHA256 `301012988ea8329ae5da329d2fba162a474a8f8f5819e40e88d9f79a24b686d7` and passed on hardware with hash `b656c299`; the deployed SDL bundle has SHA256 `1b74f86a45d6f196a81442efa533aa1577ad5c4535a8f5da954337cae7789a5e`, GemRB has SHA256 `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`, and the final `SDLVideo.so` has SHA256 `5d76ce7c30287bd55734cd63aa126406cc6bbd2d77bfc8c376f8dfa6547e00ba`.

The BG2 start screen ran at 18-19fps with approximately 24.5ms in queue execution and 28-30ms in presentation. After loading the Throne of Bhaal AR4000 autosave, the user confirmed correct colours, tiles and sprites with no visible corruption, but performance fell below 1fps. Queue execution rose progressively to 7.1 seconds per frame while presentation remained 27-37ms and non-renderer work remained mostly 7-15ms; GemRB used 148MB RSS, had 329MB available system memory and used no swap. This isolates the gameplay failure to submitted rendering work rather than presentation, CPU game logic or memory pressure. Direct SSH launches also established that Main retains exclusive physical-input grabs unless they are explicitly released after its video-mode switch; after releasing Main's actual event-device descriptors, GemRB received the physical keyboard and mouse correctly. GemRB opened `/dev/MrAudio` and loaded and played the area music resources, but the user heard no audio, confirming that the remaining core-audio path is separate from application audio initialization.

#### Next Steps:

Add opt-in per-frame counts and pixel totals for accelerated fills, copies and blends, CPU fallbacks, uploads, readbacks and residency events, then repeat the AR4000 test to identify the operation class responsible for the seven-second queue. Use that measurement to scope SDL draw batching or the smallest required core change. Integrate a supported Main input handoff into the OSD launch path and implement core audio in separate bounded cycles after rendering throughput is understood.

#### Files Modified:

- README.md
- patches/0003-sdlvideo-check-renderer-creation.patch
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [x] Built
- [x] Passed

---

## 7 COMMIT Unreleased 64e3f54 2026-09-24T07:25:48-07:00

#### Coming From:

Unreleased cfb7985

#### Purpose:

Measure the rendering workload responsible for the multi-second Noodles command queue in Baldur's Gate II gameplay.

#### Outcome:

Source `64e3f54` extends the disabled-by-default five-second statistics with command and pixel totals for accelerated fills, plain and flagged draws, software copies and blended fills, primitive and geometry counts, submission stalls, upload and readback counts and bytes, evictions, drains and current FPGA residency. Fresh SDL, diagnostic and complete GemRB cross-builds passed. The deployed SDL library SHA256 is `571a12f459403ce488f0cc7fd01af8b47a41d3a7d656b27657176a2bf58a80bf`; the diagnostic binary retained SHA256 `301012988ea8329ae5da329d2fba162a474a8f8f5819e40e88d9f79a24b686d7` and passed on the timing-qualified protocol-1.3 core with hash `b656c299`.

The BG2 start screen held about 18.3fps while processing approximately 2.9 million accelerated pixels and seven queue drains per frame, with no CPU fallback, readback, eviction or swap activity. In the same Throne of Bhaal AR4000 save, steady gameplay fell to 0.14-0.20fps while each frame contained only 1.04 million fill pixels, 0.40 million plain-draw pixels and 1.27 million flagged-draw pixels. The actual bottleneck was 98 software primitive commands and 19 blended software fills interleaved with accelerated draws: each transition invalidated the whole 800x600 CPU or FPGA copy and produced about 110 full-surface readbacks plus 110 full-surface uploads, reaching 201MiB in each direction per frame. Queue time reached 6.83 seconds while presentation remained 39ms, memory remained healthy at 144MB RSS with 333MB available, and swap remained unused. The workload therefore does not require greater blit pixel throughput; it requires eliminating full-surface synchronization around small fallback primitives.

#### Next Steps:

Accelerate opaque points and lines with the existing solid-fill command, and make blended fill and remaining primitive fallbacks read and upload only their affected region while preserving command order and surface coherence. Validate mixed accelerated and regional-fallback ordering in the diagnostic before repeating the same AR4000 save. Defer new RTL primitive opcodes unless the regional path remains a measured bottleneck.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [x] Built
- [x] Passed

---

## 8 COMMIT Unreleased 9ced707 2026-09-24T07:34:17-07:00

#### Coming From:

Unreleased 64e3f54

#### Purpose:

Eliminate full-surface CPU and FPGA synchronization around small fallback primitives in the Noodles SDL renderer.

#### Outcome:

Source `9ced707` submits opaque points and horizontal, vertical or 45-degree lines through the existing solid-fill command, while arbitrary antialiased lines retain SDL's rasterizer. Blended fills and remaining primitive fallbacks read the affected bounding region from the current FPGA target, render into that region of the CPU shadow and upload the same region immediately, preserving command order and the untouched pixels inside and outside the bounding box without marking the whole shadow current. Fresh SDL, diagnostic and complete GemRB cross-builds passed. The deployed stripped SDL library SHA256 is `4e0a4a25d7c8dc561d7a1f01bab9cd54ecdcec5f3e8dce7bd9857cd8b937ff9c`, the expanded diagnostic binary SHA256 is `80ea3787aa6ccf7910b38fdc64e164a05c04244265ee3867777f031d68daed72`, and its SDL library SHA256 is `06ca8887bcc011745719b30207b87bdcfe81f962e8243097b56bb18d643ca2cb`. The hardware diagnostic passed with hash `31b60c99`, including a regional blended fill between accelerated operations.

In the same BG2 Throne of Bhaal AR4000 save, the user reported substantially better performance and did not report a visual problem. Steady gameplay improved from 0.14-0.20fps to 6.5-6.8fps. Per-frame synchronization fell from approximately 201MiB of uploads plus 201MiB of readbacks to 0.40MiB in each direction, CPU primitive fallbacks fell to zero, and the remaining 17 blended fill fallbacks per frame transferred only their regions. Queue execution fell from 5.0-6.8 seconds to approximately 104ms; presentation remained 35-37ms and other work remained about 8ms. The remaining queue pressure is 104 plain plus 137 flagged draws per frame submitted as one-entry sprite batches, producing approximately 228 draw stalls and drains per frame even though the total accelerated workload is only about 2.7 million draw and fill pixels.

#### Next Steps:

Group consecutive compatible draws to the same target into the core's existing 64-entry sprite batches and flush them at ordering boundaries such as fills, regional fallbacks, uploads, readbacks and presentation. Extend statistics with submitted batch counts and sizes, verify exact mixed-operation ordering in the diagnostic, and repeat AR4000 before considering a double-buffered descriptor table or a direct-back-buffer presentation path.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 9 COMMIT Unreleased 09ccf56 2026-09-24T07:35:37-07:00

#### Coming From:

Unreleased 9ced707

#### Purpose:

Reduce Noodles command-queue stalls by grouping consecutive SDL draws into the core's existing sprite-batch operation.

#### Outcome:

Source `09ccf56` accumulates up to 64 consecutive managed-surface draws to the same target and submits them through the existing ordered sprite-batch operation. Pending draws flush at target changes, fills, regional software fallbacks, surface transfers, residency changes, queue completion, destruction and presentation so batches cannot reorder visible operations or outlive referenced surfaces. The disabled-by-default statistics now report batch count and maximum occupancy, and the diagnostic crosses a full 64-entry batch before an ordered fill and following draw. Fresh SDL, diagnostic and complete GemRB cross-builds passed. The deployed stripped SDL library SHA256 is `1431fd7d2ea72a1e6b7493d2fbfed76eb936fe1ea87c63bc06a122a6982332f2`, the diagnostic binary SHA256 is `687ca9d94befe61f0131d5e826f9eef95a66746abae0adede516f56e2c80684e`, and its SDL library SHA256 is `59b1a87e85d9e7c7f8982684db2bcef36b3b747d93d14bf72fdabc3de9b95d9a`. The timing-qualified protocol-1.3 core passed the expanded hardware diagnostic with hash `a6d5728e`. In the same BG2 Throne of Bhaal AR4000 save, the user observed an improvement and steady gameplay rose from 6.5-6.8fps to 11.3-11.6fps. Queue execution fell from approximately 103-107ms to 37-39ms per frame while the comparable workload retained about 1.04 million fill pixels, 0.40 million plain-draw pixels and 1.28 million flagged-draw pixels per frame. Approximately 268 draws per frame became 22 sprite batches with a measured maximum of 64 entries; draw stalls fell from about 228 to 4.1 per frame, with total drains about 5.1 per frame including one fill stall. Presentation now accounts for approximately 41ms and other work remains 8-9ms per frame. Regional synchronization remains bounded, memory has 273MiB available, and swap remains unused.

#### Next Steps:

Instrument the full-screen composition-to-back-buffer copy separately from vertical-blank wait and split the remaining queue time among fill, sprite-batch and regional synchronization submissions. Use that measurement to choose between a direct or page-flipped presentation path and reducing residual batch or fill serialization, while keeping core audio and supported Main input handoff in separate cycles.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 10 COMMIT Unreleased 774a7eb 2026-09-24T07:48:20-07:00

#### Coming From:

Unreleased 09ccf56

#### Purpose:

Measure the remaining presentation and command-queue costs after sprite batching so the next FPS optimization targets the dominant host-side path.

#### Outcome:

Source `774a7eb` adds disabled-by-default timing for solid fills, sprite-batch submission, queue and total CPU-to-FPGA synchronization, remaining command-queue work, the full-screen composition copy and vertical-blank waiting. With statistics enabled only, presentation waits for the composition copy to finish before submitting the present command so copy execution and display wait can be measured independently; the normal path remains asynchronous. Fresh SDL, diagnostic and complete GemRB cross-builds passed. The deployed stripped SDL library SHA256 is `3b0262e2aa5655bd444786419cbe263517f208788bc4650e9d02945b07c75d2d`, the unchanged diagnostic binary SHA256 is `687ca9d94befe61f0131d5e826f9eef95a66746abae0adede516f56e2c80684e`, and its SDL library SHA256 is `452f079dfed4561278ead53ccc160dfeb885d80644d6cf9695bf2daf8bc4100e`. The existing timing-qualified protocol-1.3 core passed the exact-pixel hardware diagnostic with hash `a6d5728e`. In the same Throne of Bhaal AR4000 area, the deliberately separated path measured 9.8-10.1fps with approximately 45-47ms in the command queue, 46ms in presentation and 8-9ms elsewhere. Per frame, queue time consists of approximately 19ms of regional synchronization, 14ms of CPU fallback and command processing, 9.4ms of sprite-batch submission and waits, and 3.3ms of fills. Presentation consists of approximately 35ms copying the completed 800x600 composition surface to the hardware back buffer and 11ms waiting for display retirement. The copy is therefore the largest single remaining cost and is avoidable because protocol 1.3 already exposes the current back buffer and accepts sprite batches and raw fills directed to it. Memory remained healthy with 305MiB available and swap unused.

#### Next Steps:

Add bounded SDK operations for reading, updating and filling the current back buffer, then make the SDL default render target use that buffer directly while retaining managed surfaces for textures and explicit render targets. Preserve CPU-fallback coherence and exact command order, validate render-target switching and regional fallback behavior in the diagnostic, and repeat AR4000 without changing the RBF. Defer audio until a performance change requires an RBF rebuild so both hardware changes can share qualification.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [x] Built
- [x] Passed

---

## 11 COMMIT Unreleased 667661e 2026-09-24T07:59:06-07:00

#### Coming From:

Unreleased 774a7eb

#### Purpose:

Eliminate the measured full-screen presentation copy by rendering SDL's default target directly into the current Noodles back buffer.

#### Outcome:

Source `43b4fe7` pins MiSTer-Noodles `0df688d`, makes the SDL default target use the core's current back buffer for fills, sprite batches and synchronized CPU transfers, retains managed surfaces for textures and explicit render targets, removes the 800x600 composition copy from presentation, extends the exact-pixel diagnostic and makes dependency stamps follow the pinned SDK revision. The first hardware diagnostic exposed a remaining managed-surface guard on full default-target readback; source `667661e` corrected that guard, and fresh SDK, SDL, diagnostic, GemRB and bundle builds passed. The deployed diagnostic binary SHA256 is `92c9fdb1b8db581786c35e0f88120d814d8eae463136fa0956434d2ce7412c97`, its SDL library SHA256 is `7ad9759e13d7ab39163e56ceabcdd6d4cd0a045a667810ad778588114eeabbfd`, the stripped GemRB SDL library SHA256 is `32929b4a014c9dd1682d05d3c4f17db17790cf6cd571f3ee5b32ea249e9f9164`, and the timing-qualified protocol-1.3 core passed the expanded hardware diagnostic with hash `a6d5728e`. In the same Throne of Bhaal AR4000 save, the user observed better performance and no visual fault. Four uncontaminated live timing intervals measured 11.78-12.06fps, 45.6-47.7ms in the command queue, 28.2-28.9ms in presentation and 8.3-8.8ms elsewhere; the presentation copy is zero, compared with the prior deliberately serialized 9.8-10.1fps measurement whose copy alone took about 35ms. Memory retained 239MiB available and swap remained unused.

#### Next Steps:

The remaining host-visible cost is approximately 20 blended rectangle fallbacks per frame, which spend about 19-20ms in regional synchronization plus 13-14ms in CPU fallback and queue handling; the existing host path already keeps those transfers regional and ordered. The next material FPS step should add an FPGA blended-fill operation and remove those fallbacks, which requires a new RBF, so enable the existing MiSTer ALSA path in the same hardware qualification cycle as requested. Also replace the manual post-launch Main input-grab release with a supported handoff because opening the OSD makes Main reacquire the physical devices.

#### Files Modified:

- README.md
- scripts/build-deps.sh
- scripts/env.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 12 COMMIT Unreleased 7eb7efd 2026-09-24T08:29:17-07:00

#### Coming From:

Unreleased 667661e

#### Purpose:

Move SDL blended rectangle fills onto the new Noodles hardware operation and qualify GemRB audio over HDMI with the same RBF.

#### Outcome:

Source `7eb7efd` pins the exact MiSTer-Noodles `2dea6a1` source used for the timing-qualified protocol 1.4 RBF, requires the blended-fill capability, and routes every supported SDL rectangle blend mode on managed and default targets through hardware while retaining opaque fills and the bounded unsupported-mode fallback. Separate counters expose opaque and blended hardware fills. The diagnostic adds a managed custom blended fill, preserves exact readback checks on both target kinds, and queues a deterministic one-second 48 kHz stereo S16 tone through SDL's `mister` audio driver. Fresh SDK, SDL, diagnostic, GemRB and bundle builds passed; the diagnostic binary SHA256 is `fe8153e4f4279dba13c7d7e0293a5945b73c02cdf22d9c45fb69e04ae77f49cb`, its SDL SHA256 is `75ed5c5fc0d8a7132bce5131e1cda0cd10c0d5c355d65980df6731453e97878a`, and the deployed bundle SDL SHA256 is `0f207da2ee273c461ece9cf2fb2e8e5dc7afbeece00b783f6c27b13feb25c7ed`. On the seed-13 core the diagnostic passed with renderer hash `64d5728e`, selected the `mister` audio driver and drained its tone; the user also heard the direct RBF tone and GemRB menu music over HDMI. In the same Throne of Bhaal AR4000 save, cutscene intervals ran at 15.5-19.3fps and settled combat rose from 11.58 to 13.93fps while command-queue time was 24.1-26.5ms per frame, roughly half the prior 45.6-47.7ms. Hardware handled about 26-28 blended fills per combat frame with zero blended-fill stalls, CPU fills or readbacks; GemRB consumed 48-50.5% of one Cortex-A9 core during sampled combat. The run reached the game-over video without a renderer fault, and that video held 19.8fps with zero fallback work. Peak RSS was 390MiB, USB swap remained effectively unused, and the intentionally terminated post-game run exited 137 after launcher cleanup. The complete captured log has SHA256 `ed6f80e6a12cf93b6910dd9e3f720d62dd89e774de0f46f0a74139a8c51f04b8`.

#### Next Steps:

Repeat AR4000 once with statistics disabled to quantify instrumentation overhead, then add a supported Main input handoff so an OSD launch cannot retain or reacquire the physical devices. The next rendering optimization should address the high sprite-batch drain count through core descriptor buffering; blended fills no longer justify host-side work.

#### Files Modified:

- README.md
- scripts/build-noodles-test.sh
- scripts/env.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 13 COMMIT Unreleased 50cf286 2026-09-24T10:21:35-07:00

#### Coming From:

Unreleased 7eb7efd

#### Purpose:

Integrate MiSTer-Noodles protocol 1.5 and SDK 0.9 so GemRB sprite batches can use independent descriptor tables without draining the previous batch first.

#### Outcome:

Source `50cf286` pins MiSTer-Noodles `513f218` and its GitHub archive SHA256 `cd8bf87cec9e7168364dab2dbd5fd3c0df43373c324a4dff368f982fa56b8814`, rebuilds the renderer against SDK 0.9 and documents protocol 1.5 as preferred while retaining protocol 1.4 compatibility. No renderer batching change was required because SDK 0.9 selects and fence-protects descriptor tables below the existing interface. Fresh SDK, SDL, diagnostic, GemRB and bundle builds passed. The deployed diagnostic binary SHA256 is `fe8153e4f4279dba13c7d7e0293a5945b73c02cdf22d9c45fb69e04ae77f49cb`, its SDL SHA256 is `f5d4439886e7c348e4eb50801779ebf165e972622d1756d6dd4b3c3317640530`, and the deployed bundle SDL SHA256 is `7396d3a8cd2f3ce0ad012856fa59e34c2dfd58bed5e2e4ab9f66028d5eeff45d`. On the protocol-1.5 seed-13 core the diagnostic retained exact-pixel hash `64d5728e` and drained its audio tone. A four-batch core stress held 60.4fps. In the same Throne of Bhaal AR4000 combat workload, command-queue time fell from 24.1-26.5ms to 14.4-15.2ms per frame, sprite-batch stalls were zero and drains fell from approximately 25-27 per frame to about one. Active-combat intervals reached 15.9-17.8fps and GemRB used 41.8% of one Cortex-A9 core over 15 seconds versus the prior 48-50.5%. The user reached game over without a renderer fault; RSS was 371MiB, process swap stayed zero and 109MiB system memory remained available. The captured run log has SHA256 `35be51c2cb944b891f2e2a4932af4eb68879a2af222ccbdd68ac3b2e5bf38c0e`.

#### Next Steps:

Repeat AR4000 once with statistics disabled to quantify instrumentation overhead, add a supported Main input handoff so OSD access cannot strand GemRB without mouse and keyboard input, and capture a spell-heavy interval before deciding whether scaling or another shared drawing primitive is the next Noodles feature.

#### Files Modified:

- README.md
- scripts/env.sh

#### Status:

- [x] Built
- [x] Passed

---

## 14 COMMIT Unreleased c1f5c21 2026-09-24T10:38:00-07:00

#### Coming From:

Unreleased 50cf286

#### Purpose:

Give every Noodles game launch a supported Main input handoff and clean core session before profiling the remaining spell-animation cost.

#### Outcome:

Sources `cd0362d`, `8a64424` and `c1f5c21` added a Noodles wrapper that validates and reloads the protocol-1.5 RBF before starting GemRB, restricts normal use to Main's OSD Scripts terminal and logs launch progress. Local wrapper tests and bundle generation passed, but two hardware launches failed. The second persistent log proved that the wrapper ran on `/dev/tty2` and successfully requested the core load; Main then restarted for the new RBF, reacquired all four physical evdev devices and stranded the surviving launcher without input. The source built and deployed correctly but did not pass its hardware purpose.

#### Next Steps:

Replace the self-loading wrapper with the approved stock-Main MGL workflow: a single generic process will detach from an initial Scripts launch, watch for a Noodles MGL for 30 seconds, validate its engine and game data, use a temporary virtual keyboard for Main's normal framebuffer handoff, supervise the game and restore Main on exit.

#### Files Modified:

- README.md
- scripts/bundle.sh

#### Status:

- [x] Built
- [ ] Passed

---

## 15 COMMIT Unreleased 27c55d1 2026-09-24T11:18:22-07:00

#### Coming From:

Unreleased c1f5c21

#### Purpose:

Launch GemRB through a stock-Main Noodles MGL using one silent watcher and supervisor with validated game data and automatic input handoff.

#### Outcome:

Sources `aeccf9b`, `b68a754`, `540441e` and `27c55d1` add a deterministic static ARM coordinator, one silent OSD script and a Baldur's Gate II MGL without changing Main or the RBF. The coordinator detaches from the initial script, identifies the restarted Main and MGL through `/proc`, accepts only the qualified protocol-1.5 seed-13 RBF and registered GemRB metadata, validates the exact game-data directory and `CHITIN.KEY`, then uses a temporary virtual keyboard to open Main's OSD context and invoke its supported framebuffer handoff. It supervises GemRB, records bounded logs and returns Main to the core with F12 when the adapter exits or validation fails. The launcher also deduplicates multiple MiSTer mount aliases for the same USB partition so a silent launch can select the existing swap file. Strict native, analyzer and static ARM builds passed; the deployed launcher SHA256 is `2150da81b2b93c3186c695a527fe0939144a59558fd33abe87b1fe819fa085e1`, the generated script SHA256 is `8e7e6094491b7a6d9e755b4a949c3af8a7e706504457324441c53d78495a584e` and the MGL SHA256 is `8354fe8dfa8797256d2f7d0f33427fc3320fe3de9db420d9b8cfdf3368a03db6`. On hardware the one-second no-selection path exited cleanly, a deliberately missing data path was rejected after releasing all four physical input devices and Main reacquired them after the ten-second error interval, and the valid MGL reached the BG2 start menu with the Noodles renderer, music playback and the existing 384 MiB USB swap active while the coordinator remained as supervisor.

#### Next Steps:

Verify mouse and keyboard control plus HDMI music in the live MGL-launched game, then exit through GemRB and confirm automatic Main restoration and a second clean launch. Recheck OSD open-close behavior during gameplay; if stock Main exposes no usable close transition, scope the existing Noodles `OSD_STATUS` signal as the smallest two-build RBF follow-up before returning to spell-heavy profiling.

#### Files Modified:

- README.md
- scripts/bundle.sh
- scripts/deploy.sh
- scripts/release.sh
- tools/noodles-launcher.c

#### Status:

- [x] Built
- [ ] Passed

---

## 16 COMMIT Unreleased 96b1249 2026-09-24T19:04:00-07:00

#### Coming From:

Unreleased 27c55d1

#### Purpose:

Preserve the Noodles core display while handing Main's physical input devices to an MGL-launched game.

#### Outcome:

Sources `5bc7c86` and `96b1249` retain the watcher, validation and supervision architecture while replacing the valid-game framebuffer transition with the proven kernel `pidfd_getfd` path. The coordinator duplicates Main's four existing event descriptors, removes their exclusive grabs without closing them or covering the Noodles output, and restores the same grabs after GemRB exits; the temporary virtual keyboard remains limited to the visible validation-error path. The BG2 MGL now installs directly under Utility because Main did not expose its extra Noodles Games subdirectory on the test system. Strict native, analyzer and static ARM builds passed, the deployed launcher SHA256 is `aedd628e470e2e5584756224673e3eb1b6fd969803e2143138c3bbfb727a8d78`, and the generated MGL retained SHA256 `8354fe8dfa8797256d2f7d0f33427fc3320fe3de9db420d9b8cfdf3368a03db6`. Two valid launches reached the BG2 menu with visible Noodles output, working mouse and keyboard input and HDMI music; one exited normally with status zero, restored all four Main grabs and removed swap, and the user then launched the direct Utility entry, loaded the Throne of Bhaal save and entered combat.

#### Next Steps:

Reduce HPS memory pressure by releasing redundant CPU texture shadows while their current contents are resident in FPGA memory, recreating them only for updates, fallback access or eviction. Then repeat the same combat workload and compare RSS, swap traffic, CPU use and frame rate before returning to spell-heavy rendering work.

#### Files Modified:

- README.md
- scripts/bundle.sh
- scripts/deploy.sh
- scripts/release.sh
- tools/noodles-launcher.c

#### Status:

- [x] Built
- [x] Passed

---

## 17 COMMIT Unreleased 564c25c 2026-09-24T19:18:00-07:00

#### Coming From:

Unreleased 96b1249

#### Purpose:

Eliminate redundant HPS texture shadows that forced the live Baldur's Gate II combat workload into USB swap.

#### Outcome:

Sources `fb089f2` and `564c25c` make texture dimensions independent of CPU-shadow lifetime, release a non-pinned source shadow once the same current pixels are resident in FPGA memory, recreate current contents for later updates or locks, and retain that recreated shadow after repeated CPU access. Eviction still reads current FPGA contents before destroying residency, while the composition surface and active target retain their shadows. Renderer statistics now report current and peak shadow bytes plus allocation and release counts, and the diagnostic covers upload, release, partial update, later lock and eviction restoration. Fresh SDL, diagnostic, GemRB and bundle builds passed; the diagnostic binary SHA256 is `ed88d719b1de2fc56baa7364f6aacb9000591fc64893b68c0b8baef7b14e4219`, its SDL SHA256 is `be6b57e1a87483f1a17dab3a00604a70d091e78fd43cf20bdbafbdf2fa91708a`, and the deployed bundle SDL SHA256 is `6d8ab7ae560b9e399d4f4eeacfd42dbb52d494e0ac3451a56b4390ad551a7f4e`. On the protocol-1.5 seed-13 core the exact-pixel diagnostic passed with hash `fbd286bf` and its HDMI audio test passed. An initial release-after-every-upload policy held shadows at 1.8-2.0MiB and eliminated active swap traffic, but repeated dynamic-texture access caused hundreds of allocations and readbacks per interval and raised GemRB to 53.5% of one Cortex-A9 core. The adaptive policy stabilized shadows at 12.3-12.9MiB and reduced settled intervals to negligible allocation and readback traffic. With USB swap completely disabled, the Throne of Bhaal save loaded, sustained combat, played the game-over video and reached the game-over screen. Observed RSS peaked at 374MiB, Linux retained at least 107MiB available, process swap stayed zero, and no OOM occurred; a final mixed combat/video sample used 35.3% of one Cortex-A9 core. Combat ranged from 11.5 to 15.5fps as effects and game load varied, and the 12fps game-over video held its source rate. Two intervening runs reproduced the existing Kobold Commando projectile heap fault with exits 134 and 139, but both occurred with ample available RAM and clean launcher input restoration, independently of the renderer memory policy.

#### Next Steps:

Make no-swap operation the default for the tested Noodles MGL launch while retaining an explicit USB-swap override, then repeat the save and one area transition with statistics disabled. After that, profile a spell-heavy interval and address the largest remaining renderer or core cost rather than the resolved HPS shadow footprint.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 18 COMMIT Unreleased e7245ec 2026-09-24T13:25:27-07:00

#### Coming From:

Unreleased 564c25c

#### Purpose:

Make the hardware-proven Noodles Baldur's Gate II launch run without swap by default while retaining an explicit USB-swap fallback.

#### Outcome:

Source `e7245ec` makes the Noodles coordinator set `MISTER_SWAP=none` before starting its engine adapter, while the later per-installation `env.sh` source point can override it with `MISTER_SWAP=usb`; software-renderer launches retain their USB-swap default, invalid modes are rejected, and user, generated-launcher and release documentation distinguish the two paths. Strict native, GCC analyzer, static ARM, shell-syntax and bundle builds passed. The stripped deployed coordinator SHA256 is `1873ac97f7767eb534b7b59823e73b439978154c1ca3ead4347c742e496a319e`, the generated `run.sh` SHA256 is `ec97b4980b2b5a7ed80f29164a945eb4635b75a5d8968f406ee96fe08a8180f7`, and the MGL retained SHA256 `8354fe8dfa8797256d2f7d0f33427fc3320fe3de9db420d9b8cfdf3368a03db6`. With `env.sh` absent, hardware inspection proved that GemRB inherited `MISTER_SWAP=none` and `SDL_RENDER_DRIVER=noodles`, statistics were disabled and `/proc/swaps` remained empty. The autosave, combat, game-over video and game-over screen completed without a renderer, allocator or launcher fault; across a 163-second sample RSS rose from 179MiB to a 358MiB peak, Linux retained at least 113.6MiB available, process and system swap stayed zero, and GemRB used approximately 43% of one Cortex-A9 core. Forced test shutdown returned status 137 as expected, after which the supervisor restored all four Main input grabs.

#### Next Steps:

Capture a spell-heavy interval with statistics enabled to identify whether flagged sprites, scaling, uploads or a remaining software primitive dominates its visible slowdown, then implement the largest reusable Noodles acceleration opportunity. Keep the reserved-DDR swap-driver idea as a future fallback only if another supported engine exceeds Linux-visible RAM after adopting the same texture-shadow policy.

#### Files Modified:

- README.md
- scripts/bundle.sh
- scripts/release.sh
- tools/noodles-launcher.c

#### Status:

- [x] Built
- [x] Passed

---

## 19 COMMIT Unreleased e7245ec 2026-09-24T13:37:50-07:00

#### Coming From:

Unreleased e7245ec

#### Purpose:

Measure the statistics-enabled Noodles workload during the visible Throne of Bhaal spell-animation slowdown before selecting another shared accelerator feature.

#### Outcome:

The qualified protocol-1.5 seed-13 RBF and GemRB source `e7245ec` were profiled without swap during the same autosave combat path. Stable combat reached 18.9-19.4fps with 14.2-14.5ms in hardware queue submission, 30.9-31.4ms in statistics-serialized presentation and 6.5-7.0ms elsewhere. The user-marked spell interval fell to 11.4-11.5fps, but queue time increased by only 1.6-2.1ms while non-renderer time rose to 38.5-39.6ms, proving that most of the spell-specific dip is CPU-side GemRB animation or game work. Hardware still has a large reusable cost: every sampled frame spent approximately 11.5-11.9ms submitting roughly 500-560 opaque fills, and the single fill-command path caused one command-queue drain per frame. Draw submission took only 1.2-2.4ms, sprite-table stalls and CPU drawing fallbacks stayed zero, and texture synchronization during the marked interval used 3.1-3.5ms per frame. Over the 20-second marked sample GemRB used 49.7% of one Cortex-A9 core, RSS rose from 321MiB to 354MiB, Linux retained 111MiB available, and process and system swap remained zero. Forced test shutdown returned status 137 as expected, restored all four Main input grabs and removed the temporary statistics environment.

#### Next Steps:

Add ordered descriptor batches for opaque fills using the proven multi-table ownership model so the host can submit consecutive rectangles without one bridge transaction per fill, preserve exact interleaving with sprite batches and blended fills, and qualify the change with no more than two Quartus builds before repeating the same combat comparison. Treat the extra approximately 32ms of spell-only non-renderer time as a separate GemRB CPU investigation rather than attributing it to Noodles hardware.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 20 COMMIT Unreleased 4a41cb0 2026-09-24T14:12:59-07:00

#### Coming From:

Unreleased e7245ec

#### Purpose:

Use protocol-1.6 fill descriptors to collapse consecutive SDL opaque fills while preserving exact render ordering and protocol-1.5 fallback behavior.

#### Outcome:

Source `4be9ddd` pins MiSTer-Noodles `d1702b4` and its GitHub archive SHA256 `672fb6e71fd069bc7bd876527ad3e491fa3c937eb16297c5761976bfce5a7004`, detects `NOODLES_CAP_FILL_BATCH`, buffers up to 64 consecutive opaque rectangles for one target, and flushes them at every ordering boundary; source `4a41cb0` selects the accepted seed-13 image in the launcher and MGL. Existing scalar fills remain the fallback on older cores, and statistics distinguish rectangles, fill batches and table-pressure stalls. Fresh SDK, SDL, diagnostic, GemRB, launcher and bundle builds passed. Protocol 1.5 passed the extended diagnostic through scalar fallback, then protocol 1.6 RBF SHA256 `6b19b4a21e3f5fcfecd46558c9ba49c12f1056d87a5ed44bdfe7468f864d82b9` reported the expected identity and twice passed exact pixels with hash `787b0fbd`, including 70 fills across the descriptor boundary and mixed fill/draw ordering, while its audio queue drained. The deployed launcher SHA256 is `4d140dedc3313ed34e1e96760f8c71278879a459330e686d112768b2b819e794`, the MGL SHA256 is `d785835734498cb6da817ed582c423063b6eb9d873ecd295f6fede7d8b8a4ddd`, and SDL SHA256 is `0c29ebf44e81ff6a91408f5107e48a9cbfa34911c84ae0ea4e1ec84f52ab5526`. In AR4000, fill submission fell from 11.5-11.9ms to 1.25-1.76ms per frame and total queue time fell from 14.2-14.5ms to 3.7-5.0ms; stable combat reached 19.9fps, but spell intervals remained 10.4-13.4fps and the user reported similar stutter because non-renderer time rose to 28.9-62.6ms and presentation waits took 28.7-42.0ms. RSS was 332MiB with 125MiB system memory available and no swap, and no renderer or allocator fault occurred.

#### Next Steps:

Keep protocol 1.6 as the accepted Noodles baseline and investigate the spell-specific CPU and presentation path before adding another FPGA operation. Separate ordinary animation work from texture update/readback bursts, and compare statistics-disabled pacing so measurement serialization does not hide overlap; choose another generic accelerator feature only if that evidence identifies reusable render work.

#### Files Modified:

- README.md
- scripts/bundle.sh
- scripts/env.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-launcher.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 21 COMMIT Unreleased ebaa9e5 2026-09-24T14:52:46-07:00

#### Coming From:

Unreleased 4a41cb0

#### Purpose:

Separate texture synchronization and SDL command-construction costs from GemRB's spell-heavy frame time before choosing another accelerator change.

#### Outcome:

Source `ebaa9e5` adds opt-in five-second timing and call-volume summaries for Noodles texture creation, updates, locks, target changes, explicit readback and destruction plus SDL command-building callbacks. It also corrects the statistics documentation: presentation fence time includes completion of queued FPGA rendering and the vertical-blank handoff, while statistics-disabled operation uses the same synchronization with performance-clock reads and counters disabled. The SDL cross-build, renderer diagnostic build and complete bundle build passed; the diagnostic remains SHA256 `21595f3d89ef0542407d9059c0ecf06df4d1cb1ba69f54b66089ff430bf10a0e`, the unstripped SDL SHA256 is `c3cbaad9c4113711106b78ee8cd08f23952b1a7d5b66cd0c15e54afddc8b233a`, and the hash-verified deployed library SHA256 is `78c6ea2cd6d23951e9139d546eda676c4e967aa4a01586b8d9afa79ca20ee18d`. Spell-heavy AR4000 samples ran at 9.3-11.5fps: SDL command construction stayed at 1.08-1.27ms per frame, texture updates used 4.17-14.32ms, explicit texture locks and readbacks were absent, and creation, target changes and destruction were negligible except one 0.71ms destruction burst. After subtracting measured callbacks, 36-57ms per frame remained in GemRB and SDL's front end. Renderer submission varied from 5.75-15.02ms as heavy frames produced command-stream backpressure and 33-54 drains per five-second window, while presentation fence time was 27.8-30.0ms. The user reported unchanged stutter with statistics disabled, confirming profiler overhead is not causal. A controlled paused comparison then isolated viewport panning: continuous movement regenerated the viewport wall stencil as 3,646-5,483 tiny fills per frame in 60-89 batches, used 9.7-13.7ms per frame constructing 6,489-8,896 SDL commands and 12.1-16.9ms submitting the renderer queue, and ran at 10.8-13.0fps. The same scene stationary used 154 fills in five batches, 0.44-0.49ms of command construction, 1.67-1.89ms of queue time and held 20.0fps with no synchronization traffic. The path is GemRB's `DrawPolygonImp`, which calls `SDL_RenderDrawLine` separately for every pre-rasterized horizontal wall span whenever the viewport invalidates the stencil cache. Two later live-debug attempts were discarded: the known kobold projectile fault exited GemRB with signal 11 before attachment, and stopping a healthy process under GDB caused repeated fill-batch `EINVAL` errors until a clean core reload; normal untraced operation recovered with no renderer error.

#### Next Steps:

Replace GemRB's one-`SDL_RenderDrawLine`-per-wall-span path with one exact `SDL_RenderFillRects` batch per polygon and validate stationary and panning pixels plus the same paused comparison; this should remove thousands of SDL command objects without changing the already batched Noodles fill execution. Then add opt-in GemRB frame-phase timing for the remaining stationary spell cost before changing engine behavior or selecting another reusable FPGA operation.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [x] Built
- [x] Passed

---

## 22 COMMIT Unreleased f06343e 2026-09-24T15:42:59-07:00

#### Coming From:

Unreleased ebaa9e5

#### Purpose:

Coalesce compatible consecutive fill commands inside the Noodles SDL renderer to eliminate redundant command construction without changing GemRB.

#### Outcome:

Source `f06343e` leaves GemRB, the launcher and the RBF unchanged and gives only the Noodles SDL backend an internal opt-in path that appends consecutive rectangles to one compatible fill command when target, color, blend, viewport and clip state are unchanged. Fresh SDL and diagnostic builds passed, a clean-tree replay verified the SDL patch, and the expanded diagnostic twice produced exact framebuffer hash `93f8e614` for inclusive forward and reverse spans plus clipping, color, blend, viewport and draw-order boundaries; the deployed SDL SHA256 is `90d08e7fccfb942ccffc1c105c617b67b4b880de258a07bfb6c8fdd99c4c2c19`. In the paused Throne of Bhaal scene, stationary operation held 19.9-20.0fps and returned after movement with 2.91-3.23ms of renderer queue time, while continuous panning ran at 10.8-13.8fps with 11.3-15.1ms of queue time. The merge absorbed approximately 98-99% of 5,400-9,800 span-fill callbacks per panning frame without ordering errors, but command construction still took 9.5-15.5ms and the observed panning range remained close to the prior 10.8-13.0fps result, so eliminating command objects alone produced only a modest benefit.

#### Next Steps:

Keep the SDL coalescer as a correct generic reduction, profile the remaining stationary spell-animation cost in the current save without changing GemRB, and distinguish texture-update and game work from viewport regeneration. Consider a larger generic span-list submission only if later measurements show descriptor submission remains material after SDL profiling overhead is excluded, because the retained per-span SDL calls and hardware fill volume now dominate movement more than command-object traversal.

#### Files Modified:

- README.md
- patches/sdl2/noodles-renderer.patch
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 23 COMMIT Unreleased 2d409e3 2026-09-24T17:46:07-07:00

#### Coming From:

Unreleased f06343e

#### Purpose:

Measure GemRB's spell-heavy ARM instruction profile without stopping the process or modifying the engine.

#### Outcome:

Source `2d409e3` adds a static ARM `perf_event_open` sampler and host symbolicator that profile GemRB without stopping it or changing GemRB, SDL or the RBF. A two-second MiSTer smoke test collected 975 samples with zero loss while its target continued running, and build IDs for the executable, core library and SDLVideo plugin matched the deployed files. The valid paused capture collected 503 main-thread user samples over 8.02 seconds with zero loss; all GemRB threads used 1.82 CPU seconds over a separate 5.02-second window, equal to 36.3% of one core or 18.1% of the dual-core ARM. The valid combat capture collected 1,122 main-thread user samples over 12.02 seconds with zero loss; all threads used 5.58 CPU seconds over 12.04 seconds, equal to 46.3% of one core or 23.2% of the chip, while the main thread used 2.41 seconds in user mode and 2.03 seconds in the kernel. A whole-system combat sample was 66.4% busy, with the MiSTer frontend pinned to CPU 1 and consuming approximately one core. Combat user samples were 41.7% SDL2, 30.3% GemRB core plus SDLVideo and 20.4% libc; audio conversion accounted for 19.0% of all samples, while direct Noodles symbols were 14.2% and remained nearly flat at 13.25 samples per second versus 12.38 paused. The ARM therefore has substantial headroom and combat logic is not the frame-rate limit; the remaining evidence points to serialized presentation and waiting, consistent with the earlier 28-32ms `noodles_present_and_wait` interval on every frame.

#### Next Steps:

Keep the profiler as the non-stopping CPU diagnostic and investigate a generic one-frame asynchronous presentation path so CPU frame preparation can overlap FPGA rendering and vertical blank instead of blocking in `noodles_present_and_wait`. Preserve render ordering, buffer ownership and tear-free output, then compare paused and combat frame pacing directly against both the synchronous Noodles path and the original software renderer before considering lower-value audio conversion work.

#### Files Modified:

- README.md
- scripts/build-profiler.sh
- tools/noodles-perf-sampler.c
- tools/symbolize-perf-samples.py

#### Status:

- [x] Built
- [x] Passed

---

## 24 COMMIT Unreleased b404508 2026-09-25T03:17:01-07:00

#### Coming From:

Unreleased 2d409e3

#### Purpose:

Overlap GemRB's next-frame preparation with Noodles rendering and vertical blank by keeping one submitted presentation in flight.

#### Outcome:

Sources `159ce0f`, `fa21192`, `43f60aa`, `b77f77c` and `b404508` implement the SDL-only asynchronous path while leaving GemRB and the RBF unchanged. The renderer keeps one presentation in flight, submits safe following commands through Noodles SDK `973dfb6`, predicts back-buffer parity, defers managed texture uploads by rotating surfaces that remain in use, and resolves transient queue or descriptor pressure with bounded progress rather than a whole-stream drain. Native builds, the complete bundle build and the expanded MiSTer diagnostic passed; the diagnostic twice produced exact framebuffer hash `93f8e614` and audio passed. The menu reached its 30fps cap. In the controlled paused scene, texture rotation removed 30-38ms update waits and bounded retries reduced queue time from 21-24ms to 3.8-4.5ms with zero drains, but that time moved to a 32-35ms presentation wait and the scene remained at 20.1fps with unchanged visible stutter. Each frame issues about 0.96 million opaque-fill, 0.043 million blended-fill, 0.48 million plain-copy and 1.16 million flagged-draw pixel operations, so the experiment rules out remaining ARM submission serialization and identifies FPGA engine throughput as the active limit.

#### Next Steps:

Retain the asynchronous SDL path because it removes avoidable host serialization, measure the accepted RBF's fill, copy and blend rates independently, and use those rates with the captured frame mix to choose a generic RTL throughput improvement. Require exact pixels, audio and the same paused-scene timing before accepting the next RBF, and use combat only after the stationary bottleneck improves.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 25 COMMIT Unreleased 928fa97 2026-09-25T04:19:35-07:00

#### Coming From:

Unreleased b404508

#### Purpose:

Classify GemRB's accelerated draw workload by blend mode and rectangle size so the next reusable RTL throughput change targets measured traffic.

#### Outcome:

Source `928fa97` adds statistics-only classification of submitted draws by hardware path, blend mode, mirroring, modulation and pixel-area bucket, plus sparse alpha sampling and a one-time read-only capture of each unique large GPU-generated blend source. Fresh SDL, diagnostic and bundle builds passed; the final diagnostic library SHA256 is `ba9254cdef028e4628ec0e050aaf9217242a9ede3c1f99660a6d26fe2b37777e`, the deployed stripped SDL SHA256 is `8e8fd83fd00ef07e19490bc10c673c0be71f5301135db535df896f5040b77481`, and the exact-pixel diagnostic twice produced hash `93f8e614` with audio passing. Stable paused windows held 20.1fps and showed 0.48 million plain-copy pixels per frame, including only 0.058 million alignment reroutes, while about 1.15 million pixels were true standard alpha blends. Roughly 1.11 million blend pixels came from six large draws per frame. Full captures found repeated 640x480 sources entirely opaque, one 800x600 source with 86,400 opaque and 153,600 transparent pairs, another 800x600 source entirely transparent, large edge surfaces entirely opaque, and only one 279x281 source with 247 partial pairs among 39,059. Two full-screen fills account for about 0.96 million additional pixels per frame. The data rules out copy alignment and alternate blend modes as primary targets and shows that known alpha coverage can eliminate most destination reads and blend work. GemRB, command generation, the SDK protocol and the RBF remain unchanged.

#### Next Steps:

Add conservative alpha-state tracking to Noodles SDL render targets: full-surface writes establish opaque, transparent or unknown state; operations preserve a known state only when SDL alpha equations prove it; partial or unsupported writes invalidate it. Skip standard-alpha draws from known-transparent sources and submit known-opaque, identity-modulated standard-alpha draws through the plain copy path. Validate every transition and boundary in the exact-pixel diagnostic, then repeat this paused scene before considering the more complex RTL fallback that detects opaque and transparent pixel pairs dynamically.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [x] Built
- [x] Passed

---

## 26 COMMIT Unreleased cb02171 2026-09-25T04:48:46-07:00

#### Coming From:

Unreleased 928fa97

#### Purpose:

Eliminate provably redundant standard-alpha work by tracking conservative whole-texture alpha state inside the generic Noodles SDL renderer.

#### Outcome:

Source `cb02171` implements the SDL-only change, tracking whether a texture is entirely opaque, entirely transparent or unknown and preserving that state only where SDL's alpha equations prove the result. Standard-alpha draws from known-transparent sources become no-ops, while known-opaque identity-modulated draws use the plain copy path; partial, unsupported or ambiguous writes invalidate state. The one-time forced readback probe is removed, counters report skipped and converted pixels, and the diagnostic covers CPU updates plus GPU-generated full and partial alpha-state transitions. Fresh SDL, diagnostic and bundle builds passed; the diagnostic SHA256 is `0dacdabc22b84c9d0d09368b79778bd0c48e14f0cefaa0fc70ac1f7e667126c5`, the diagnostic SDL SHA256 is `f33d17109f105b3d7a5879b9429953305828af9330ec5bb51da7dcce40b9160e`, and the hash-verified deployed stripped SDL SHA256 is `77a420d5f976c4d40ecad04fab9fe381ebe7b338b0e3c9d808d55694d7b90d0a`. The expanded MiSTer diagnostic twice produced exact framebuffer hash `93f8e614` and audio passed. In the stable paused scene, about 0.15 million pixels per frame moved from blend to copy, but no whole draw could be skipped and the dominant 1.11 million large-source pixels remained unknown after partial render-target writes; stationary performance remained 20.0-20.1fps with a 32.6-34.0ms presentation wait. Sustained panning similarly remained about 19.7-20.0fps while high-motion windows still raised command construction to 3.6ms and queue submission to 16.4ms. The optimization is correct and reusable but does not materially change this workload. GemRB, the SDK protocol and the RBF remain unchanged.

#### Next Steps:

Retain conservative alpha state because it safely removes work when future consumers expose uniform surfaces, and implement the next optimization in the generic FPGA blend engine: for identity-modulated standard-alpha draws, inspect source pairs before destination access, skip fully transparent pairs and write fully opaque pairs without reading the destination, while sending partial pairs through the existing exact blend pipeline. Validate exhaustive pixels, stalls, alignment, mirroring and row edges before a timing-qualified RBF build, then repeat stationary and panning measurements before adding a larger span-list command.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [x] Passed

---

## 27 COMMIT Unreleased cd506c8 2026-09-25T08:17:15-07:00

#### Coming From:

Unreleased cb02171

#### Purpose:

Measure the loaded core's engine rates and PRESENT pacing and split GemRB's presentation wait before committing to an RTL throughput change.

#### Outcome:

The protocol-preserving MiSTer-Noodles binary-alpha source `eb5886f` had passed exact pixels and audio with an initial paused result of 20.9-21.6fps, but panning stayed at 16.6-17.9fps, combat fell to 12.1-14.5fps before the known Kobold Commando signal-11 fault, and a valid combat profile showed GemRB using only about 60% of one Cortex-A9 core, so the approved DDR3 write-burst proposal was deferred for measurement. Sources `91abd18` and `cd506c8` add `tools/noodles-throughput.c` to the renderer diagnostic package and statistics-only SDL pacing that times the raw completion count of the draws before a pending PRESENT, of the PRESENT itself and of the SDK's confirmation. The SDL, diagnostic and bundle builds passed with only the three existing SDL warnings; the deployed bundle SDL SHA256 is `8a0f17b4496e4da4545ad73e6a39b274db63cdce0727f7e7b78b7477e6e14f5b`, the diagnostic SDL SHA256 is `e6b6cf4454b5cc04895decf6bb9b26b9a2e14a5f17a4ed4e703e37f63b1474fc`, the throughput tool SHA256 is `b8367ccefd4e9d691392ec241b37830d9c5805c2f1ff9053b5cbaecc29a64088`, and the exact-pixel diagnostic produced hash `93f8e614` with audio passing. On the accepted protocol-1.6 image, now restored as `Noodles_fill_batch_seed13.rbf` with the candidate retained as `Noodles_alpha_fast2_seed13.rbf`, full-surface solid fills ran at 175 Mpix/s or 0.57 clocks per pixel, about 88% of the 64-bit 100MHz port's one-write-per-cycle ceiling, plain sprite-batch draws at 83 Mpix/s, standard-alpha blends at 69 Mpix/s regardless of source alpha and single-command `BLIT_COPY` at 28 Mpix/s, which the renderer does not use. The candidate raised plain draws to 88 Mpix/s, opaque-source blends by only 3% and transparent-source blends to 97 Mpix/s. Write bursts cannot exceed one beat per cycle, so they could recover at most about 12% of fill time, while blends write only half a word per clock and remain pipeline-limited. PRESENT behaves as ordinary vertical-sync double buffering: frame period is draw time rounded up to the next 16.6ms refresh with no additional retirement frame, but a command queued behind PRESENT starts only after the flip, and the SDK's confirmed wait trails raw completion by the remainder of the executing command rather than the whole queue. In GemRB's paused AR4000 scene the rate held 19.84-19.91fps with 6.9ms of submission, 19.2ms elsewhere and a 24.2ms deferred wait made of 5.9ms of remaining draws, 15.6ms of flip and vertical-blank wait and 2.7ms of confirmation, with only 5 of 100 frames drawn before the wait; each frame's commands therefore occupy the engine for about 35ms after the previous flip, just beyond the 33.3ms two-refresh budget, and the engine idles until the third refresh. The frame mix was about 0.97 Mpx of fills in 573 commands, 1.12 Mpx of plain draws and 0.53 Mpx of flagged draws, so plain copy now costs about 13.5ms at measured rates. The game-over screen behaved the same way at 19.8fps with 26.7ms of remaining draws. Combat ran at 9-14fps with frames drawn before every wait, 19-25ms of submission including 11-15ms of texture-update callbacks and 45-92ms elsewhere, and panning ran at 9-18fps with 10-27ms of SDL command construction, so neither is engine-bound. The rendering thread's user CPU was 13-32% throughout while it made about 100-330 voluntary context switches per frame, indicating substantial blocking outside the engine wait in combat. The thread-usage parser read `/proc/thread-self/stat` one field late, so the reported user share was always zero and the reported system share was actually user time; system time was not captured.

#### Next Steps:

Stationary scenes are engine-bound and quantized by PRESENT holding the command queue until vertical blank, so propose either a Noodles pacing change that lets later work proceed while a flip is pending, such as a third display buffer, which would allow about 29fps at the measured 35ms engine time, or a sprite-batch plain-copy throughput change that brings the frame under 31ms. Combat needs attribution of the main thread's blocked time before any renderer change, using corrected thread-usage accounting and off-CPU sampling of its wait states alongside texture-update fence waits. Record the write-burst proposal in MiSTer-Noodles as superseded by these measurements and obtain user approval for the selected plan before implementation.

#### Files Modified:

- README.md
- scripts/build-noodles-test.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-throughput.c

#### Status:

- [x] Built
- [x] Passed

---

## 28 COMMIT Unreleased 5b103be 2026-09-25T08:46:09-07:00

#### Coming From:

Unreleased cd506c8

#### Purpose:

Adopt the opt-in MiSTer-Noodles third display buffer so GemRB's next frame renders while the previous flip waits for vertical blank.

#### Outcome:

Sources `c30d29b` and `5b103be` pin MiSTer-Noodles `26acb9c`, whose protocol 1.7 and SDK 0.12 add a third display buffer and queued PRESENT, and make the SDL renderer enable three buffers whenever the core advertises them, with `SDL_RENDER_NOODLES_BUFFERS=2` keeping two, skip the presentation wait before back-buffer CPU transfers in that mode and log the buffer count. They also correct the thread-usage parser, run the throughput tool's pacing and behind-PRESENT tests in both modes, and point the launcher's exact RBF check and generated MGL at `pet/Noodles_triple_seed13.rbf`. SDL, diagnostic and bundle builds passed with only the three existing SDL warnings; the bundle SDL SHA256 is `87b8a6586cee6ea905b8efb58596ce4f9b877052a4aa1593e7c2f3d50e1b3dac`, the diagnostic SDL SHA256 is `db45abd606b08aa509de496c7bfd33291ee24a98e616e991ac8c03232ee7eb7f`, and the throughput tool SHA256 is `581c6fec8345fa946e9576e8e7216aba8263fd2400bd68a62d46c77b7bda57fa`. The first Noodles image, from `ca23af4`, still held later commands until each flip, which the throughput tool exposed; the corrected RBF SHA256 `ae0159da05a78b3209a6f3619173bb83cff84ca319d24c62159b2156a7e0fcd9` passed the exact-pixel diagnostic with hash `93f8e614` and audio in both modes, left engine rates unchanged, completed a fill behind a queued flip 2.76ms after acceptance and ran 35.5ms off-screen frames at 28.1fps instead of 20fps. In the AR4000 save, the paused scene rose from 19.84-19.91fps to 29.15-29.23fps with presentation wait near 0.02ms against GemRB's 30fps cap, uncontrolled panning windows ran at 15.8-27.3fps, the menu felt smoother and the user saw no ghosting. Spell-heavy combat instead ran at 7.8-10fps with every frame drawn before the wait, 88-117ms per frame outside the renderer, 18-21% main-thread user and 12-16% system CPU and 22-25 involuntary context switches per frame, and the user found it laggier than before; the known Kobold Commando projectile fault then ended GemRB with signal 11 while the core stayed loaded.

#### Next Steps:

Measure directly whether concurrent Noodles engine traffic slows ARM memory latency and bandwidth through the shared DDR3 controller, then run a controlled two- versus three-buffer combat comparison on this core, before choosing between controller priority, renderer pacing or read-only texture sources in board SDRAM. Capture the Kobold Commando fault with `MISTER_DEBUG=1`, because it now ends every combat measurement.

#### Files Modified:

- README.md
- scripts/bundle.sh
- scripts/env.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-launcher.c
- tools/noodles-throughput.c

#### Status:

- [x] Built
- [x] Passed

---

## 29 COMMIT Unreleased 00b3678 2026-09-25T09:50:40-07:00

#### Coming From:

Unreleased 5b103be

#### Purpose:

Measure whether Noodles engine traffic slows ARM memory access through the shared DDR3 controller.

#### Outcome:

Three-buffer presentation removed the engine's idle vertical-blank interval and lifted engine-bound scenes, but CPU-bound combat became slower while the renderer waited on nothing and the main thread's user time per frame rose, suggesting contention in the DDR3 device the HPS and FPGA share. Sources `1029d8d` and `00b3678` add `tools/noodles-contention.c` to the renderer diagnostic package: pinned to CPU 0 it measures dependent-load latency over a 64MiB chain and streaming read, write and copy bandwidth over 16MiB buffers while a thread on CPU 1 keeps the engine idle or busy with full-surface fills, plain draws, standard-alpha blends or a mix; the first build's copy figure was invalid because the compiler removed the unobserved copy, which `00b3678` corrects. The build passed without warnings and the tool SHA256 is `316b240ed0617c9be52d38ac67ba32e8dc61ff62c9f28aaa564c8767e059b106`. On the protocol-1.7 image with GemRB stopped, two runs agreed closely. With the engine idle the ARM measured 246-251ns latency, 695MB/s reads, 1,235-1,273MB/s writes and 393MB/s copies. Engine fills raised ARM latency 2.1-2.2 times and cut reads 42%, writes 54% and copies 48-49%; plain draws raised latency 1.9 times and cut reads about 40%, writes about 60% and copies about 51%; blends raised latency only 1.07-1.08 times and cut reads 4-6%, writes 32-35% and copies 16%; the mix raised latency 1.7-1.8 times and cut reads 32%, writes 56% and copies 45%. The contention is mutual, because engine fills fell from 175 Mpixel/s alone to 101-139 Mpixel/s while the ARM streamed. ARM impact follows the rate at which the engine issues memory requests more than its bytes moved, since blends move more nominal bytes than plain draws yet disturb the ARM far less. Three buffers keep the engine busy through most of each CPU-bound combat frame instead of idling for about 15ms at vertical blank, which is consistent with the slower combat, though the controlled two- versus three-buffer comparison remains outstanding.

#### Next Steps:

Obtain user approval for the next mitigation. Candidates are measuring HPS SDRAM controller port priority or weights with this tool before any change, removing redundant GemRB render-target clears because full-screen fills cause the largest ARM slowdown, pacing the renderer so the engine does not run a full frame ahead during CPU-bound scenes, and moving read-only texture sources to board SDRAM, which relieves reads but not fill writes. Run the controlled two- versus three-buffer combat comparison and capture the Kobold Commando fault with `MISTER_DEBUG=1`.

#### Files Modified:

- README.md
- scripts/build-noodles-test.sh
- tools/noodles-contention.c

#### Status:

- [x] Built
- [x] Passed

---

## 30 COMMIT Unreleased 3dd6632 2026-09-25T10:15:29-07:00

#### Coming From:

Unreleased 00b3678

#### Purpose:

Measure whether HPS SDRAM controller port arbitration settings protect ARM memory access from Noodles engine traffic.

#### Outcome:

Entry 29 showed engine fills and copies doubling ARM DDR3 latency. Reference records CVHPS-001 through CVHPS-005 establish from the Cyclone V HPS Technical Reference Manual that the MPU uses command ports 7 and 9 and FPGA traffic ports 0-5, that `mppriority`, `mpweight_0_4` to `mpweight_3_4` and `remappriority` set absolute priority, deficit round-robin weights and command-queue jumping, and that they take effect at run time. The live controller used the preloader values: every port at priority 0, FPGA ports at weight 16, MPU ports at 8 and L3 at 4. Source `3dd6632` adds `tools/mister-mpfe.sh`, which validates, applies, reads back and decodes the `default`, `equal`, `mpu`, `mpu-priority` and `mpu-remap` profiles until the next boot; `mpu-remap` extends the approved plan by also setting `remappriority` bit 1 for the MPU ports' priority, and every profile writes that register so `default` restores it. Under the mixed engine load, ARM latency was 1.74 times idle with `default`, 1.69 with `equal`, 1.60 with `mpu`, 1.54 with `mpu-priority` and 1.38 with `mpu-remap`; ARM read bandwidth rose from 68% of idle to 74%, 79%, 78% and 82%; and engine throughput fell from 60.8 to 55.7, 49.6, 48.1 and 37.5 Mpixel/s. Weight changes therefore recover bandwidth but little latency, because FPGA commands already in the controller's queue still precede the ARM; `mpu-remap` alone reduces latency materially but also cuts the ARM's own streaming writes and copies with the engine idle, from about 1,270 to 941MB/s and from 393 to 262MB/s. In GemRB with three buffers, profiles switched live with log offsets recorded: the paused AR4000 scene held 28.4-29.3fps with near-zero draw wait under `default` and `mpu`, while `mpu-remap` left 8-10ms of draw wait and 27.8-28.6fps. In the same continuous combat, first-round means were 20.8fps for `default`, 25.0 for `mpu` and 25.7 for `mpu-remap`, but the second round gave 22.5 for `default` and 21.8 for `mpu`, and within-profile windows spanned 17.0-27.5fps; this fight also ran far faster than the 7.8-10fps spell-heavy interval of entry 28. The arbitration effect is therefore smaller than combat's own variation and is not demonstrated in game. The party's death ended the fight, and the controller was restored to `default`.

#### Next Steps:

Leave the preloader arbitration unchanged, retaining `mpu` as the best synthetic trade-off at no paused-scene cost for a later controlled comparison. Attribute CPU-bound combat directly, since time outside the renderer dominates it, by profiling the main thread with the non-stopping sampler during a spell-heavy interval and distinguishing game logic, texture updates and blocking; then proceed with the approved formal-verification and pipeline-buffer cycles for the Noodles core.

#### Files Modified:

- README.md
- tools/mister-mpfe.sh

#### Status:

- [x] Built
- [x] Passed

---

## 31 COMMIT Unreleased 1eb84fc 2026-09-25T10:49:16-07:00

#### Coming From:

Unreleased 3dd6632

#### Purpose:

Run GemRB on the ARM core that the MiSTer frontend leaves idle instead of inheriting the frontend's CPU 1 affinity.

#### Outcome:

Profiling combat found the MiSTer frontend pinned to CPU 1 and nearly saturating it, while every GemRB and `noodles-launcher` thread carried the inherited `Cpus_allowed_list` of 1, CPU 0 stayed almost idle, and the main thread's scheduler statistics showed 206s running against 217s waiting to run. In one continuous fight with the three-buffer image and statistics enabled, moving the live process between CPUs every 15 seconds gave 17.4fps mean and 8.1fps worst on CPU 1 against 24.2fps mean and 20.8fps worst on CPU 0, CPU 0 won every paired interval, and the two 8fps collapses, with 106-109ms per frame outside the renderer and about 33 involuntary context switches per frame, occurred only on CPU 1. Source `1eb84fc` makes the generated `run.sh`, used by the Noodles and software launchers, move its own shell with `taskset` before starting GemRB or gdb, from `MISTER_CPUS` in `env.sh` with CPU 0 as the default; an unparsable value is reported and leaves the inherited affinity. The bundle built and deployed with `run.sh` SHA256 `320071b00105085197e61fef9a9a9b0083af1c55f837851bcc660c7b379943ec`, and the MiSTer's `taskset` applied `0` and `0-1` to child processes and rejected an invalid list as intended. At the user's request the run was repeated without any diagnostics: `SDL_RENDER_NOODLES_STATS` was commented out in the MiSTer's `env.sh`, no profiler, watcher or test tool ran, and the controller used its default arbitration. A normal Noodles launch placed every GemRB thread on CPU 0, and after 334s the main thread had run 97.9s and waited 19.0s, a wait-to-run ratio of 0.19 instead of 1.05. The user reported that everything ran much better, panning was almost perfect, the endgame video played almost perfectly and combat kept a few stutters; the session did not crash. Two changes were combined, so the gain cannot be divided exactly between affinity and disabled statistics, but the statistics mode is not negligible: its raw fence timing from `cd506c8` polls every 20us and produced 100-330 voluntary context switches per frame in earlier windows, which competed with GemRB on the shared core and inflated CPU-bound measurements. A later five-second sample showed one secondary GemRB thread running 2.8% and waiting 4.9% of CPU 0, consistent with a short periodic thread queued behind the main thread; the kernel does not expose `wchan` or kernel stacks. Earlier sessions ended three times through the known Kobold Commando heap fault with exit codes 134 and 139.

#### Next Steps:

Reduce the statistics mode's overhead before further CPU-bound measurement, in particular replacing the 20us raw-fence polling with sparse or coarser timing, and compare `MISTER_CPUS=0-1` with CPU 0 alone for the remaining combat stutter. Then capture the Kobold Commando heap fault with `MISTER_DEBUG=1` and `MISTER_MALLOC_CHECK=1`, and continue with the approved Noodles formal-verification and pipeline-buffer cycles; batched wall-polygon fills are lower priority now that panning is nearly smooth.

#### Files Modified:

- README.md
- scripts/bundle.sh

#### Status:

- [x] Built
- [x] Passed

---

## 32 COMMIT Unreleased c80f452 2026-09-25T11:00:34-07:00

#### Coming From:

Unreleased 1eb84fc

#### Purpose:

Reduce the Noodles statistics mode's raw-fence polling overhead without changing statistics-disabled rendering.

#### Outcome:

Source `c80f452` samples raw draw and flip completion on one of every ten presentation waits and increases the raw-count sleep from 20us to 0.5ms, while unsampled waits use the normal SDK fence path and the five-second log reports the sample count explicitly. SDL, the diagnostic package and the complete bundle built; the diagnostic SDL SHA256 is `1aee6f50f72055c7204f8e86cc47432265876c92cd5926dbdf37553394206bd3` and the deployed stripped SDL SHA256 is `92d6ba521d116451bcb50fd3b2ebdfde45a2afa9a3d5f920c04eda9bb995a49f`. On the protocol-1.7 seed-13 core the exact-pixel diagnostic passed with hash `93f8e614` and HDMI audio passed. In AR4000, statistics windows sampled about 9 of 90 waits and usually reported 18-63 voluntary context switches per frame, below the previous 100-330 range, but one window still reached 121. The following statistics-disabled control used about 42.6% user and 5.9% system CPU over a 20-second battle sample, and the user reported much better behavior: panning was almost perfect, the endgame video played almost perfectly and the game-over screen was reached without a renderer or allocator fault. The source is correct and reduces polling, but it did not pass its purpose because periodic raw-fence sampling still makes the measurement path visibly unlike normal play.

#### Next Steps:

Obtain user approval to remove raw-fence pacing probes from ordinary `SDL_RENDER_NOODLES_STATS=1` operation and place them behind a separate explicit diagnostic setting, leaving the normal statistics counters and SDK fence wait intact; then repeat the same statistics-on and statistics-off AR4000 comparison before continuing to the CPU-affinity test.

#### Files Modified:

- sdl-renderer/noodles/SDL_render_noodles.c
- README.md

#### Status:

- [x] Built
- [ ] Passed

---

## 33 COMMIT Unreleased 35409f1 2026-09-25T11:24:28-07:00

#### Coming From:

Unreleased c80f452

#### Purpose:

Remove raw-fence pacing probes from ordinary Noodles statistics while retaining them as a separately requested diagnostic.

#### Outcome:

Source `35409f1` makes `SDL_RENDER_NOODLES_STATS=1` use the same SDK presentation wait as normal play while retaining workload, callback, aggregate timing and scheduler counters, and moves the sampled raw draw and flip measurements behind the additional `SDL_RENDER_NOODLES_PACING=1` setting. SDL, the diagnostic package and the complete bundle built; the diagnostic SDL SHA256 is `4001089fb15f35395a20ecbd77904050580d6eb5b58d21e0e51b1ffd372c452c` and the deployed stripped SDL SHA256 is `ebbd9a711b29e3d17f924cb5b25b8976f5f0a6c0cdea06f82a2972c274edcb46`. The protocol-1.7 seed-13 core passed exact pixels with hash `93f8e614` and passed HDMI audio. Hardware logs confirmed that ordinary statistics reported raw pacing disabled, emitted no pacing samples and retained the separate thread metrics. The menu reached 28.5fps, and in AR4000 the user found combat and the endgame video effectively indistinguishable from the statistics-disabled control and reached the game-over screen, but panning felt between the earlier statistics-on and statistics-off runs. Panning and combat windows still made about 29,000-53,000 timed SDL queue callbacks per five seconds and used 14-17% system CPU versus 5.9% in the earlier statistics-disabled battle sample; video windows with only 213 callbacks used about 3% system CPU. Raw pacing is therefore isolated successfully, but ordinary statistics still do not fully pass because performance-clock reads around every queue callback measurably affect the callback-heavy panning path.

#### Next Steps:

Obtain user approval to retain queue-callback counts in ordinary statistics while moving only their per-call performance-clock timing behind a separate explicit callback-timing diagnostic, then repeat the same panning comparison before proceeding to the CPU-affinity test.

#### Files Modified:

- sdl-renderer/noodles/SDL_render_noodles.c
- README.md

#### Status:

- [x] Built
- [ ] Passed

---

## 34 COMMIT Unreleased 35409f1 2026-09-25T11:34:26-07:00

#### Coming From:

Unreleased 35409f1

#### Purpose:

Compare diagnostics-off GemRB gameplay with CPU 0 affinity against allowing both ARM cores before selecting the launcher default.

#### Outcome:

The deployed `35409f1` bundle and protocol-1.7 seed-13 core ran without renderer statistics, pacing probes, profilers or MPFE changes. With `MISTER_CPUS=0-1`, panning was usually nearly perfect but some sections slowed severely; during one 30-second sample the main thread migrated nine times, appeared on busy CPU 1 in 5 of 120 samples and spent 4.61 seconds waiting to run. Pinning every live thread back to CPU 0 removed migration but made battle more stuttery. A hybrid with only the main thread fixed to CPU 0 and existing secondary and audio threads allowed on both cores made panning perfect, but the known Kobold Commando fault ended the process before combat qualification: immediately after repeated `SHOOT` activity, glibc reported `malloc(): unaligned tcache chunk detected`, GemRB exited 134 and the launcher restored all four Main input grabs. Unrestricted `0-1` is rejected and CPU 0 remains the launcher default; the hybrid remains an uncommitted experiment until correctness work permits a complete comparison.

#### Next Steps:

Capture the reproducible Kobold Commando heap corruption with the existing debug-symbol and allocator-checking path under the CPU 0 default, identify the earliest failing allocation and stack, and defer any hybrid-affinity implementation until the same battle can complete reliably.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 35 COMMIT Unreleased 35409f1 2026-09-25T11:41:35-07:00

#### Coming From:

Unreleased 35409f1

#### Purpose:

Capture the reproducible Baldur's Gate II Kobold Commando heap corruption at its earliest detectable failure with matching debug symbols.

#### Outcome:

The accepted CPU 0 affinity, protocol-1.7 seed-13 core, default MPFE arbitration and statistics-disabled renderer reproduced the fault under matching debug symbols and glibc allocator checks. The first capture stopped on `malloc(): smallbin double linked list corrupted`; its raw stack showed that allocation detected earlier heap damage while resolving `SPWI214`, not that the resource lookup caused it. A temporary ARM AddressSanitizer build then reproduced the Kobold failure as a main-thread heap use-after-free at `Actor::UpdateActorState` line 7866. The 76-byte `Animation` was allocated by `AnimationFactory::GetCycle` for `currentStance`, then freed when the last ranged charge passed through `Actor::ChargeItem`, `Inventory::BreakItemSlot`, `Inventory::UpdateWeaponAnimation`, `Actor::SetUsedWeapon` and `CharAnimations::DropAnims`; `UpdateActorState` subsequently read `first->endReached` through the stale raw pointer. This conclusively excludes the FPGA renderer, SDL, audio and CPU affinity. The ASan report is preserved locally as `work/asan-kobold-35409f1.log` with SHA256 `a93982920ef2bee34a081d0713a38408866f324b1c286e93cc3adcdf8b9ffc2e`, and the normal `35409f1` bundle and original target environment were restored after capture.

#### Next Steps:

Propose a narrow backport of the relevant part of upstream GemRB commit `6041dd8c`: retain the stance animations with `Holder<Animation>` instead of borrowing raw pointers from the cache, update the affected local uses, build normally and with AddressSanitizer, then repeat the same depleted-ammunition Kobold battle before resuming performance work.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 36 COMMIT Unreleased 6bcd48e 2026-09-25T12:10:41-07:00

#### Coming From:

Unreleased 35409f1

#### Purpose:

Backport the upstream GemRB animation-ownership correction that prevents depleted ranged ammunition from leaving `Actor::currentStance` with freed animation pointers.

#### Outcome:

Source `6bcd48e` adds a narrow GemRB 0.9.5 patch derived from the `Actor.h` and `Actor.cpp` portion of upstream commit `6041dd8c`. It replaces raw `Animation*` entries in `currentStance` with existing managed `Holder<Animation>` references and adjusts eight affected local ownership or access sites, without importing the remainder of upstream's sixteen-file animation-lifetime refactor or altering SDL, the Noodles renderer, FPGA logic, audio, affinity or launcher behavior. The patch applied cleanly and idempotently, the normal bundle and matching ARM AddressSanitizer diagnostic built successfully with only existing alignment warnings, and the normal bundle was deployed with core-library SHA256 `9cef1e56fdb017b0e10ef8e6bac8eb774c97b48a65d85086e0c045c1322da514`; the unchanged deployed SDL SHA256 remains `5d76ce7c30287bd55734cd63aa126406cc6bbd2d77bfc8c376f8dfa6547e00ba`.

#### Next Steps:

Repeat the depleted-ammunition Kobold battle with the deployed normal build; if normal gameplay completes, deploy the matching sanitizer build for one final reproduction to prove the captured use-after-free is absent, restore the normal bundle, and then resume performance work.

#### Files Modified:

- patches/0004-gemrb-retain-current-stance-animations.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 37 COMMIT Unreleased c904d9a 2026-09-25T12:46:37-07:00

#### Coming From:

Unreleased 6bcd48e

#### Purpose:

Prevent depleted ranged ammunition from force-evicting a shared cached item definition while other actors retain weapon-header pointers into it.

#### Outcome:

The normal `6bcd48e` build completed the previously crashing Kobold battle and reached the game-over screen, confirming that its 76-byte `Animation` use-after-free was removed. The matching AddressSanitizer run then exposed a separate main-thread heap use-after-free: `Actor::GetCombatDetails` read `ITMExtHeader::THAC0Bonus` at offset 28 in an exactly 72-byte freed `ITMExtHeader` after a Kobold exhausted its ammunition. Static tracing showed that `Inventory::BreakItemSlot` uniquely force-evicted the globally shared item resource even though other actors could retain raw `WeaponInfo::extHeader` pointers into that resource. Source `c904d9a` changes that release to the normal non-evicting cache path and documents the lifetime constraint, preserving the existing weapon-header contract without retaining ammunition instances or changing renderer, FPGA, audio, affinity or launcher behavior. Patch application and reverse-application checks passed, and normal and ARM AddressSanitizer builds completed with only the existing SDL alignment warning. The staged normal core-library SHA256 is `19dbda6e6de5ed717fcb93e2cf247118f422fde1e01bc1bda9254c024d467e3e`, the sanitizer core-library SHA256 is `9c87466c65135d848ed463ddd08b67dc2a146a4a33c6f367ab859da2a06c46d5`, and the sanitizer core build ID is `07eb6a104138de34f725d46bddb9aeaae22c375d`.

#### Next Steps:

Deploy the ARM AddressSanitizer bundle with USB swap, repeat only the depleted-ammunition Kobold battle to catch this or any subsequent memory error at its first invalid access, then preserve any report and restore the staged normal build without requiring a second normal battle repetition.

#### Files Modified:

- patches/0005-gemrb-preserve-cached-weapon-items.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 38 COMMIT Unreleased c904d9a 2026-09-25T13:12:50-07:00

#### Coming From:

Unreleased c904d9a

#### Purpose:

Qualify the shared weapon-item cache correction through depleted-ammunition combat under ARM AddressSanitizer and restore the normal target afterward.

#### Outcome:

The correctly packaged sanitizer build, with core-library SHA256 `fb07c059e587788222a201ff686628fcdc4d004c5a84625f40a35d0ba9c941df` and build ID `07eb6a104138de34f725d46bddb9aeaae22c375d`, loaded the original AR4000 save with a 384 MiB USB swap file. Multiple Kobold Commandos reached zero ammunition, continued evaluating and performing attacks beyond the former `ITMExtHeader` failure point, and the user completed combat, the endgame video and the game-over screen without an AddressSanitizer report. The clean game log is preserved locally as `work/asan-cachefix-pass-c904d9a.log` with SHA256 `2c9c3294f36bc7efa774021d244360666aa4ebeedb7afa0420f2584e301a8cbd`. After the completed test the user reset the FPGA core while GemRB remained at its main menu, so the still-audible diagnostic process was deliberately terminated and its resulting exit 137 is not a test failure or an out-of-memory event. The normal bundle was restored with core-library SHA256 `19dbda6e6de5ed717fcb93e2cf247118f422fde1e01bc1bda9254c024d467e3e` and SDLVideo SHA256 `5d76ce7c30287bd55734cd63aa126406cc6bbd2d77bfc8c376f8dfa6547e00ba`; sanitizer files, diagnostic environment settings and active swap were removed.

#### Next Steps:

Resume the deferred diagnostics-off performance work on the normal `c904d9a` build, beginning with the narrowly scoped hybrid CPU-affinity comparison whose panning result was promising but whose combat qualification had previously been blocked by the Kobold crash.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 39 COMMIT Unreleased c904d9a 2026-09-25T13:17:54-07:00

#### Coming From:

Unreleased c904d9a

#### Purpose:

Qualify the deferred hybrid CPU-affinity layout on the crash-corrected normal build before changing the launcher default.

#### Outcome:

The approved diagnostics-off comparison retains GemRB's main thread on CPU 0 while allowing only its already-created non-main threads to run on CPUs 0-1. It uses the normal `c904d9a` bundle and the accepted protocol-1.7 seed-13 core with renderer statistics, pacing probes, profilers and swap disabled. This is a reversible runtime test that changes no source or deployed files and preserves CPU 0 as the launcher default until the result is accepted.

#### Next Steps:

Launch AR4000 normally, apply and verify the hybrid masks after GemRB has created its steady-state threads, and compare panning, depleted-ammunition Kobold combat, the endgame video and the game-over transition with the CPU 0 baseline; then either reject the layout and retain the existing default or propose a deterministic launcher implementation.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 40 COMMIT Unreleased c904d9a 2026-09-25T13:23:16-07:00

#### Coming From:

Unreleased c904d9a

#### Purpose:

Record the completed hardware comparison of hybrid GemRB thread affinity against the CPU-0-only launcher default.

#### Outcome:

On the normal diagnostics-off `c904d9a` build, GemRB's main thread remained fixed to CPU 0 while its four non-main threads, including `SDLAudioP2`, retained CPU 0-1 masks throughout the test. The user reported that panning was essentially perfect and that combat, loading and scene changes all ran better than with every thread restricted to CPU 0, although combat still stuttered occasionally. The depleted-ammunition Kobold encounter completed without a crash, the enemies were defeated and the game loaded the next area successfully. The hybrid layout therefore passes this comparison and supersedes CPU-0-only as the preferred affinity design, while unrestricted migration of the main thread remains rejected.

#### Next Steps:

Obtain approval to implement the accepted hybrid layout deterministically in the launcher, ensuring that the main GemRB thread remains on CPU 0 while current and subsequently created non-main threads may use CPUs 0-1, then rebuild and repeat a short panning, combat and area-transition regression without diagnostic instrumentation.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 41 COMMIT Unreleased c904d9a 2026-09-25T13:25:51-07:00

#### Coming From:

Unreleased c904d9a

#### Purpose:

Determine whether the hybrid-affinity session ended because the no-swap target exhausted Linux-visible memory.

#### Outcome:

After the successful Kobold battle and next-area load recorded in Entry 40, the kernel OOM-killed GemRB PID 2478 at 20:25:01 UTC. The kill record reports 602416 KiB total virtual memory, 463496 KiB anonymous resident memory, 244 KiB file-backed resident memory and 520 KiB of page tables on a target with 502864 KiB total Linux-visible RAM and no active swap; `run.sh` consequently recorded exit 137. The subsequent 461628 KiB available-memory reading reflects memory reclaimed after the kill, not remaining capacity at the failure. This does not reverse the completed hybrid performance comparison or resemble either corrected Kobold use-after-free, but the overall no-swap session did not pass sustained gameplay and FPGA presentation has not eliminated GemRB's peak host-memory requirement.

#### Next Steps:

Keep the affinity and memory findings separate, and obtain approval either to enable the existing USB swap path for the short hybrid implementation regression or to defer affinity implementation while a dedicated low-overhead memory-growth investigation identifies whether the approximately 463 MiB resident peak is reclaimable cache, area-lifetime retention or required AR4000 working data.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 42 COMMIT Unreleased 8b0c829 2026-09-25T13:35:54-07:00

#### Coming From:

Unreleased c904d9a

#### Purpose:

Implement the accepted hybrid CPU-affinity layout deterministically in the launcher so GemRB's main thread stays on CPU 0 while every other GemRB thread may use CPUs 0-1.

#### Outcome:

Threads inherit the affinity of the thread that creates them, so pinning the main thread alone would also confine every later thread to CPU 0. Source `8b0c829` adds `tools/mister-affinity.c`, built into the bundle as `libs/libmister-affinity.so`. When preloaded it records the process mask applied by `run.sh`, pins the main thread in its constructor to `MISTER_MAIN_CPUS`, default CPU 0, and wraps `pthread_create` so each new SDL or GemRB thread, including threads created by other threads, restores the recorded process mask before running its start routine. It reports the applied masks once on standard error, adds no watcher process or polling, and wraps nothing when the two masks are equal; an invalid `MISTER_MAIN_CPUS` or one outside the process mask leaves every thread on the process mask with a message. `run.sh` now defaults `MISTER_CPUS` to `0-1`, preloads the library in the normal path and after any heap-checking library in the `MISTER_DEBUG` path, and appends the swap-in and swap-out page counts accumulated during the run to `last-run.log`; `MISTER_CPUS=0` reproduces the previous CPU-0-only layout. Renderer, FPGA, audio and GemRB sources are unchanged. A host build of the library confirmed the main, child and grandchild masks for pthread and `std::thread` creation with default, custom, out-of-range and invalid main-CPU lists. The bundle built with the library warning-free under `-Werror`; its SHA256 is `25418a29743d4f23e8b8e8cc8602bf34ca10c57b5ae4e0b22feaef087097487c` and `run.sh` is `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`, while the core library `19dbda6e6de5ed717fcb93e2cf247118f422fde1e01bc1bda9254c024d467e3e` and SDLVideo `5d76ce7c30287bd55734cd63aa126406cc6bbd2d77bfc8c376f8dfa6547e00ba` match the accepted `c904d9a` normal build. Deployment was not possible because the MiSTer at 10.10.0.22 did not answer ping or ssh.

#### Next Steps:

When the MiSTer is reachable, deploy the bundle and set `MISTER_SWAP=usb` in its `env.sh` for this regression only, because Entry 41 showed the no-swap target reaching the OOM killer after the next area loaded. Verify the main and non-main thread masks once after startup, then repeat a short diagnostics-off panning, depleted-ammunition Kobold combat and area-transition regression and record smoothness together with the swap counts from `last-run.log`. The memory-growth investigation follows as a separate cycle.

#### Files Modified:

- tools/mister-affinity.c
- scripts/bundle.sh
- README.md

#### Status:

- [x] Built
- [ ] Passed

---

## 43 COMMIT Unreleased 8b0c829 2026-09-25T13:48:30-07:00

#### Coming From:

Unreleased 8b0c829

#### Purpose:

Qualify the launcher's deterministic hybrid CPU-affinity layout on hardware with the USB swap safety file enabled.

#### Outcome:

The target had rebooted and is now at 10.10.0.21. Its last pre-reboot session, from 20:28 to 20:34 UTC on the old `c904d9a` bundle, saved a game and then logged two Noodles draw-batch timeouts and exit 139 as the FPGA and Main were reset under it; the user directed that this reset-induced exit be disregarded. The `8b0c829` bundle was deployed with all device hashes matching the build, and `MISTER_SWAP=usb` was added to the device `env.sh` for this regression. A normal Noodles launch at 20:43 UTC logged `mister-affinity: main thread CPUs 0, other threads CPUs 0-1`, and a single check found main thread 1616 on CPU 0 and its four non-main threads, including `SDLAudioP2`, on CPUs 0-1, the same layout that passed in Entry 40; the masks were unchanged when checked again after the test. With no diagnostics running, the main thread had run 73.7s against 14.5s waiting to run, the process's resident peak was 367520 KiB with about 118 MiB still available, and 284 pages were swapped out with none swapped in while the process itself held no swapped pages, so paging did not affect smoothness. The inspected log for that process shows the AR4000 load and battle music without errors other than the usual missing optional resources, but no subsequent area load; the user independently verified panning, the depleted-ammunition Kobold combat, the area transition and further progress into the game and reported that everything passed. GemRB was still running at the time of recording, so `last-run.log` holds no exit record for it. The temporary `MISTER_SWAP=usb` line was then removed from the device `env.sh`, restoring the Noodles no-swap default for later launches.

#### Next Steps:

Keep the hybrid layout as the launcher default. Begin the separate low-overhead memory-growth investigation from Entry 41, determining whether the approximately 463 MiB resident peak that reached the OOM killer without swap is reclaimable cache, retention across area lifetimes or required working data, and whether the Noodles path should keep its no-swap default.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 44 COMMIT Unreleased f4b56eb 2026-09-25T14:11:21-07:00

#### Coming From:

Unreleased 8b0c829

#### Purpose:

Make the Noodles SDL renderer clear and blend only the written region of mostly transparent render targets, without changing GemRB or any rendered pixel.

#### Outcome:

With `DrawFPS=1`, a stationary AR0015 scene on the `8b0c829` bundle showed about 27fps, and a ten-second sample of per-thread kernel counters showed GemRB's main thread running only about 42% of the time while it slept in Noodles fence waits, so the scene is limited by FPGA drawing at roughly 37ms per frame rather than by ARM time. GemRB 0.9.5 clears its full-screen transparent HUD buffer with `SDL_RenderClear` every frame and composites every window buffer with an `SDL_BLENDMODE_BLEND` copy, so an HUD holding little more than the cursor still costs an 800x600 fill and an 800x600 blend, about 2.7ms and 7ms at Entry 27's engine rates. Following the user's direction that MiSTer-Noodles serve as a general SDL accelerator while games stay close to upstream, source `f4b56eb` changes only `sdl-renderer/noodles/SDL_render_noodles.c` and the renderer diagnostic. Each managed texture records a rectangle outside which every pixel equals one known value. An unblended clear or fill to that value covering the rectangle writes only the rectangle, or nothing when it is empty, provided the FPGA copy is current, while any other full clear resets the rectangle and value. An unmirrored `SDL_BLENDMODE_BLEND` or `SDL_BLENDMODE_ADD` copy from a texture whose known outside value has zero alpha copies only its intersection with the rectangle, except where an opaque source is converted to an unblended copy. Blended fills, copies, points, whole line segments, CPU updates, locks and scaled or self copies grow the rectangle, geometry makes it the whole texture, and the display composition target is never tracked because its hardware buffers rotate. Statistics mode reports trimmed draw and fill pixels. The diagnostic adds twelve transparent-overlay frames covering reduced and skipped clears, blended fills, sprite copies, points, a diagonal line, a CPU update, geometry, a scaled copy, transparent non-black, opaque and restoring clears, and BLEND and ADD composites with modulation, a partial source, a clip and a mirror; the first pass checks each overlay's untouched pixels against its clear colour and the second replays the frames without intermediate readbacks, both compared per pixel against a CPU reference. SDL built with only the three existing warnings and the diagnostic without warnings; the diagnostic SDL SHA256 is `c39fd8093bb139f726b9aa0175583c845c54d79df66048af2f007fa42da2d411`, the diagnostic binary `945096d3544e3cd68d695f3136047f3fb3504cefe19554bf8c7e8817215e3979` and the bundle SDL `d8c07a8021858d25ffca7ab02bc4149666ccf4da33aa3972fa88b9d01913bccf`, with all other bundle files unchanged. On the loaded protocol-1.7 core with GemRB stopped, the new diagnostic passed against both the previous renderer, validating the reference model, and the new renderer, each with hash `93f8e614` and HDMI audio passing. Two deliberately broken renderer builds, one skipping the reduced clear and one offsetting trimmed copies by a pixel, were each rejected by the existing alpha-state checks and, with those checks removed in a temporary uncommitted binary, independently by the new sequence at frame 1's stale pixel and the offset composite. The bundle was deployed with USB swap and `DrawFPS=1` retained.

#### Next Steps:

Test the same stationary AR0015 scene with `DrawFPS=1` and diagnostics off against the 27fps baseline, followed by panning, combat, menus, dialogue and videos for visual correctness, particularly cursor, tooltip and window-frame drawing.

#### Files Modified:

- sdl-renderer/noodles/SDL_render_noodles.c
- tools/noodles-render-test.c

#### Status:

- [x] Built
- [ ] Passed

---

## 45 COMMIT Unreleased f4b56eb 2026-09-25T14:39:01-07:00

#### Coming From:

Unreleased f4b56eb

#### Purpose:

Record hardware qualification of the Noodles renderer's written-region clearing and blending and the resulting fog-of-war workload measurement.

#### Outcome:

With the `f4b56eb` bundle, `DrawFPS=1`, USB swap and diagnostics off, the stationary AR0015 scene that had shown about 27fps reached GemRB's 30fps cap, and the user found the cursor, tooltips, window borders, menus and videos visually correct. Panning into explored-but-unseen areas still dropped occasionally to 28fps. Kernel counters during such a view showed the main thread running about 42% of the time with no swap activity and no new log errors, so the limit remained FPGA drawing. GemRB's `FogRenderer` darkens those areas with rows of half-transparent black rectangles, plus blended edge sprites under `SpriteFogOfWar=1`, which the renderer submits as Noodles blended fills. A statistics-enabled relaunch, which does not alter FPGA workload, produced 39 five-second windows in AR0015. Heavy fog added up to 0.44 Mpx of blended fills per frame, close to a full 800x600 screen. The trimmed HUD area still cleared and blended each frame varied from 0.014 to 0.30 Mpx with the cursor position, because GemRB draws both its FPS counter at the top left and the cursor into the HUD buffer and the renderer's single bounding rectangle spans both. Estimated engine time at Entry 27's rates, with an assumed 69 Mpixel/s blended-fill rate, was 22-30ms per frame, leaving little of the 33.3ms two-refresh budget in heavy fog when the cursor is far from the counter. Statistics were disabled again in the device `env.sh` afterwards.

#### Next Steps:

The user chose to investigate raising MiSTer-Noodles engine throughput next, particularly for blended fills, copies and blends, following that repository's own project policies. Renderer-side candidates remain available for later approval: tracking several content rectangles per texture so separated overlay elements do not merge into one large area, and dropping a display clear that the next opaque full-screen copy overwrites.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 46 COMMIT Unreleased de2dfd4 2026-09-25T19:02:04-07:00

#### Coming From:

Unreleased f4b56eb

#### Purpose:

Load the Noodles core from its release name so a new core build is deployed by overwriting one file without launcher or MGL changes.

#### Outcome:

The launcher accepted only pet/Noodles_triple_seed13.rbf and the MGL named that file, so every new core build needed a launcher and bundle change. At the user's direction, source de2dfd4 installs the core under the MiSTer release convention beside the game MGL: the MGL names _Utility/Noodles without a date, the launcher accepts only Noodles.rbf or Noodles_YYYYMMDD.rbf directly in /media/fat/_Utility, and deploy.sh core installs a given RBF as _Utility/Noodles_<date>.rbf after removing any earlier Noodles core there, printing both hashes. A host test of the path check accepted the dated and undated release names in any letter case and rejected the old pet path, per-build names, malformed dates and subdirectories. The launcher and deploy script built cleanly, and the bundle built with launcher SHA256 7b2cea4abf6cb7f5af6d218d4963ae112513082ffe2b8ba7b917c910a1e14833 and the unchanged renderer SDL d8c07a80. The bundle was deployed, and MiSTer-Noodles d5051f4, the unqualified 120MHz seed-3 core with RBF SHA256 fe858c3fce82ac17cb867485bf66c0627e5219c6cdd6f351dc7478364399a0c3, was installed as _Utility/Noodles_20260925.rbf with a matching device hash. That core passed the exact-pixel diagnostic and audio in three-buffer mode, which GemRB uses, but fails a deferred texture update in two-buffer mode that the accepted 100MHz core passes; the user chose to try it in the game first. The temporary pet/Noodles_c120_seed3.rbf copy was removed. USB swap and DrawFPS=1 remain set.

#### Next Steps:

Build and deploy the bundle and the core, then have the user compare the AR0015 fog and panning scenes and combat with DrawFPS=1, USB swap and diagnostics off against the 25-30fps seen on the accepted core. Whatever the result, investigate the two-buffer deferred-update fault before accepting any 120MHz core.

#### Files Modified:

- tools/noodles-launcher.c
- scripts/bundle.sh
- scripts/deploy.sh
- README.md

#### Status:

- [x] Built
- [ ] Passed

---

## 47 COMMIT Unreleased 5fe8719 2026-09-25T21:09:38-07:00

#### Coming From:

Unreleased de2dfd4

#### Purpose:

Preserve GemRB's intended game-state update cadence when rendered frames do not divide evenly into the simulation tick or a combat frame finishes late.

#### Outcome:

Source `5fe8719` adds one GemRB v0.9.5 patch that advances the main-loop game-state deadline by its fixed 66ms interval after each update instead of resetting it to the current wall-clock time, preserves the fractional remainder and bounds accumulated backlog to one tick after a long stall. A deterministic 60-second model reproduced 600 updates at a 20fps render cadence with the old scheduler and produced 909 with the correction, equal to the corrected 30fps case; the long-stall case retained no more than one 66ms tick. All six project patches applied cleanly to an untouched v0.9.5 tree, and the normal ARM build and bundle completed successfully. The staged core library SHA256 is `7b77e70c255df20495cea46a83a5b8e670ac3392cade1697a0543ec88098d582`. The bundle was not deployed because GemRB PID 1304 remains active on the target with `CapFPS=20`; renderer, audio, affinity, game rules and FPGA behavior are unchanged.

#### Next Steps:

After the active GemRB session exits, deploy the staged normal bundle without changing the current 20fps configuration or the loaded FPGA core, then repeat the same combat sequence and compare visible animation cadence. If combat still stutters, restore the normal 30fps cap before attributing the remainder to rendering, because 20fps presentation cannot display every 15Hz simulation interval evenly.

#### Files Modified:

- patches/0006-gemrb-preserve-game-tick-cadence.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 48 COMMIT Unreleased 5fe8719 2026-09-25T21:18:13-07:00

#### Coming From:

Unreleased 5fe8719

#### Purpose:

Deploy the game-tick cadence correction for the controlled 20fps combat comparison without changing the loaded FPGA core or diagnostic settings.

#### Outcome:

After the user rebooted the MiSTer, no GemRB process was active and the normal `5fe8719` bundle was deployed. Device hashes match the staged core library `7b77e70c255df20495cea46a83a5b8e670ac3392cade1697a0543ec88098d582`, executable `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d` and unchanged SDLVideo plugin `5d76ce7c30287bd55734cd63aa126406cc6bbd2d77bfc8c376f8dfa6547e00ba`. The preserved target configuration has `CapFPS=20`, `DrawFPS=0`, renderer statistics disabled and USB swap enabled. The canonical MGL still names `_Utility/Noodles`, and the deploy did not touch the existing Noodles RBF with SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`.

#### Next Steps:

Repeat the same combat sequence and assess attack and hit animation cadence at the 20fps cap. If cadence is smoother, restore the normal 30fps cap for a final combat comparison; if it is unchanged, use the existing capture to separate the remaining render-queue spike from simulation scheduling without repeating the earlier broad profiling cycles.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 49 COMMIT Unreleased 5fe8719 2026-09-25T21:24:36-07:00

#### Coming From:

Unreleased 5fe8719

#### Purpose:

Record the 20fps cadence-fix result and distinguish the observed MVE video stutter from the remaining combat presentation stutter.

#### Outcome:

The user reported that combat looked unchanged with the `5fe8719` cadence correction at `CapFPS=20`, so the correction does not resolve the visible symptom under that presentation cadence. A non-stopping 12.012-second sample taken while an MVE video played collected 2,443 main-thread samples with zero loss and was concentrated in video frame decoding, paletted `CopyPixels`, palette lookup and SDL audio resampling, confirming that FPGA drawing speed is not the principal movie cost. A following 8.021-second combat-only sample collected 358 samples with zero loss, equivalent to about 0.72 seconds of main-thread CPU time, and was diffuse across fog rendering, animation, sprite work and Noodles submission with no dominant combat-logic operation. The video and combat therefore show similar visible stutter from different paths, and the combat evidence again excludes HPS compute saturation. Because the corrected 15Hz simulation cannot divide evenly across a 20fps presentation and produces uneven visible spacing, the preserved target configuration was restored to `CapFPS=30`; `DrawFPS=0` and renderer statistics remain disabled, and the running process was not interrupted.

#### Next Steps:

Exit and relaunch GemRB so the restored 30fps cap takes effect, then repeat the same combat sequence once. Use that result to decide whether the correction improves late-frame recovery at the normal cadence; investigate MVE conversion and movie deadline accounting separately from combat, without using FPGA frequency as the movie remedy.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 50 COMMIT Unreleased 5fe8719 2026-09-25T21:30:50-07:00

#### Coming From:

Unreleased 5fe8719

#### Purpose:

Compare the same scene paused and in combat at the restored 30fps cap to identify the remaining main-thread operation without repeating broad profiling.

#### Outcome:

A non-stopping paired sample used the same deployed `5fe8719` process, scene, core and diagnostics-off configuration. The 12.026-second paused baseline collected 1,255 main-thread samples with zero loss, about 20.9% of one core at the 499Hz sampling rate. The user then unpaused into the laggy fight, and the 30.045-second combat window collected 5,518 samples with zero loss, about 36.8% of one core. Actor updates, effects, scripts and attack reevaluation were individually negligible, again excluding combat logic and HPS compute saturation. The largest resolved combat-only increase was `SDL_ResampleCVT_c2`, absent from the paused capture and accounting for 451 combat samples or 8.17% of sampled work; format conversion and ACM decoding added further audio work. Static tracing places this resampler in the main-thread `SDLAudioBackend::LoadSound` cache-miss path, while review also found that `SDLSoundBufferHandle::Disposable` returns true for a buffer that is still playing even though the shared LRU predicate treats true as safe to evict. No configuration, source, renderer or FPGA state changed during either capture.

#### Next Steps:

Obtain approval for one reversible no-build comparison of the same save with `AudioDriver=none`, followed immediately by restoration, to determine whether synchronous sound decode and resampling cause the visible hit-triggered stalls before proposing a narrow audio-cache or conversion correction. Keep the MVE movie path separate because it has its own decode, paletted upload and deadline-accounting costs.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 51 COMMIT Unreleased 5fe8719 2026-09-25T21:42:59-07:00

#### Coming From:

Unreleased 5fe8719

#### Purpose:

Test whether synchronous combat audio conversion causes the remaining visible stutter and use the rejected hypothesis to narrow the next correction.

#### Outcome:

The original target configuration was saved, `AudioDriver=none` was applied for one relaunch, and paired non-stopping samples were taken from the same scene at `CapFPS=30` with diagnostics disabled. The paused window collected 1,114 main-thread samples over 10.030 seconds with zero loss, about 22.2% of one core, and the combat window collected 5,537 samples over 30.046 seconds with zero loss, about 36.9% of one core. These totals are effectively unchanged from the audio-enabled comparison, and the user reported unchanged visible stutter, so the prior audio-resampling correlation is not causal. Actor, effect and script execution remained individually negligible, while combat increased an optimized libc memory-copy region, paletted `Blit1to4`, `SDL_MapSurface`, palette shading and texture submission; the user further identified spellcasting as the worst trigger. Static tracing shows that SDL2 currently bakes spell tint and alpha changes into each 8-bit animation palette, converts the full frame to the renderer's 32-bit texture format and uploads it again even though the Noodles draw path can apply both modifiers directly. The exact original configuration was restored with matching SHA256 `a2fbd5e810a271682c6f3549e20744369aa50987ca26a5c3af87698427cbc3f0`, including `AudioDriver=sdlaudio`, `CapFPS=30` and `DrawFPS=0`; it takes effect on the next launch and the running process was not interrupted.

#### Next Steps:

Obtain approval for a narrow GemRB SDL2 change that keeps grey and sepia palette effects unchanged but passes ordinary colour and alpha modulation through to the existing renderer draw command, preventing tint-only spell frames from being reshaded, reconverted and uploaded. Build and deploy one normal bundle, then compare the same spell-heavy battle visually before considering the separate temporary custom-palette churn.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 52 COMMIT Unreleased 1d9f008 2026-09-25T21:50:35-07:00

#### Coming From:

Unreleased 5fe8719

#### Purpose:

Stop SDL2 from rebuilding paletted sprite textures when only ordinary colour or alpha modulation changes during spell animation.

#### Outcome:

Source `1d9f008` adds the seventh ordered GemRB patch. The SDL2 sprite call site keeps the existing palette-baked path whenever grey or sepia is active, whose ordering can differ from texture modulation, while leaving ordinary `COLOR_MOD` and `ALPHA_MOD` flags for `SDL20VideoDriver::RenderCopyShaded` and the Noodles draw command to apply. The complete v0.9.5 seven-patch stack applied cleanly in a fresh tree, and the normal ARM GemRB and bundle builds passed with only the pre-existing cast-alignment warning. The staged SDLVideo plugin SHA256 is `4b75fe5fdc97bc5b141039be92b8b60f80d8218c75a031ef7a3a56b2989adc78`; the executable remains `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`, the core library remains `7b77e70c255df20495cea46a83a5b8e670ac3392cade1697a0543ec88098d582`, and SDL, the launcher and the FPGA core are unchanged.

#### Next Steps:

Deploy the staged bundle after confirming that no GemRB process is active, verify the three changed bundle hashes on the target, and compare the same spell-heavy battle with normal audio, `CapFPS=30` and diagnostics disabled. If spell stutter remains, measure the separate temporary custom-palette texture churn rather than changing the FPGA core.

#### Files Modified:

- patches/0007-sdlvideo-use-texture-modulation-for-paletted-sprites.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 53 COMMIT Unreleased 1d9f008 2026-09-25T22:13:17-07:00

#### Coming From:

Unreleased 1d9f008

#### Purpose:

Deploy the SDL2 paletted-sprite modulation correction for the controlled spell-heavy combat test.

#### Outcome:

After the user rebooted the MiSTer, no GemRB process was active and the normal `1d9f008` bundle was deployed. Device hashes match the staged executable `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`, core library `7b77e70c255df20495cea46a83a5b8e670ac3392cade1697a0543ec88098d582` and corrected SDLVideo plugin `4b75fe5fdc97bc5b141039be92b8b60f80d8218c75a031ef7a3a56b2989adc78`. The target retains `AudioDriver=sdlaudio`, `CapFPS=30`, `DrawFPS=0` and disabled renderer statistics. The launcher, SDL library and canonical MGL are unchanged, and the MGL still selects `_Utility/Noodles`; the installed RBF remains SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`.

#### Next Steps:

Launch the game normally and repeat the same spell-heavy battle, comparing the severity and duration of spellcasting stutters. If ordinary tinted effects improve but other spells still stall, isolate the remaining custom-palette texture conversions before changing SDL or the FPGA core.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 54 COMMIT Unreleased 1d9f008 2026-09-25T22:21:13-07:00

#### Coming From:

Unreleased 1d9f008

#### Purpose:

Record the spell-modulation hardware result and isolate the remaining combat stall before selecting another change.

#### Outcome:

The user reported that the combat stutter remains but appears somewhat better. A corrected-build paused capture collected 1,110 main-thread samples over 10.034 seconds with zero loss, about 22.2% of one core, while the active combat capture collected 5,561 samples over 35.055 seconds with zero loss, about 31.8% of one core; the earlier no-audio pre-fix combat capture used about 36.9%. In normalized samples per second, the prior optimized libc memory-copy region rose from 14.86 paused to 39.11 during combat, but it is now 17.94 paused and 16.55 during combat; combat `Blit1to4` fell from 6.72 to 2.23, and `ShadePalette` is absent. These samples are from comparable rather than identical combat action, but they show that the change removed the targeted combat-only palette conversion and upload spike, consistent with the partial visible improvement. No game-logic function is a dominant remaining hotspot, and the main thread still spends most of its wall time waiting or off CPU.

#### Next Steps:

Obtain approval for one reversible no-build comparison using `SDL_RENDER_NOODLES_BUFFERS=2` in the same save, then restore the standard configuration; this directly tests the previously identified triple-buffer FPGA and ARM DDR overlap as the cause of the remaining off-CPU combat stalls before another source or FPGA change.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 55 COMMIT Unreleased 1d9f008 2026-09-25T22:34:20-07:00

#### Coming From:

Unreleased 1d9f008

#### Purpose:

Test whether reducing Noodles from three display buffers to two removes the remaining combat stutter by preventing FPGA and ARM DDR overlap.

#### Outcome:

The target was rebooted with no GemRB process active, `SDL_RENDER_NOODLES_BUFFERS=2` was added without changing any binary or FPGA image, and the renderer log confirmed protocol 1.7 with two display buffers. The user repeated the same fight and reported the same problem, rejecting display-buffer overlap as the visible cause. A paused sample collected 1,104 samples over 10.020 seconds with zero loss, about 22.1% of one core. The following 35.032-second capture collected 5,683 samples with zero loss, about 32.5% of one core, effectively matching the corrected three-buffer combat capture at 31.8%; its final portion includes the `deathand.mve` transition and therefore is not a pure combat function comparison. The exact original target environment was restored with SHA256 `8420bf2c1ced1b7a2370eaeb8ba1e0817fe4551ba510d0f510816fb9617e7a2f`; the running process retains two buffers until it exits, while the next launch will use the standard three-buffer mode.

#### Next Steps:

Obtain approval for a short reversible frame-boundary trace that records only long frame intervals, `SDL_RenderPresent` duration and slow file operations into tmpfs during the same battle, then restores the launcher exactly; this requires no GemRB or Quartus build and will distinguish synchronous asset access from renderer waits and time spent elsewhere on the main thread.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 56 COMMIT Unreleased bdd3431 2026-09-25T22:35:21-07:00

#### Coming From:

Unreleased 1d9f008

#### Purpose:

Add a temporary low-overhead frame tracer that distinguishes long GemRB frame work from renderer presentation and slow file access during combat.

#### Outcome:

Source `bdd3431` adds a small ARM preload library and deterministic build script. The library timestamps `SDL_RenderPresent` boundaries, renderer duration, frame-cap delays and file operations longer than four milliseconds, emits only slow events to a tmpfs log, guards against recursive tracing and leaves gameplay data untouched. The first compiler pass exposed the 32-bit glibc aliases that emit `pread` and `fopen` as their 64-bit symbols; conditional wrappers corrected the duplicate definitions. The final strict cross-build produced an ARM ELF32 library with SHA256 `29709c21d42480b79a5c99cc534f461b5578bc251d26817e634efe9b4b5cf2fa`, and the native preload self-test verified constructor, frame interception and destructor output. GemRB, SDL and the FPGA core were not rebuilt.

#### Next Steps:

Deploy the tracer only after the current GemRB process has exited, preserve the exact target `run.sh`, add the tracer beside the existing affinity preload for one launch, confirm normal three-buffer mode and diagnostics-off settings, capture one spell-heavy battle, and restore the exact original target launcher immediately after the trace.

#### Files Modified:

- scripts/build-frame-tracer.sh
- tools/noodles-frame-trace.c

#### Status:

- [x] Built
- [ ] Passed

---

## 57 COMMIT Unreleased 5aaec84 2026-09-25T22:42:42-07:00

#### Coming From:

Unreleased bdd3431

#### Purpose:

Correct the frame tracer so its SDL wrappers resolve the dynamically loaded video library before forwarding calls.

#### Outcome:

The first target launch produced audio with a blank screen because the preload constructor looked up `SDL_RenderPresent` before GemRB loaded the SDL video plugin, retained a null target and suppressed presentation. The exact original target `run.sh` was restored immediately with SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`. Source `5aaec84` now retries both SDL symbol lookups lazily on their first call and reports a trace error if forwarding is still unavailable. The native self-test now loads its fake SDL library after tracer initialization, reproducing the target load order and proving that the wrappers forward a 56-millisecond frame correctly. The strict ARM build passed with corrected library SHA256 `6cd910f57f4a1693d777c0d6c8412674ea7757b39502618794340e9304c19410`; GemRB, SDL and the FPGA core remain unchanged.

#### Next Steps:

After the blank-screen process has exited, replace only the temporary tracer library, reactivate the already prepared one-launch `run.sh`, verify video output and a live `FRAME` record before starting the controlled spell-heavy battle, then restore the exact original launcher after collection.

#### Files Modified:

- scripts/build-frame-tracer.sh
- tools/noodles-frame-trace.c

#### Status:

- [x] Built
- [ ] Passed

---

## 58 COMMIT Unreleased 5aaec84 2026-09-25T22:50:03-07:00

#### Coming From:

Unreleased 5aaec84

#### Purpose:

Use the corrected frame tracer to separate the remaining spell-heavy combat stalls from presentation, frame-cap delay and slow storage access.

#### Outcome:

The corrected tracer launched with visible video, no forwarding errors and the standard protocol 1.7 three-buffer renderer. The paused boundary was saved before the user ran a spell-heavy combat segment that did not enter the death movie. The segment produced 59 frame records, including 54 intervals of at least 45 milliseconds; those long intervals had a 54.642-millisecond median and 794.398-millisecond maximum. After subtracting the measured frame-cap delay, work between presents had a 35.343-millisecond median, 771.179-millisecond maximum and 5.459-second aggregate across the recorded long frames. The actual presentation calls had a 0.130-millisecond median and 5.016-millisecond maximum, so they did not cause any recorded long interval. Only two file operations exceeded four milliseconds: a 6.676-millisecond music read on a secondary thread and a 4.068-millisecond `SHAREA.bam` open on the main thread, neither sufficient to explain the repeated 50-to-800-millisecond stalls. The exact original target `run.sh` was restored with SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`; the running process retains the tracer until exit, while the next launch is normal.

#### Next Steps:

Obtain approval to extend the same lightweight tracer with the `SDL_SetRenderTarget(NULL)` frame-phase boundary and slow `SDL_UpdateTexture`, texture-lock and render-target-switch measurements, then repeat one short combat segment. This will divide the proven between-present stall into GemRB update and drawing work, Noodles target flush and texture upload without rebuilding GemRB, SDL or the FPGA core.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 59 COMMIT Unreleased 4ce1773 2026-09-25T22:51:21-07:00

#### Coming From:

Unreleased 5aaec84

#### Purpose:

Extend the lightweight frame tracer to divide between-present combat stalls into GemRB drawing, Noodles target flushing and texture-transfer time.

#### Outcome:

Source `4ce1773` timestamps the first display-target switch before each present and accumulates the time spent in SDL texture update, lock, unlock and render-target-switch calls. Long-frame records report those phase totals beside the existing frame-cap and presentation measurements, while individual SDL operations are emitted only when they exceed four milliseconds. The delayed-load native self-test exercises every new wrapper and verifies a synthetic 64.449-millisecond frame split into a negligible engine phase, an 8.309-millisecond display phase and the expected target, update, lock, unlock and present calls. The strict ARM build passed with library SHA256 `0154958352d3990aa7374f0ee776862fa943e502c733f51d14607eca038f868b`; GemRB, SDL and the FPGA core were not rebuilt.

#### Next Steps:

Wait until the current game process exits, validate the ARM library with the delayed-load target self-test, deploy it for one normal three-buffer launch, record a paused boundary and one short spell-heavy combat segment, then restore the exact launcher and use the phase split to select the source correction.

#### Files Modified:

- scripts/build-frame-tracer.sh
- tools/noodles-frame-trace.c

#### Status:

- [x] Built
- [ ] Passed

---

## 60 COMMIT Unreleased 4ce1773 2026-09-25T22:57:16-07:00

#### Coming From:

Unreleased 4ce1773

#### Purpose:

Use the extended frame tracer to separate the remaining between-present combat stalls into display composition, texture transfer, target switching and earlier engine work.

#### Outcome:

The extended ARM tracer passed its delayed-load target self-test and ran without errors in the standard three-buffer configuration. The controlled combat segment produced 74 frame records, including 73 intervals of at least 45 milliseconds. Those long frames had a 49.533-millisecond median and 680.346-millisecond maximum. After the measured cap delay, work before the display-target boundary had a 32.026-millisecond median, 667.031-millisecond maximum and 6.603-second aggregate, while final display composition had a 0.093-millisecond median and 4.647-millisecond maximum and presentation had a 0.131-millisecond median and 3.336-millisecond maximum. The long frames averaged hundreds of target switches and several texture updates, but all target switches together had a 6.015-millisecond median and 9.536-millisecond maximum, and all texture updates had a 3.819-millisecond median and 15.875-millisecond maximum. In the two worst frames, the pre-display phase consumed 667.031 and 606.464 milliseconds while measured target and texture operations totaled less than eight milliseconds each. Texture locking was unused, and only one 4.150-millisecond main-thread file open exceeded the storage threshold. Display composition, presentation, storage, target switching and texture transfer therefore do not explain the severe stalls. The exact original target launcher was restored again with SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`.

#### Next Steps:

Obtain approval for one final low-overhead marker using the first existing render-target call after each present as the start of window drawing, without adding another hot wrapper. A short repeat will divide the remaining pre-display interval into game and GUI update before drawing versus the complete window-drawing pass, providing the source boundary needed for a correction.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 61 COMMIT Unreleased 2cf4651 2026-09-25T22:59:16-07:00

#### Coming From:

Unreleased 4ce1773

#### Purpose:

Add the final frame-phase marker that separates game and GUI update time from the complete window-drawing pass.

#### Outcome:

Source `2cf4651` records the first existing render-target call after each present as the drawing boundary. Each long-frame record retains the prior display, presentation, target and texture totals while dividing the current engine phase into pre-draw update time and drawing time. The delayed-load native self-test exercises a synthetic 66.011-millisecond frame and correctly separates 0.005 milliseconds before drawing from an 8.599-millisecond drawing pass, including the measured target, update, lock and unlock calls. The strict ARM build passed with library SHA256 `ad890b0197799914a44bc99346a8bfda76e16df17b1a4ec058db6c8ae021453d`; no new hot wrapper or GemRB, SDL or FPGA build was added.

#### Next Steps:

Run the ARM delayed-load self-test on the rebooted target, deploy the marker for one normal three-buffer launch, capture a paused boundary and a short spell-heavy combat segment, then restore the exact launcher and use the measured phase to select the correction.

#### Files Modified:

- scripts/build-frame-tracer.sh
- tools/noodles-frame-trace.c

#### Status:

- [x] Built
- [ ] Passed

---

## 62 COMMIT Unreleased 2cf4651 2026-09-25T23:06:48-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Use the final frame-phase marker to locate the remaining spell-heavy combat stalls within GemRB's main-thread frame.

#### Outcome:

The delayed-load target self-test passed and the standard three-buffer combat capture completed without trace errors. All 66 recorded frame intervals exceeded 45 milliseconds, with a 53.023-millisecond median, 323.093-millisecond 90th percentile and 870.406-millisecond maximum. The measured engine phase accounted for 8.214 seconds of the 9.075-second interval aggregate. Pre-draw game and GUI update dominated 25 frames and reached 733.453 milliseconds, while drawing independently reached 373.303 milliseconds. The worst frame split 849.706 milliseconds of engine work into 476.404 milliseconds before drawing and 373.303 milliseconds during drawing. Final display composition, presentation, target switching, texture updates and slow storage operations remained too small to explain the stalls. Static inspection shows that first-time BAM access eagerly decodes every frame into the process-lifetime animation-factory cache, after which drawing can lazily prepare the decoded sprites for the renderer. This matches the two observed main-thread phases and makes cold spell-asset creation the leading cause. The exact normal launcher was restored on disk; the current game process retains the tracer only until it exits.

#### Next Steps:

Reload the same save without exiting or rebooting and repeat the same battle once in the current traced process. Compare that warm-cache segment with this capture before changing source; a large improvement will justify prewarming or moving first-use spell asset preparation, while unchanged stalls will require finer instrumentation of game and GUI update logic.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 63 COMMIT Unreleased 2cf4651 2026-09-25T23:11:40-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Compare the same combat sequence after reloading its save in the existing process to test whether process-lifetime asset caches reduce the stalls.

#### Outcome:

The warm replay recorded 22 long frames over approximately 29.95 seconds, versus 66 over approximately 41.46 seconds in the cold capture, reducing the observed long-frame rate from 1.59 to 0.73 per second. Measured engine work within those frames fell from 198 to 93 milliseconds per elapsed second, the maximum interval fell from 870.406 to 577.424 milliseconds and the maximum drawing phase fell from 373.303 to 169.438 milliseconds. Process-lifetime caching therefore removes a material part of the stutter. It is not the complete cause: six warm frames still exceeded 100 milliseconds before drawing, that phase still reached 508.787 milliseconds, and the warm segment opened the previously unseen `spflmarr.bam` plus `CREAnim1.bif`. Combat continues to introduce uncached resources whose main-thread preparation is consistent with the remaining stalls, while display, presentation and measured SDL operations again remained small.

#### Next Steps:

Obtain approval to run the existing non-stopping instruction sampler during one more replay in the same process, with no build, restart or deployed-file change. Correlate its main-thread symbols with the frame trace to identify whether the remaining pre-draw stalls are BAM decoding, effect and script update, or another resource path before proposing the source correction.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 64 COMMIT Unreleased 2cf4651 2026-09-26T06:38:02-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Profile the live GemRB main thread during a warm combat replay to identify the code consuming the remaining pre-draw stalls.

#### Outcome:

The existing non-stopping sampler captured 5,442 main-thread task-clock samples over 45.05 seconds with zero loss and complete symbol resolution. That represents approximately 10.9 CPU-seconds, or 24.2 percent of one core during the wall interval. GemRB core code accounted for 33.48 percent of samples, SDL for 37.39 percent, SDLVideo for 6.60 percent and libc for 18.50 percent. The largest named GemRB functions were `FogRenderer::IsUncovered` at 4.70 percent, `Animation::NextFrame` at 2.06 percent and map exploration functions at 2.13 percent combined. `BAMImporter` and `BIFImporter` together accounted for only 0.16 percent, so eager BAM decoding is not consuming the repeated warm combat stalls even though process caching materially improved the preceding replay. The restarted normal game process did not have the frame tracer loaded, so the instruction samples cannot be assigned to individual long frames. The profile has no CPU hotspot compatible with repeated 500-millisecond execution bursts, making main-thread descheduling or another blocked interval the leading explanation for the remaining wall-time spikes.

#### Next Steps:

Obtain approval for one no-build replay bracketed by `/proc` scheduler, fault, context-switch and per-thread CPU snapshots. Use their deltas to determine whether the main thread accumulates run-queue wait during the visible stalls or consumes CPU in an unresolved path; only then choose between isolating its scheduler contention and adding CPU-time fields to the existing frame tracer.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 65 COMMIT Unreleased 2cf4651 2026-09-26T06:46:55-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Capture a user-marked single-hit interaction from an older save with exact frame-phase timing.

#### Outcome:

The final frame tracer was confirmed in the relaunched GemRB process and captured from a 366-line paused boundary through the user's single hit and ending dialog. This interval did not reproduce a severe frame drop: only three frames exceeded the tracer's 45-millisecond threshold, at 47.340, 46.627 and 45.229 milliseconds. Their pre-draw update phases were 0.148, 0.156 and 0.792 milliseconds, drawing phases were 23.339, 22.413 and 22.103 milliseconds, and final display and presentation remained negligible. The only slow resource event was a 4.608-millisecond open of `cgconjur.bam`. The exact normal launcher was restored on disk immediately after the traced process started, so subsequent launches remain diagnostic-free.

#### Next Steps:

Do not attribute the earlier combat stalls to this single-hit interaction because the measured replay stayed at normal capped cadence. Keep the current traced process available and repeat only a scenario in which the user visibly observes the severe drop, marking capture immediately afterward; otherwise use the exact result to narrow the problem to the spell-heavy battle rather than general hostility or hit processing.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 66 COMMIT Unreleased 2cf4651 2026-09-26T06:48:15-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Correct the prematurely ended single-hit capture and analyze the complete interaction after the actual unpause marker.

#### Outcome:

Entry 65 ended when the user said `capture now`, but that phrase marked the unpause rather than completion, so its no-drop conclusion did not cover the interaction. The tracer remained active and the subsequent segment captured 107 long frames with no trace errors. Exactly 11 frames reached at least 270 milliseconds, matching the user's count of 11 visible stutters; their intervals were 882.472, 658.952, 280.257, 285.940, 336.240, 450.842, 271.132, 500.221, 468.451, 350.516 and 300.459 milliseconds. The worst frame divided 866.165 milliseconds of engine time into 502.656 milliseconds before drawing and 363.508 milliseconds during drawing, while the second-worst placed 618.521 of 636.649 engine milliseconds before drawing. Twelve frames exceeded 100 milliseconds before drawing and eight exceeded 100 milliseconds while drawing. Final display, presentation, target switching, texture updates and file operations remained much smaller, confirming that this single-hit sequence reproduces the same two internal GemRB stall phases as spell-heavy combat.

#### Next Steps:

Obtain approval to reload this exact save in the current traced process and bracket the same interaction with the existing instruction sampler and scheduler snapshots. Start all measurements before telling the user to unpause and stop only after the user explicitly says the interaction is complete, so the exact frame trace can distinguish CPU execution from run-queue or blocked time without another build or restart.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 67 COMMIT Unreleased 2cf4651 2026-09-26T06:58:42-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Combine exact cold frame timing, instruction sampling and scheduler counters to identify the cause of the repeatable animation-heavy stutters.

#### Outcome:

The user reported no visible drops after reloading the same save in the existing process, then rebooted and observed approximately 11 stutters again during the same interaction. The synchronized cold capture recorded 93 long frames and ten intervals of at least 270 milliseconds, closely matching that count; the worst interval was 956.896 milliseconds and divided 938.510 milliseconds of engine time into 481.335 milliseconds before drawing and 457.176 milliseconds during drawing. Fifteen frames exceeded 100 milliseconds before drawing and six exceeded 100 milliseconds while drawing, while display, presentation, measured target switching, texture-update calls and individual file operations remained small. Over the 67.94-second interval the main thread accumulated 28.93 seconds running and 5.28 seconds waiting on its run queue, the process RSS grew by 94,832 KiB, minor faults increased by 122,272, major faults increased by 35 and block reads increased by 7,787,520 bytes. The instruction sample had zero loss and again showed diffuse animation, surface conversion, fog and renderer-front-end work instead of a combat-logic hotspot. The disappearance after a same-process reload and return after reboot, together with the cold memory growth and split update and drawing stalls, identifies first-use animation and effect preparation on GemRB's main thread as the cause rather than FPGA throughput or sustained game logic.

#### Next Steps:

Stop diagnostic replays and review the animation-factory and SDL surface-render preparation paths for a bounded correction that moves cold frame preparation into save or area loading, or incrementally prewarms it before gameplay. Present that source plan for approval before changing GemRB, and retain the exact cold capture as the validation baseline.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 68 COMMIT Unreleased dcfd5ec 2026-09-26T07:03:11-07:00

#### Coming From:

Unreleased 2cf4651

#### Purpose:

Move first-use animation preparation into area loading with memory-aware preloading and pressure-based retention.

#### Outcome:

Source `dcfd5ec` adds the eighth ordered GemRB patch. Area loading now prepares every current actor's combat, casting, damage and ready animation cycles and their SDL textures before closing the loading screen, then discovers memorized-spell effect resources and projectile, trail and visual-effect animations; lower-priority stance, movement and alternate-attack cycles are added only while at least 112 MiB remains available. The pass stops at an 80 MiB available-memory reserve, and the shared factory cache now tracks recent use and can discard its oldest cache-only objects under pressure without invalidating externally owned animations. Dynamic script resources retain the existing lazy fallback, and video backends other than SDL2 retain a no-op preparation hook. The complete eight-patch stack applied after pristine GemRB 0.9.5, reproduced the development worktree exactly, and the official ARM GemRB and bundle builds passed with only the existing SDL cast-alignment warning. The staged core library SHA256 is `77b7a19f5d57495316a26bbf03d47b2eb50d66e858a408fbcb7aa2afd2b77f27`, SDLVideo SHA256 is `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and the executable remains `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`. The corrected target at `10.10.0.21` was reachable, but GemRB PID 1348 remained active with 82,820 KiB available, so the running session was left untouched and deployment and hardware validation remain pending.

#### Next Steps:

After the active GemRB session exits or the MiSTer is rebooted, deploy the staged normal bundle without changing its configuration, launcher or FPGA core. Cold boot and load the exact save, note the added loading time, verify available memory stays above 80 MiB with no swap use, then replay the interaction that produced approximately 11 severe stutters and compare it with the retained cold baseline.

#### Files Modified:

- patches/0008-memory-aware-area-animation-preload.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 69 COMMIT Unreleased dcfd5ec 2026-09-26T07:17:54-07:00

#### Coming From:

Unreleased dcfd5ec

#### Purpose:

Deploy the memory-aware area animation preload build for cold hardware validation.

#### Outcome:

After the MiSTer rebooted, no GemRB process was active and the normal  bundle was deployed to . Device hashes match the staged core library , SDLVideo plugin  and unchanged executable . The preserved BG2 configuration uses normal SDL audio,  and ; the canonical MGL still names , and the timing-qualified Noodles RBF remains SHA256 . Immediately after deployment the target had 462,668 KiB available and no configured swap.

#### Next Steps:

Cold-load the exact combat save, allow the longer preload phase to finish, and replay the sequence that previously produced approximately 11 severe stutters. Record whether loading completes, the available-memory floor, any swap use and the visible stutter count before deciding whether the bounded preload is accepted or needs narrower resource discovery.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---


## 70 COMMIT Unreleased dcfd5ec 2026-09-26T07:18:17-07:00

#### Coming From:

Unreleased dcfd5ec

#### Purpose:

Correct the incomplete deployment metadata recorded in Entry 69.

#### Outcome:

Entry 69 lost shell-quoted inline values while it was written; its deployment conclusion remains valid, and this entry supplies the omitted evidence. The normal `dcfd5ec` bundle was deployed to `10.10.0.21` after confirming no GemRB process was active. Device hashes match the staged core library `77b7a19f5d57495316a26bbf03d47b2eb50d66e858a408fbcb7aa2afd2b77f27`, SDLVideo plugin `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and unchanged executable `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`. The preserved BG2 configuration uses normal SDL audio, `CapFPS=30` and `DrawFPS=0`; the canonical MGL still names `_Utility/Noodles`, and the timing-qualified Noodles RBF remains SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`. Immediately after deployment the target had 462,668 KiB available and no configured swap.

#### Next Steps:

Cold-load the exact combat save, allow the longer preload phase to finish, and replay the sequence that previously produced approximately 11 severe stutters. Record whether loading completes, the available-memory floor, any swap use and the visible stutter count before deciding whether the bounded preload is accepted or needs narrower resource discovery.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 71 COMMIT Unreleased dcfd5ec 2026-09-26T07:23:34-07:00

#### Coming From:

Unreleased dcfd5ec

#### Purpose:

Validate whether loading-screen animation prewarming removes the repeatable cold combat stutters.

#### Outcome:

The user observed a modestly longer load but the same combat stutters. The deployed build logged that its pass prepared 11,084 animation frames for only one actor and six spells while 339,572 KiB remained available, so memory pressure did not stop it. The sequence included a cutscene between loading and combat; after the preload completed, that cutscene loaded `cut204f2.bcs`, created `illasera.cre` and loaded the actor's scripts and dialog. Combat then cold-loaded `CGConjur.bam`, `spmon1.eff`, `MONSUM01.2da`, `dogwisu.cre`, `worgsu.cre`, `SPMONSUM.vvc`, `SPMONSUM.bam` and `SPWI412.spl`. The cache and renderer preparation ran as designed, but the loading-screen discovery point preceded the dynamically created mage and summon dependency graph, so it could not prepare the resources responsible for this battle.

#### Next Steps:

Replace the one-time map snapshot with a bounded queue that accepts actors when they are dynamically added, walks their spell and script resource dependencies including EFF, SPL, CRE and summon-table links, and prepares animation frames incrementally during the intervening cutscene and dialog. Retain the 80 MiB reserve and lazy fallback, build one ARM GemRB bundle without rebuilding Quartus, then cold-test the same sequence and verify that Illasera and the dog and worg resources are prepared before combat.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 72 COMMIT Unreleased 47ce25f 2026-09-26T07:24:26-07:00

#### Coming From:

Unreleased dcfd5ec

#### Purpose:

Prewarm dynamically created actors and their recursive combat dependencies before first use.

#### Outcome:

Source `8ac89d9` adds the ninth ordered GemRB patch, and source `47ce25f` moves its `Game.cpp` method hunk away from patch 0008 so the build driver's reverse checks remain idempotent without changing behavior. Every actor added to a loaded map is queued by global ID; one actor, generic resource or stance-orientation unit is processed per rendered frame. Actor spellbooks and compiled scripts expose resource references, and the queue recursively recognizes SPL, external EFF, monster-summoning 2DA, CRE, projectile, VVC and BAM dependencies while retaining the 80 MiB available-memory reserve. This covers the observed cutscene-created Illasera and her spell-driven dog and worg summon chain without resource-name special cases. The complete nine-patch stack and ARM bundle built successfully. The deployed core library SHA256 is `a611473a6b3c8c17e8d0ca7aea96b68541199fcaba39c0e46e89d5007ee9aef5`; SDLVideo remains `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and the executable remains `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`. The rebooted target had 464,380 KiB available before launch, and deployment preserved normal audio, `CapFPS=30`, `DrawFPS=0`, the canonical MGL and the timing-qualified Noodles RBF.

#### Next Steps:

Cold-load the exact save and let the cutscene and dialog run normally, then replay the same battle. Confirm from the game log that the dynamic queue discovers and completes the Illasera and summon resource graph before combat, record its memory floor and swap use, and compare the visible severe-stutter count with the prior approximately 11-event baseline.

#### Files Modified:

- patches/0009-dynamic-actor-animation-prewarm.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 73 COMMIT Unreleased 47ce25f 2026-09-26T07:39:02-07:00

#### Coming From:

Unreleased 47ce25f

#### Purpose:

Validate whether dynamic actor and dependency prewarming removes the cold combat stutters without violating its memory reserve.

#### Outcome:

The user reported that the combat stutter remained. Before combat, the dynamic pass reported 101 resources and 14,471 animation frames prepared with 115,048 KiB available, but the exact battle resources `CGConjur.bam`, `spmon1.eff`, `MONSUM01.2da`, `worgsu.cre`, `SPMONSUM.bam` and `SPWI412.spl` were still first discovered during combat. Preparing the resulting creature graph then reduced available memory to 39,964 KiB before the next reserve check, while the GemRB process reached 448,940 KiB RSS and the system had only 18,960 KiB available. GemRB itself had not entered swap, but the pass both missed the runtime-selected spell dependency before use and allowed one coarse preload task to overshoot the intended 80 MiB reserve. This hardware result rejects the dynamic broad-preload design.

#### Next Steps:

Do not continue testing this build. Trace the runtime action handoff that exposes numeric and resource-named spells, then seek approval for a narrow replacement that queues only exact spell and creature dependencies when their actions are enqueued, divides animation work into memory-checked units, and removes broad actor-script and all-stance prewarming before rebuilding GemRB.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 74 COMMIT Unreleased d6d5103 2026-09-26T07:42:20-07:00

#### Coming From:

Unreleased 47ce25f

#### Purpose:

Replace broad animation prewarming with exact numeric-spell discovery and bounded incremental preparation.

#### Outcome:

Source `d6d5103` recognizes both resource-named and numeric spell actions in compiled actor scripts, including Illasera's action 191 with spell number 2412 that resolves to `SPWI412`. It queues only exact spell, external effect, summon table, creature, projectile and visual dependencies; disables the broad loading-screen actor pass and generic script-resource scan; limits live actors to combat-relevant casting and attack poses; divides temporary creature preparation into stance-orientation tasks; and checks the 80 MiB reserve before every animation frame. The correction is partitioned across patches 8 through 10 so every patch passes the build driver's reverse check after the complete stack is applied. A clean GemRB 0.9.5 tree accepted all ten patches, its final source matched the development tree, the ARM build and normal bundle completed, and the staged core library SHA256 is `53db70dda028bea953d7426584dc84852c908fb3939f390b6edb6743581060b3`; SDLVideo remains `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and the executable remains `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`.

#### Next Steps:

After the active low-memory GemRB session has ended or the MiSTer has rebooted, deploy the staged normal bundle without changing the launcher, configuration or FPGA core. Cold-load the exact save, confirm `SPWI412`, its summon table and creature resources are discovered before combat, verify available memory remains above 80 MiB with no GemRB swap use, and compare the visible stutter count with the approximately 11-event baseline.

#### Files Modified:

- patches/0008-memory-aware-area-animation-preload.patch
- patches/0009-dynamic-actor-animation-prewarm.patch
- patches/0010-targeted-spell-action-prewarm.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 75 COMMIT Unreleased d6d5103 2026-09-26T07:53:10-07:00

#### Coming From:

Unreleased d6d5103

#### Purpose:

Deploy the targeted numeric-spell prewarm build for cold combat validation.

#### Outcome:

The MiSTer at `10.10.0.21` had no active GemRB process, 466,348 KiB available and no configured swap, so the normal bundle was deployed without replacing game data, saves, cache or the edited BG2 configuration. Device hashes match the staged core library `53db70dda028bea953d7426584dc84852c908fb3939f390b6edb6743581060b3`, SDLVideo plugin `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and executable `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`. The preserved configuration uses normal SDL audio, `CapFPS=30` and `DrawFPS=0`; the canonical MGL still selects `_Utility/Noodles`, and the timing-qualified RBF remains `Noodles_20260925.rbf` with SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`. The idle target had 465,072 KiB available after deployment.

#### Next Steps:

Cold-launch BG2 through the normal MGL, load the exact save and allow the cutscene and dialog to reach the battle. Record whether `SPWI412`, `MONSUM01` and the summoned creature resources are prepared before combat, inspect the memory floor and GemRB swap use, and compare the visible severe-stutter count with the prior approximately 11-event baseline.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 76 COMMIT Unreleased d6d5103 2026-09-26T08:09:26-07:00

#### Coming From:

Unreleased d6d5103

#### Purpose:

Validate whether exact numeric-spell discovery and bounded dependency preparation remove the cold combat stutters.

#### Outcome:

The user reported unchanged combat stutter. The corrected queue did resolve Illasera's numeric spell and prepared `spmon1.eff`, `MONSUM01.2da`, `SPMONSUM.bam`, `koboldsu.cre`, `ogrelesu.cre`, `worgsu.cre` and `dogwisu.cre` before combat, completing 39 resources and 11,602 animation frames with 140,236 KiB available. The first cast then opened the dynamically selected `CGConjur.bam`, instantiated the selected kobold and expanded the queue to 13,842 prepared frames with 111,080 KiB available. The paused process used 369,812 KiB RSS, retained no swapped pages and left 110,036 KiB system memory available, so neither the reserve nor paging caused the unchanged symptom. This rejects missing spell-dependency discovery as the principal cause. The Noodles renderer has a 192 MiB default resident-texture budget, so sweeping every orientation and every summon-table candidate can evict early prepared textures before combat; that cache-pollution explanation is an inference from the verified queue size and renderer policy rather than a measured eviction count because renderer statistics were disabled for the normal run.

#### Next Steps:

Stop broad pose preparation and seek approval for a cache-aware correction that prepares each spell's casting-glow resource, retains only casting orientations for scripted casters, and limits summon candidates to their stored orientation and immediate attack poses. Keep the resulting working set below the renderer's 192 MiB residency budget, preserve the per-frame memory guard, rebuild GemRB without Quartus and cold-test the same battle.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 77 COMMIT Unreleased 6f62121 2026-09-26T08:12:07-07:00

#### Coming From:

Unreleased d6d5103

#### Purpose:

Keep targeted combat prewarming within the renderer's resident-texture budget.

#### Outcome:

Source `6f62121` prepares each exact spell's generic casting-glow visual and restricts scripted casters to 32 casting and conjuring orientation tasks. Summon-table creature candidates contribute resource dependencies without materializing poses, while an actual noncaster added to the map prepares only eight immediate combat and movement poses at its current orientation. The complete ten-patch stack applies cleanly, every patch passes the build driver's reverse check, and the resulting source exactly matches the development tree. The ARM build and normal bundle completed successfully; the staged core library SHA256 is `82eecc14d971cfa6918c95ecfbaad15886a024768eed9480c1675eb29004039f`, while SDLVideo remains `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and the executable remains `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`.

#### Next Steps:

After the active GemRB session has ended or the MiSTer has rebooted, deploy the staged normal bundle without changing the canonical launcher, configuration or FPGA core. Cold-load the same save, confirm that `CGConjur` and exact spell dependencies are prepared before combat with a much smaller frame count, and compare the visible severe-stutter count with the approximately 11-event baseline.

#### Files Modified:

- patches/0009-dynamic-actor-animation-prewarm.patch
- patches/0010-targeted-spell-action-prewarm.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 78 COMMIT Unreleased 6f62121 2026-09-26T08:17:20-07:00

#### Coming From:

Unreleased 6f62121

#### Purpose:

Deploy the cache-aware combat prewarm build for cold hardware validation.

#### Outcome:

The MiSTer at `10.10.0.21` had no active GemRB process and 465,472 KiB available, so the normal bundle was deployed without replacing game data, saves, cache or the edited BG2 configuration. Device hashes match the staged core library `82eecc14d971cfa6918c95ecfbaad15886a024768eed9480c1675eb29004039f`, SDLVideo plugin `5f49144764a31449747720353f51363a408ec573bf553f83b89c816e4624dbe3` and executable `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d`. The preserved configuration uses normal SDL audio, `CapFPS=30` and `DrawFPS=0`; the canonical MGL still selects `_Utility/Noodles`, and the timing-qualified `Noodles_20260925.rbf` remains unchanged with SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`. The idle target had 463,636 KiB available after deployment.

#### Next Steps:

Cold-launch BG2 through the normal MGL, load the exact save and allow the cutscene and dialog to reach the battle. Compare the visible severe-stutter count with the approximately 11-event baseline, then inspect the game log to verify that `CGConjur` was prepared before combat and that the queue completed with a substantially smaller animation-frame working set.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 79 COMMIT Unreleased 6f62121 2026-09-26T08:27:09-07:00

#### Coming From:

Unreleased 6f62121

#### Purpose:

Validate whether the cache-aware combat prewarm working set removes the cold battle stutters.

#### Outcome:

The user reported the same combat stutter. Before combat the corrected queue completed the same 39-resource dependency graph with 5,310 prepared frames and 243,596 KiB available, down from the preceding build's 11,602 frames at the same point. Creating the actual selected Ogrillon added only 85 frames and completed at 5,395 with 145,952 KiB available. The paused GemRB process used 349,596 KiB RSS, retained no swapped pages and left 130,184 KiB system memory available. Exact spell discovery, `CGConjur` preparation, candidate-pose removal and the 61 percent frame-count reduction therefore did not improve the symptom, rejecting the current preload strategy. Whether 5,395 multipart frames still exceed the Noodles renderer's 192 MiB resident-texture budget remains unmeasured because normal renderer statistics were disabled.

#### Next Steps:

Do not rebuild. Enable the existing disabled-by-default Noodles renderer statistics for one cold replay with the same binary and core, then use its upload, eviction and resident-byte counters to decide whether to remove hardware texture prewarming in favor of CPU-only factory preparation or abandon asset residency as the cause. Restore the normal statistics-disabled environment immediately after launch so later sessions remain normal.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 80 COMMIT Unreleased 6f62121 2026-09-26T08:37:51-07:00

#### Coming From:

Unreleased 6f62121

#### Purpose:

Measure whether renderer texture eviction or upload work causes the unchanged cold combat stutters.

#### Outcome:

The user completed the same cold battle with the existing Noodles statistics enabled for one launch. Texture eviction remained zero throughout loading, prewarming and combat, resident texture memory peaked at only 35.2 MiB against the 192 MiB budget, readbacks remained zero, renderer queue maxima stayed below 26.3 milliseconds, and texture creation and update callbacks consumed only a few milliseconds per interval. The 5,310-frame preload therefore did not churn the hardware cache, definitively rejecting renderer residency, texture upload and FPGA throughput as causes of the severe stalls. The statistics environment disabled itself for subsequent launches. The remaining cold combat log consists of main-thread resource discovery and actor setup, while the normal configuration also formats every message and debug record into an asynchronous logger that writes through the launcher's append-only SD-card log; with the deliberately single glibc allocation arena, that cold resource burst can contend with the main thread even though individual file operations and renderer calls remain short.

#### Next Steps:

Seek approval for one no-build cold comparison with GemRB `Logging=0`, preserving normal audio, 30 FPS, the launcher and the FPGA core. If disabling the full debug stream removes the stutters, make release logging disabled by default and remove the failed animation-prewarm patches; if it does not, abandon logging as well and scope background CPU-side resource construction rather than any further renderer or FPGA work.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 81 COMMIT Unreleased 4102834 2026-09-26T08:59:38-07:00

#### Coming From:

Unreleased 6f62121

#### Purpose:

Reuse prepared actor-palette animation textures instead of rebuilding shared sprite surfaces during every draw.

#### Outcome:

Source `4102834` makes SDL2 actor drawing select a persistent texture variant keyed by the complete actor-palette hash instead of replacing the shared factory sprite palette before and after every draw. Each sprite retains the four most recently used variants, invalidates them when its source pixels change and leaves grey and sepia effects on the existing fallback path. The combat preloader now prepares each actor part and shadow with the exact palette that runtime drawing will request. The complete ten-patch stack applied to a pristine GemRB 0.9.5 tree, reproduced the development source exactly, passed every full-stack reverse check and built successfully both natively and for ARM. After the active session ended and the MiSTer rebooted, the normal bundle was deployed to `10.10.0.21`; device hashes match `33f5c3acb264b6ee03d9789150159fa410c0611d5b6f22b13423b33e4bb25933` for `libgemrb_core.so.0.9.5`, `7abb4cf705eacd376afe06c3f81f25d4627670cbc3c7579b9b9f01e7bd15df30` for `SDLVideo.so` and the unchanged `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d` for the executable. Deployment preserved normal SDL audio, `CapFPS=30`, `DrawFPS=0`, disabled renderer statistics, the USB swap setting, the canonical MGL and the timing-qualified Noodles RBF SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`. The idle target had 463,688 KiB available after deployment. The user then cold-loaded and replayed the battle and reported the same result, so persistent actor-palette textures did not resolve the visible pauses. The live log proves why the broader preload still misses the summoned attackers: it found `worgsu.cre` and `dogwisu.cre` before completing 5,310 frames, but its CRE branch only inspected their scripts and never requested their actor animations; after the worg spawned in combat, GemRB loaded `MWLF_WO.bmp` and completed 74 additional animation frames. The palette cache operated on prepared objects, but the relevant summoned-creature frames had not been prepared.

#### Next Steps:

Prewarm the combat animations of CRE resources discovered through spell and summon tables before those creatures spawn, then cold-test the same battle once without adding diagnostics or changing the FPGA core.

#### Files Modified:

- patches/0007-sdlvideo-use-texture-modulation-for-paletted-sprites.patch
- patches/0008-memory-aware-area-animation-preload.patch
- patches/0010-targeted-spell-action-prewarm.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 82 COMMIT Unreleased 1f9b746 2026-09-26T12:01:54-07:00

#### Coming From:

Unreleased 4102834

#### Purpose:

Prepare summoned-creature combat animations before the spell creates those actors.

#### Outcome:

Source `1f9b746` makes the dynamic dependency walker use each discovered CRE resource's temporary actor to prepare physical attack variants, shooting, damage, ready, walk and shadow animations in every orientation with exact part palettes before deleting it. This closes the observed gap where the dependency graph found the dog and worg creature files before combat but waited until the live summon entered the map to request `MWLF_WO.bmp` and its 74 animation frames. The complete ten-patch stack applied to pristine GemRB 0.9.5, reproduced the development source exactly, passed every full-stack reverse check and built successfully both natively and for ARM. After the active session ended and the MiSTer rebooted, the normal bundle was deployed to `10.10.0.21`; device hashes match `41bff74fbd092da652242652e392b3945524ab77d52d77459389bde56b47aded` for `libgemrb_core.so.0.9.5`, the unchanged `7abb4cf705eacd376afe06c3f81f25d4627670cbc3c7579b9b9f01e7bd15df30` for `SDLVideo.so` and the unchanged `5b2b535182fb1d2cd8ed814d4ba5a5141184f446fa3531c990315ada6218a46d` for the executable. Deployment preserved normal SDL audio, `CapFPS=30`, `DrawFPS=0`, disabled renderer statistics, the USB swap setting, the canonical MGL and launcher, and the timing-qualified Noodles RBF SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`. The idle target had 462,072 KiB available after deployment. The user reported no visible change. The live log confirms that the intended work occurred before combat: `MWLF_WO.bmp` was loaded before the dependency pass completed, and that pass increased from the prior 5,310 frames to 10,460 frames with 216,656 KiB still available; the later live-actor pass added only 183 frames. This rejects missed summon animation preparation and, together with the preceding exact-palette, residency and upload results, rejects asset preloading as the solution to the stutter.

#### Next Steps:

Do not add more preload coverage. Remove the failed memory-aware, dynamic-actor and targeted-spell preload patches before the next production build, retaining the independently useful palette-modulation work, and return to the remaining blocked or descheduled main-thread interval rather than asset or FPGA work.

#### Files Modified:

- patches/0010-targeted-spell-action-prewarm.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 83 COMMIT Unreleased 1f9b746 2026-09-26T12:42:20-07:00

#### Coming From:

Unreleased 1f9b746

#### Purpose:

Isolate normal GemRB logging from the unchanged cold combat stutters without rebuilding.

#### Outcome:

At the user's direction, the target BG2 configuration was copied to `GemRB-bg2.cfg.logging1-backup` with SHA256 `a2fbd5e810a271682c6f3549e20744369aa50987ca26a5c3af87698427cbc3f0`, then only `Logging` was changed from `1` to `0`; the resulting configuration SHA256 is `fa87b79d13f66540e6db2b5aebb33b185702d80642dae09f89e608eb0e48973e`. Normal SDL audio, `CapFPS=30`, `DrawFPS=0`, renderer statistics, the launcher, binaries and FPGA core were unchanged. The user cold-launched and replayed the battle and reported that disabling logging made no difference, rejecting logger formatting, allocation and output contention as the cause. The exact logging-enabled backup was then restored with matching SHA256 `a2fbd5e810a271682c6f3549e20744369aa50987ca26a5c3af87698427cbc3f0`; the active PID 1299 retains its launch-time logging state until it exits.

#### Next Steps:

Remove the failed memory-aware, dynamic-actor and targeted-spell preload patches before the next production build. Further work should target the measured blocked or descheduled main-thread intervals directly rather than renderer throughput, audio, logging or asset preparation, all of which have now been rejected by controlled comparisons.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 84 COMMIT Unreleased 1f9b746 2026-09-26T12:52:40-07:00

#### Coming From:

Unreleased 1f9b746

#### Purpose:

Capture the first general post-load movement stutters without assuming they are combat-specific.

#### Outcome:

After the MiSTer rebooted with no GemRB process active and 465,872 KiB available, the normal `run.sh` was preserved as `run.sh.pre-general-stutter` with SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`. A temporary launcher differing only by the existing frame-tracer preload was installed with SHA256 `583eb0ea8a3177701bc55bf3bfa1e4b337f327264334fbe939bec35ea90cec70`; the tracer SHA256 is `ad890b0197799914a44bc99346a8bfda76e16df17b1a4ec058db6c8ae021453d` and the non-stopping sampler SHA256 is `3ede29c397776eb865071a75cceb92859f349d4b1980fbf986a692b1596caaba`. Normal audio, logging, the 30 FPS cap, disabled renderer statistics, binaries and FPGA core remain unchanged. The tracer records frame phase, SDL texture calls and bounded file operations while the sampler and kernel snapshots will measure main-thread execution, faults, I/O and run-queue delay over the same paused and movement windows.

#### Next Steps:

Cold-load a save and pause before moving. Capture the paused baseline, then synchronously sample approximately ten seconds of ordinary movement through the first two or three visible stutters and a final paused boundary. Restore the exact normal launcher immediately after collecting the files and analyze the common wall-time phase without using combat-specific assumptions.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---

## 85 COMMIT Unreleased 1f9b746 2026-09-26T12:59:35-07:00

#### Coming From:

Unreleased 1f9b746

#### Purpose:

Determine which system and frame phases contain the first visible post-load movement stutters.

#### Outcome:

The paused baseline, movement window, continuing frame trace and final paused boundary were captured from PID 1352. The fixed 25.027-second sampler ended just before the visible cluster and contained only two frames above 45 milliseconds, but the tracer remained active while movement continued and recorded fourteen long frames from 288.0 through 304.3 seconds, including six frames above 90 milliseconds and maxima of 449.849 and 210.874 milliseconds. Across the six frames above 90 milliseconds, 989.623 milliseconds was inside GemRB engine work, split between 624.759 milliseconds of update phase and 364.863 milliseconds of draw phase, while display and `SDL_RenderPresent` totaled only 3.556 milliseconds. No traced file operation above four milliseconds coincided with the cluster. The surrounding process-counter interval accumulated only 565,760 bytes of physical reads and four major faults with no swap or reclaim, but RSS grew by 61,136 KiB and the process incurred 76,415 minor faults, showing CPU-side allocation and resource construction rather than storage latency or FPGA presentation. The exact normal launcher was restored with its original SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`; subsequent launches do not preload the tracer.

#### Next Steps:

Use one bounded no-build replay with the sampler kept active through the user's stop signal and add per-long-frame process-fault deltas to the existing temporary tracer. The result should identify the main-thread call sites active during the 90-to-450-millisecond update and draw frames and distinguish allocation and decoding work from scheduler delay before proposing a production source change.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 86 COMMIT Unreleased 1f9b746 2026-09-26T13:18:50-07:00

#### Coming From:

Unreleased 1f9b746

#### Purpose:

Correlate the reproducible first post-load movement stalls with exact main-thread execution sites.

#### Outcome:

A clean cold run captured a four-second menu baseline followed by save loading, immediate movement and the user's paused boundary with a monotonic timestamp attached to every main-thread task-clock sample. The user identified one large pause immediately after issuing movement and two later pauses. The matching largest frames were 792.761, 474.351 and 169.963 milliseconds; presentation took only 0.426, 0.128 and 1.421 milliseconds. At the sampler's observed rate of about 607 samples per main-thread runtime second, those frames contained only approximately 275, 59 and 82 milliseconds of sampled user execution, so most wall time was spent in kernel work or waiting to run. The first frame's user work was dominated by 50 samples in `memset` and 30 in `TraversabilityCache::ValidateTraversabilityCacheSize`, which is lazily called by the first requested path and resizes and clears the complete area traversability array despite a source comment that this should happen when the map loads. Its remaining gaps and the two later frames correlate with BIF resource lookup, heap growth, zlib decompression and sprite construction. Across the complete load-and-movement window GemRB grew by 155,736 KiB RSS, incurred 113,166 minor and 95 major faults, and waited runnable for 5.293 seconds with no swap or reclaim. This isolates the first-command pause as lazy traversability-cache initialization and shows that later cold pauses are synchronous resource decompression and memory population rather than game logic, FPGA presentation or slow individual file reads. The exact normal launcher was restored with SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`.

#### Next Steps:

Move traversability-array sizing and clearing from the first path request into map construction so the known cost remains inside the loading screen, then validate the same cold-load movement sequence in isolation before removing the rejected broad combat-preload patches or addressing the separately measured lazy BIF and sprite decompression pauses.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 87 COMMIT Unreleased 9f5eb1a 2026-09-26T13:22:36-07:00

#### Coming From:

Unreleased 1f9b746

#### Purpose:

Create a deterministic MiSTer input capture and replay harness for autonomous cold-load stutter testing.

#### Outcome:

Source `9f5eb1a` adds a standard-library Python tool that records timestamped Linux evdev events from explicitly selected devices without grabbing them, validates the portable JSON-lines trace, and replays each source through a separate Linux uinput device with original relative timing. Target discovery correctly identified the physical Telink mouse as `event0`, the physical keyboard as `event3`, and excluded the MiSTer virtual input. A one-second empty-recording self-test produced a clean trace, and uinput keyboard creation and destruction succeeded without injecting an event. The user's complete launch, save-load, movement and final pause sequence produced 1,550 events over 64.753 seconds, comprising 1,501 mouse and 49 keyboard events; every pressed key and mouse button has a matching release. The preserved local trace SHA256 is `8f874471e9f382d7f34dede9d50bf840b11c240132bca23699c190a1f9ff1b45`. The GemRB binary, configuration, launcher and FPGA core were unchanged.

#### Next Steps:

Reboot the MiSTer once, stage the existing frame tracer and timestamped CPU sampler, and replay the preserved 64.753-second sequence at normal speed. Confirm that it launches GemRB, loads the intended save, moves through the cold stalls and ends paused before accepting the harness for autonomous regression tests.

#### Files Modified:

- tools/mister-input-harness.py

#### Status:

- [x] Built
- [ ] Passed

---

## 88 COMMIT Unreleased ??? 2026-09-26T13:36:59-07:00

#### Coming From:

Unreleased 9f5eb1a

#### Purpose:

Make the captured cold-load test replay deterministic by loading the normal Noodles MGL directly and replaying only its GemRB input interval.

#### Outcome:

The first full-trace validation did not launch GemRB: stock Main hotplugged the synthetic devices but ignored relative pointer movement in its frontend, then interpreted the first replayed click as selection of the already-highlighted Arcade folder. The replay was stopped, GemRB never started, and the exact normal launcher was restored. The captured trace remains valid for GemRB, whose SDL input path is separate from Main's frontend, so this change will add a bounded trace-start option and use Main's supported command FIFO to load the unchanged canonical MGL after the Noodles watcher starts. No FPGA, GemRB, SDL or launcher binary will change.

#### Next Steps:

Validate trace selection locally, stage the harness and existing diagnostics without launching them, then ask the user to reboot the MiSTer manually. After that reboot, create the replay devices before GemRB starts, directly load the canonical MGL, replay the segment beginning with the GemRB menu interaction at normal speed and confirm that it loads the save, moves through the cold stalls and ends paused.

#### Files Modified:

- tools/mister-input-harness.py

#### Status:

- [ ] Built
- [ ] Passed

---
