---
# lvgl9fixaho1
title: Run full UI harness regression for lifecycle fix
status: completed
type: task
priority: critical
tags:
    - lvgl9
    - qa
    - ui-harness
created_at: 2026-05-18T12:07:21Z
updated_at: 2026-05-18T12:52:45Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixngkl
    - lvgl9fixl8ej
    - lvgl9fixqu96
---

## Objective
Prove the refactor fixes the reported regressions with simulator UI coverage.

## Implementation requirements
- Use edge16-ui-harness or CLI fallback with one persistent simulator session per target.
- tx16s coverage: model selector names/images, repeated model switching, Inputs, Mixes, Outputs, Flight Modes, Global Variables, Special Functions enable controls, scrolling under live values, and shutdown flow if supported.
- tx16smk3 coverage: at least smoke coverage for the same navigation paths and screenshots for the highest-risk pages.
- Capture screenshots/tree evidence for before/after pages and record any residual pixel-only differences.

## Acceptance criteria
- No empty Inputs list, delayed Flight Modes/GVars content, missing model names, checkbox/toggle regressions, or long-list scroll lag.
- No simulator freeze during model switching or shutdown flow.

## Verification
Attach or reference harness screenshots/logs in the bean body before marking complete.
