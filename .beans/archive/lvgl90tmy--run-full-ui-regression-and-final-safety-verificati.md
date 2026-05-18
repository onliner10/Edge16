---
# lvgl90tmy
title: Run full UI regression and final safety verification for sigslot event refactor
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - qa
created_at: 2026-05-18T10:12:35Z
updated_at: 2026-05-18T12:04:31Z
parent: lvgl9nkq3
blocked_by:
    - lvgl9n47g
    - lvgl9kc90
    - lvgl9nzas
    - lvgl9m2ug
---

## Objective
Prove the sigslot event-refactor has no UI regressions and does not affect safety-critical control paths before closing the epic.

## Steps
1. Run native lifetime/binding tests under ASAN/UBSAN.
2. Run `tx16s` and `tx16smk3` simulator builds.
3. Use the UI harness to regression-test Outputs, Flight Modes, Global Variables, Special Functions, generic lists, number input, switches/toggles, text input, dialogs, splash screen, model images, and bitmap rendering.
4. Compare key screenshots against the known-good main version where practical; allow only small expected LVGL 9 pixel differences.
5. Run UI lifetime/policy checks.
6. Run strict firmware builds for both supported targets if final code touches shared UI lifecycle or build integration.
7. Record commands, results, failures, and any deferred risk in this bean before marking complete.

## Acceptance criteria
- Outputs, Flight Modes, and Global Variables page switching/scrolling are smooth.
- Special Functions list checkbox and edit-page Enable switch work.
- Bitmap/splash/model-image rendering remains correct.
- Lists, number input, toggles, text input, and dialogs behave correctly.
- No dangling subscription, callback-after-delete, ASAN, UBSAN, or UI escape-hatch issue is found.
- `tx16s` and `tx16smk3` verification passes or any blocker is documented with exact reproduction steps.

## Verification commands
- `nix develop -c tools/ui-harness/edgetx-ui build tx16s`
- `nix develop -c tools/ui-harness/edgetx-ui build tx16smk3`
- `nix develop -c python3 tools/check-ui-escape-hatches.py`
- `nix develop -c python3 tools/check-repeated-if-invariants.py radio/src/gui/colorlcd/libui`
- `git diff --check`
- Strict firmware build for `tx16s` and `tx16smk3` using the project AGENTS.md command when code is ready.
