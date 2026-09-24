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

Source `ebaa9e5` adds opt-in five-second timing and call-volume summaries for Noodles texture creation, updates, locks, target changes, explicit readback and destruction plus SDL command-building callbacks. It also corrects the statistics documentation: presentation fence time includes completion of queued FPGA rendering and the vertical-blank handoff, while statistics-disabled operation uses the same synchronization with performance-clock reads and counters disabled. The SDL cross-build, renderer diagnostic build and complete bundle build passed; the diagnostic remains SHA256 `21595f3d89ef0542407d9059c0ecf06df4d1cb1ba69f54b66089ff430bf10a0e`, the unstripped SDL SHA256 is `c3cbaad9c4113711106b78ee8cd08f23952b1a7d5b66cd0c15e54afddc8b233a`, and the hash-verified deployed library SHA256 is `78c6ea2cd6d23951e9139d546eda676c4e967aa4a01586b8d9afa79ca20ee18d`. Spell-heavy AR4000 samples ran at 9.3-11.5fps: SDL command construction stayed at 1.08-1.27ms per frame, texture updates used 4.17-14.32ms, explicit texture locks and readbacks were absent, and creation, target changes and destruction were negligible except one 0.71ms destruction burst. After subtracting measured callbacks, 36-57ms per frame remained in GemRB and SDL's front end. Renderer submission varied from 5.75-15.02ms as heavy frames produced command-stream backpressure and 33-54 drains per five-second window, while presentation fence time was 27.8-30.0ms. The user reported unchanged stutter with statistics disabled, confirming profiler overhead is not causal. Two later live-debug attempts were discarded: the known kobold projectile fault exited GemRB with signal 11 before attachment, and stopping a healthy process under GDB caused repeated fill-batch `EINVAL` errors until a clean core reload; normal untraced operation recovered with no renderer error.

#### Next Steps:

Add opt-in frame-phase timing inside the pinned GemRB source so the main loop separates timers, GUI handling, game/script update, window drawing, SDL buffer rendering, presentation and event polling without ptrace. Use the same AR4000 interval to localize the remaining 36-57ms CPU cost before changing engine behavior or selecting another reusable FPGA operation.

#### Files Modified:

- README.md
- sdl-renderer/noodles/SDL_render_noodles.c

#### Status:

- [x] Built
- [x] Passed

---
