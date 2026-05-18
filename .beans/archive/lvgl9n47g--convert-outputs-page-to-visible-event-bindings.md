---
# lvgl9n47g
title: Convert Outputs page to visible event bindings
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - performance
created_at: 2026-05-18T10:08:17Z
updated_at: 2026-05-18T12:04:30Z
parent: lvgl9nkq3
blocked_by:
    - lvgl9ytzo
    - lvgl9lm8e
---

## Objective
Make Model Setup / Outputs smooth by replacing row polling/live updates with visible-only bindings.

## Steps
1. Convert output row labels and metadata refresh to `ModelOutputsChanged` bindings.
2. Convert output channel bars and live value text to `LiveChannelValues` bindings.
3. Publish `ModelOutputsChanged` from output edits and actions that mutate output data.
4. Ensure visible rows update immediately after edits and offscreen rows stay paused.
5. Preserve row layout, checkbox/toggle behavior, context menus, and edit-window close refresh.
6. Search for similar output widget/live-value paths and avoid duplicating page-specific polling fixes.

## Acceptance criteria
- Switching to Outputs is not noticeably slower than other model setup pages.
- Outputs scrolling remains smooth from top to bottom and back.
- Visible channel bars/text continue updating.
- Offscreen output rows do not sample live channel values.
- Output edits publish targeted updates instead of relying only on broad refresh.

## Verification
- Harness: navigate to Model Setup / Outputs, scroll top-to-bottom and bottom-to-top, open an output edit, change a value, close, and verify visible row updates.
- Compare screenshot/UI tree against expected LVGL 9 state.
- Run relevant native UI tests under ASAN.
