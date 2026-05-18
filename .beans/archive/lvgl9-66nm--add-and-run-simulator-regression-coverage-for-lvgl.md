---
# lvgl9-66nm
title: Add and run simulator regression coverage for LVGL v9 migration
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:11Z
updated_at: 2026-05-18T12:04:27Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-gku8
    - lvgl9-x93h
---

## Objective
Prove that the LVGL v9 UI is usable through simulator automation, not just that it compiles.

## Required scenarios
- Startup to main screen.
- Storage warning dismissal if shown.
- Quick menu open and close.
- Model setup page navigation.
- Scroll-heavy page navigation.
- Dialog open, confirm, cancel, and close.
- Text input using simulator keyboard path.
- Touch press/release/cancel where harness supports it.
- Rotary navigation where harness supports it.
- Lua widget page or representative Lua widget script.
- Shutdown screen path.

## Steps
1. Build the tx16s simulator.
2. Run existing smoke flow first.
3. Add or extend flows only where existing coverage is missing.
4. For each scenario, record both UI tree evidence and framebuffer screenshot evidence.
5. Repeat for tx16smk3 simulator if supported by the harness.

## Acceptance criteria
- Every required scenario is checked or explicitly marked not supported by the harness.
- At least one screenshot is captured after a meaningful UI transition.
- UI tree and screenshot agree on the visible page/state.

## Verification
- Run `nix develop -c tools/ui-harness/edgetx-ui build tx16s`.
- Run `nix develop -c tools/ui-harness/edgetx-ui run-flow tools/ui-harness/flows/tx16s-smoke.json`.
- Run equivalent tx16smk3 checks if available.
