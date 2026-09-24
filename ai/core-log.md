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
