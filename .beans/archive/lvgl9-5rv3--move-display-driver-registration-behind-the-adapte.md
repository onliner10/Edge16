---
# lvgl9-5rv3
title: Move display driver registration behind the adapter on v8.4
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:08Z
updated_at: 2026-05-18T12:04:32Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-bar9
---

## Objective
Make `lcd.cpp` use the compatibility boundary for LVGL display registration while still on LVGL 8.4.

## Current behavior to preserve
- LVGL memory pool buffer is static SDRAM: `radio/src/gui/colorlcd/lcd.cpp:43-51`.
- Two full-frame RGB565 buffers are used: `radio/src/gui/colorlcd/lcd.cpp:53-59`.
- v8 display structs are now inside the adapter (not global in lcd.cpp): `radio/src/gui/colorlcd/edgetx_lvgl_adapter.cpp:34-35`.
- Flush callback uses `flushLcd`: `radio/src/gui/colorlcd/lcd.cpp:104-146`.
- Direct mode is selected based on target orientation: `radio/src/gui/colorlcd/lcd.cpp:380-386`.

## Steps
1. Move direct LVGL display struct setup into adapter functions.
2. Keep the same draw buffers, resolution, flush callback, wait callback, full refresh, and direct mode values.
3. Keep `lcdInitDisplayDriver` control flow the same: LVGL init, styles, buffer clear, hardware LCD init, display register, default screen style removal.
4. Do not change `LcdFlushManager` behavior in this task.

## Acceptance criteria
- `lcd.cpp` no longer needs to know the full details of v8 display driver registration except through the adapter.
- All current display behavior remains the same on v8.4.

## Result

### Changes made to `radio/src/gui/colorlcd/lcd.cpp`
1. Added `#include "edgetx_lvgl_adapter.h"` after existing includes (line 41).
2. Removed static globals `disp_buf` and `disp_drv` (were lines 62-63).
3. Replaced `init_lvgl_disp_drv()` with a call to `etx::lvgl::etx_lvgl_disp_create()`.
4. Replaced all `lv_disp_flush_ready(disp_drv)` → `etx::lvgl::etx_lvgl_flush_ready(disp_drv)`.
5. Replaced all `lv_disp_flush_is_last(disp_drv)` → `etx::lvgl::etx_lvgl_flush_is_last(disp_drv)`.
6. Replaced all `lv_refr_now(nullptr)` → `etx::lvgl::etx_lvgl_refr_now()`.
7. Removed `lv_disp_drv_register(&disp_drv)` — registration is now inside the adapter.
8. Replaced `lv_scr_act()` → `etx::lvgl::etx_lvgl_screen_active()`.

### Changes made to `radio/src/gui/colorlcd/lcd.h`
1. Added `#include "edgetx_lvgl_adapter.h"` at line 29.
2. Replaced `lv_disp_flush_ready(drv_)` → `etx::lvgl::etx_lvgl_flush_ready(drv_)` in `LvglFlushToken::complete()`.

### Verification
- `git diff --check` — no whitespace errors.
- `nix develop -c tools/edge16-cpp-lsp check radio/src/gui/colorlcd/lcd.cpp` — clean, no errors.

## Verification
- [x] Run `nix develop -c tools/edge16-cpp-lsp check radio/src/gui/colorlcd/lcd.cpp`.
- [x] Run `git diff --check`.
- [ ] If simulator is already buildable, start tx16s simulator and confirm the main screen appears.
