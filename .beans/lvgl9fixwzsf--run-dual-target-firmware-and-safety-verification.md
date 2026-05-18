---
# lvgl9fixwzsf
title: Run dual-target firmware and safety verification
status: completed
type: task
priority: critical
tags:
    - lvgl9
    - firmware
    - safety
created_at: 2026-05-18T12:07:21Z
updated_at: 2026-05-18T12:54:04Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixaho1
---

## Objective
Build and safety-check both supported firmware targets after the lifecycle refactor.

## Implementation requirements
- Run targeted UI/lifecycle tests first, then strict firmware builds for tx16s and tx16smk3.
- Run git diff --check and required UI policy tools.
- If any required check cannot run locally, record the exact command and reason in the bean.
- Produce the tx16s firmware artifact path for hardware testing only after checks pass.

## Acceptance criteria
- tx16s and tx16smk3 compile with EDGE16_SAFETY_CHECKS enabled.
- No UI policy check failures.
- Firmware artifact path is recorded for TX16S MK2 hardware testing.

## Verification commands
- nix develop -c python3 tools/check-ui-escape-hatches.py
- nix develop -c python3 tools/check-repeated-if-invariants.py radio/src/gui/colorlcd/libui
- nix develop -c tools/ui-harness/edgetx-ui build tx16s
- nix develop -c tools/ui-harness/edgetx-ui build tx16smk3
- strict firmware build for FLAVOR=tx16s and FLAVOR=tx16smk3
- git diff --check
