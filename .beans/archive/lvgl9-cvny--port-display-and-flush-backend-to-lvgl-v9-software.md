---
# lvgl9-cvny
title: Port display and flush backend to LVGL v9 software rendering first
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:10Z
updated_at: 2026-05-18T12:04:27Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-kn2o
    - lvgl9-5rv3
    - lvgl9-f0dk
    - lvgl9-thjq
---

## Objective
Make the Edge16 display backend work with LVGL v9 using the software rendering path first. Do not re-enable firmware DMA2D in this task unless the software path is already stable.

## Current behavior to preserve
- Two full-frame buffers: `radio/src/gui/colorlcd/lcd.cpp:51-57`.
- Flush token guarantees flush completion: `radio/src/gui/colorlcd/lcd.h:119-157`.
- `LcdFlushManager` state machine and timeout behavior: `radio/src/gui/colorlcd/lcd.h:327-364`, `radio/src/gui/colorlcd/lcd.cpp:159-323`.
- Refresh helper behavior: `radio/src/gui/colorlcd/lcd.cpp:346-364`.
- Display initialization flow: `radio/src/gui/colorlcd/lcd.cpp:395-426`.

## Steps
1. Update the adapter v9 display implementation to use LVGL v9 display APIs.
2. Keep flush callback ownership clear: every LVGL flush must complete exactly once.
3. Keep bounded waiting in `LcdFlushManager::drain` and timeout handling.
4. Start with software rendering and simulator/native build.
5. Validate default screen cleanup or replacement for the v9 screen API.
6. Only after simulator display works, prepare notes for DMA2D follow-up.

## Acceptance criteria
- Main screen renders in simulator with LVGL v9 software rendering.
- No infinite busy state in `lcdFlushIsBusy`.
- No skipped or double flush-ready calls.
- No changes to mixer/control paths.

## Verification
- Run tx16s simulator build.
- Start tx16s simulator and confirm startup reaches main screen.
- Capture a framebuffer screenshot and verify visible UI.
- Run `git diff --check`.
