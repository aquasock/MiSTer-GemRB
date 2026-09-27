## 1 COMMIT Unreleased ??? 2026-09-26T17:08:41-07:00

#### Coming From:

Unreleased f0e2276

#### Purpose:

Remove the ineffective traversability-loading patch and isolate the remaining first post-click stall.

#### Outcome:

The completed 100-entry project log was archived as `archived_logs/core-log-20260926T170841-0700.tar.gz` before this required handoff. Remove patch `0011` because hardware validation reported no visible change, rebuild the normal GemRB bundle, and use the existing bounded tracer and main-thread sampler only around the official replay's first in-game click so no new diagnostic binary or FPGA build is required.

#### Next Steps:

Commit and build the rollback, deploy it after the active GemRB session has ended, then prepare the target without launching. After the user reboots and confirms they are watching, capture one official replay and separate the first post-click frame's engine work, blocked time and symbolized call sites from the later timing-variable asset stalls.

#### Files Modified:

- patches/0011-initialize-traversability-cache-during-area-load.patch

#### Status:

- [ ] Built
- [ ] Passed

---
