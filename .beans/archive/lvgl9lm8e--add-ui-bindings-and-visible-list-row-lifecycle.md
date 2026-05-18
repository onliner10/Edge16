---
# lvgl9lm8e
title: Add UI bindings and visible list-row lifecycle
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - performance
created_at: 2026-05-18T10:08:05Z
updated_at: 2026-05-18T12:04:29Z
parent: lvgl9nkq3
blocked_by:
    - lvgl93bv3
    - lvgl9w4uk
---

## Objective
Create the shared binding and visible-row lifecycle infrastructure so long-list pages can stop doing hidden-row polling without duplicating per-page fixes.

## Steps
1. Add `radio/src/gui/colorlcd/libui/ui_binding.h` for parent-owned value bindings.
2. Each binding subscribes to one `UiTopic`, reads a current value, compares with the last rendered value, and updates LVGL only on change.
3. Support simple values first: integers, booleans, small equality-comparable structs, and explicit custom comparators for formatted text.
4. Add pause/resume APIs so bindings can be inactive while their row is offscreen.
5. Fold visible-delay behavior into `ListLineButton` or a shared list-row helper.
6. Keep row shells stable for layout/focus; create heavy content on first visibility.
7. Make visible rows activate bindings and hidden rows pause live bindings.
8. Ensure offscreen rows do not sample live channel values.
9. Avoid broad default `Window::checkEvents()` offscreen skipping as the main mechanism; keep compatibility for non-list windows.

## Acceptance criteria
- Long-list row behavior is centralized in shared infrastructure.
- Offscreen rows have no active live-value subscriptions.
- Row size/focus/selection remain stable before and after delayed content creation.
- Binding cleanup is automatic through the owning `Window`.

## Verification
- Add gtests for render-only-on-change, pause/resume, first-visible load, and cleanup on row deletion.
- Run `nix develop -c python3 tools/check-repeated-if-invariants.py radio/src/gui/colorlcd/libui`.
- Run simulator scroll smoke before converting pages, verifying no obvious layout/focus regression.
