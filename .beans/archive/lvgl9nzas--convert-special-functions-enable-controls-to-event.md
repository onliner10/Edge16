---
# lvgl9nzas
title: Convert Special Functions enable controls to event bindings
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - controls
created_at: 2026-05-18T10:08:38Z
updated_at: 2026-05-18T12:04:30Z
parent: lvgl9nkq3
blocked_by:
    - lvgl9lm8e
    - lvgl9ytzo
---

## Objective
Keep the fixed Special Functions enable toggle behavior while moving list/edit state updates onto scoped bindings.

## Steps
1. Bind Special Functions list-page enable checkbox state to `SpecialFunctionsChanged`.
2. Bind Special Function edit-page Enable switch state to `SpecialFunctionsChanged`.
3. Publish `SpecialFunctionsChanged` when list checkbox or edit-page switch changes the model.
4. Preserve native LVGL 9 switch/button usage and current alignment fixes.
5. Verify list-page checkbox hit area remains checkable and does not collide with row borders.
6. Verify edit-page Enable pill toggles by touch/key interaction.

## Acceptance criteria
- Adding a new special function and toggling Enable works in the edit page.
- List-page compact checkbox is aligned and checkable.
- State stays synchronized between list and edit paths.
- No raw subscription survives after closing the edit page.

## Verification
- Harness: add a new special function, open edit page, toggle Enable on/off, return to list, toggle list checkbox.
- Capture screenshots for list alignment and edit switch state.
- Run native lifetime tests for closing edit window after subscription.
