## 1 COMMIT Unreleased ??? 2026-09-24T00:38:39-07:00

#### Coming From:

Unreleased e2e47b7

#### Purpose:

Add the first SDL2 Noodles renderer integration and an isolated hardware diagnostic without changing the accepted GemRB launcher path.

#### Outcome:

The planned change will pin and cross-build the accepted MiSTer-Noodles SDK, register an SDL renderer named `noodles`, and implement the protocol-1.3 operations needed for isolated bring-up: ARGB8888 managed textures, updates, render targets, clear and opaque fill, unscaled copy, mirroring, colour and alpha modulation, standard and custom blend modes, readback and presentation through a managed 800x600 composition surface. A deterministic SDL diagnostic will exercise those operations while the packaged GemRB launchers continue to select the existing software renderer.

#### Next Steps:

Implement and cross-build the renderer and diagnostic, validate the build and deterministic host-side checks, then hand the standalone renderer diagnostic to the user for hardware validation before adding synchronization fallbacks or selecting the renderer in GemRB.

#### Files Modified:

- patches/sdl2/noodles-renderer.patch
- scripts/build-deps.sh
- scripts/env.sh
- scripts/build-noodles-test.sh
- sdl-renderer/noodles/SDL_render_noodles.c
- sdl-renderer/noodles/SDL_render_noodles.h
- tools/noodles-render-test.c

#### Status:

- [ ] Built
- [ ] Passed

---
