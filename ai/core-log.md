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
