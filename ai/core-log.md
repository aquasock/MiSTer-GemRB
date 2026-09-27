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
