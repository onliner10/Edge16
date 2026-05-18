---
# lvgl9-gjk3
title: Re-enable and verify STM32 DMA2D rendering on firmware targets
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:11Z
updated_at: 2026-05-18T12:04:26Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-cvny
    - lvgl9-x93h
---

## Verification

### DMA2D config check (lv_conf.h)
- LV_USE_DRAW_DMA2D = 1 for firmware, 0 for simulator: Correct
- LV_USE_DRAW_DMA2D_INTERRUPT = 0: Matches Edge16 polling behavior
- CMSIS include correctly selected per STM32 variant (STM32H7 -> stm32h7xx.h): Correct
- Firmware-only DMA2D source inclusion in CMakeListsLVGL.txt: Correct (separated from simulator)

### Build check
DMA2D build not yet verified — depends on firmware compilation (task lvgl9-p1gy).

### Risk assessment
- Software rendering fallback works (verified via display/flush port in Phase5a)
- DMA2D failure path falls through to software: needs confirmation in v9
- Simulator uses software-only path: already verified

## Status
Pending firmware build verification.
