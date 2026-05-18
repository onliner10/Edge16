---
# lvgl9fixzbwa
title: Stabilize Window/LVGL destruction lifecycle
status: completed
type: task
priority: critical
tags:
    - lvgl9
    - ui-lifecycle
    - freeze
created_at: 2026-05-18T12:07:19Z
updated_at: 2026-05-18T12:52:39Z
parent: lvgl9fixiet2
---

## Objective
Make Window destruction safe for LVGL9 callbacks and async object deletion.

## Implementation requirements
- Replace synchronous lv_obj_del usage in Window::deleteLater with a two-phase path: detach/hide/disable now, schedule lv_obj_delete_async, free C++ wrappers only after a later LVGL handler pass.
- Cancel pending delayed loads and disconnect UI/event subscriptions before clearing LVGL user_data.
- Make Window::clear route Edge16-owned child widgets through the same deferred deletion path instead of synchronously cleaning live LVGL trees during callbacks.
- Update shutdown cleanup to use the same bounded drain path; do not immediately free wrappers that own canvas/image buffers after scheduling async LVGL deletion.

## Acceptance criteria
- Deleting a widget from its own click/long-press callback cannot reenter synchronous LVGL deletion.
- C++ buffers used by LVGL canvas/image objects outlive pending async LVGL deletes.
- Repeated deleteLater calls are idempotent.

## Verification
Add/update lifecycle tests for callback deletion, pending delayed load cancellation, and two-generation trash behavior.
