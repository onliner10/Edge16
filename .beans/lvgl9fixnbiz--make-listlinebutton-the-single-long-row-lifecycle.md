---
# lvgl9fixnbiz
title: Make ListLineButton the single long-row lifecycle API
status: completed
type: task
priority: critical
tags:
    - lvgl9
    - lists
    - ui-lifecycle
created_at: 2026-05-18T12:07:20Z
updated_at: 2026-05-18T12:52:39Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixe4dj
---

## Objective
Make ListLineButton the only supported lifecycle API for long model-setup rows.

## Implementation requirements
- Centralize first visible row setup in ListLineButton::delayedInit.
- Add protected hooks: onLineLoaded for static children, onLineRefresh for deterministic model/storage data, onLineAfterRefresh for layout adjustment, and onLineLiveUpdate for live changing values.
- Make refresh before first visible load set a pending flag that is applied during first load.
- Keep live updates visible-row-only and never required for initial content.
- Prevent subclasses from overriding delayedInit directly; future rows should override hooks only.

## Acceptance criteria
- refresh() before visible load cannot be lost.
- A new row subclass works by overriding hooks and does not need to remember base delayedInit calls.
- Live-value throttling cannot cause empty initial rows.

## Verification
Add row lifecycle tests for pending refresh, first load without live events, and visible-only live update behavior.
