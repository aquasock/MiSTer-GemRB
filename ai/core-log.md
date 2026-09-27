## 1 COMMIT Unreleased 4c49074 2026-09-26T17:08:41-07:00

#### Coming From:

Unreleased f0e2276

#### Purpose:

Remove the ineffective traversability-loading patch and isolate the remaining first post-click stall.

#### Outcome:

The completed 100-entry project log was archived as `archived_logs/core-log-20260926T170841-0700.tar.gz` before this required handoff. Source `4c49074` removes patch `0011` after hardware validation reported no visible change. The remaining ten-patch stack applies cleanly to pristine GemRB 0.9.5 and exactly reproduces the development tree; the ARM build and normal bundle completed successfully, reproducing the previously accepted core-library SHA256 `41bff74fbd092da652242652e392b3945524ab77d52d77459389bde56b47aded` and normal launcher SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`. The active MiSTer session remains untouched and still runs the rejected test build until the next reboot and deployment.

#### Next Steps:

Ask the user to reboot, deploy the reproduced accepted bundle, and prepare the existing tracer and sampler without launching. After the user confirms they are watching, capture one official replay and separate the first post-click frame's engine work, blocked time and symbolized call sites from the later timing-variable asset stalls.

#### Files Modified:

- patches/0011-initialize-traversability-cache-during-area-load.patch

#### Status:

- [x] Built
- [ ] Passed

---

## 2 COMMIT Unreleased 4c49074 2026-09-26T17:14:34-07:00

#### Coming From:

Unreleased 4c49074

#### Purpose:

Record the deployed traversability-patch rollback and leave a clean agent handoff.

#### Outcome:

After the user rebooted the MiSTer, the reproduced prior bundle from source `4c49074` was deployed without launching GemRB or attaching diagnostics. The device matches the accepted core-library SHA256 `41bff74fbd092da652242652e392b3945524ab77d52d77459389bde56b47aded`, normal launcher SHA256 `216fc0445bdf8ef049bfe5f8e5b010b622fab009e8d7df2f03d42d128694b6d3`, official replay SHA256 `4d42358d3ea17121acd8e33ec7ab36a0cf41d2f41686b90c25258df4aae1e630` and timing-qualified FPGA-core SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe`. The target retains normal SDL audio, `CapFPS=30`, `DrawFPS=0` and normal logging, has no GemRB, replay, tracer or sampler process active, and is ready for the next agent.

#### Next Steps:

The next agent should treat the repeatable opening movement stall and the zero-to-three timing-variable later stalls as separate symptoms. Patch `0011` is rejected and removed; use the official no-panning replay unchanged, tell the user before every launch, and isolate work remaining in the first post-click frame without assuming that the measured traversability allocation is the visible cause.

#### Files Modified:

None.

#### Status:

- [x] Built
- [x] Passed

---

## 3 COMMIT Unreleased 4c49074 2026-09-26T17:55:31-07:00

#### Coming From:

Unreleased 4c49074

#### Purpose:

Correct the FPGA-core qualification record and restore the accepted pre-burst Noodles image after a panning regression.

#### Outcome:

Cross-project artifact verification corrects entry 2: RBF SHA256 `29df3cc992220ba04ca171e598918080e498b1e6aff20f28f924630cedaebebe` came from intermediate Noodles source `93d20a3` seed 2 and failed slow-corner setup, so it was never timing-qualified. Noodles entry 19 later replaced it with the genuinely timing-qualified source `812f4ea` seed-2 RBF SHA256 `d2439fe6bc4d0bf237d3c7cf7795b7fdaafd26491519e3d0b67dd0ff3ef1cd78`; the user tested that final candidate and reported slower viewport panning. The approved recovery is to restore the previously hardware-qualified pre-burst source `f97ce70` seed-2 RBF SHA256 `ae2cdf45e0322f0d91bafeba481594b57c6d65c80fa444256a0153338e849cc6` and end the GemRB work without pursuing a selectable burst implementation.

#### Next Steps:

Commit this correction, let the MiSTer-Noodles recovery restore and validate the canonical RBF, record its exact deployed hash and checks in a new entry, and leave the accepted GemRB bundle and official replay unchanged with no further optimization cycle open.

#### Files Modified:

None.

#### Status:

- [x] Built
- [ ] Passed

---
