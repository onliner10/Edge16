---
# lvgl9kc90
title: Convert Flight Modes and Global Variables to event bindings
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - performance
created_at: 2026-05-18T10:08:28Z
updated_at: 2026-05-18T12:04:30Z
parent: lvgl9nkq3
blocked_by:
    - lvgl9lm8e
    - lvgl9ytzo
---

## Objective
Remove long-list lag from Flight Modes and Global Variables by binding visible row state to typed UI topics.

## Steps
1. Convert Flight Mode row current-mode highlight and row values to `ModelFlightModesChanged` bindings.
2. Publish `ModelFlightModesChanged` from flight mode edits and actions that affect displayed row state.
3. Convert Global Variable row active values and flight-mode-dependent display to `ModelGVarsChanged` bindings.
4. Publish `ModelGVarsChanged` from GVar edits and controls that mutate displayed values.
5. Keep row layout, focus, edit actions, and current close-handler behavior intact.
6. Use the shared visible-row lifecycle; do not add page-local offscreen polling hacks.

## Acceptance criteria
- Flight Modes scroll/edit behavior is smooth.
- Global Variables scroll/edit behavior is smooth.
- Current mode/GVar values update correctly after edits.
- Offscreen rows have paused bindings and no live work.

## Verification
- Harness: navigate to Flight Modes, scroll, open/edit a row, close, verify row update.
- Harness: navigate to Global Variables, scroll, edit a value, close, verify visible value update.
- Capture screenshots after meaningful transitions.
- Run native binding/lifetime tests under ASAN.
