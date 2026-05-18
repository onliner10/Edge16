---
# lvgl9-cxba
title: Move input device registration behind the adapter on v8.4
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:09Z
updated_at: 2026-05-18T12:04:33Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-bar9
---

## Objective
Make touch, keypad, and rotary registration use the LVGL compatibility boundary while still on LVGL 8.4.

## Changes made in `radio/src/gui/colorlcd/LvglWrapper.cpp`

1. Added `#include "edgetx_lvgl_adapter.h"` (line 24).
2. Removed static `lv_indev_drv_t` globals (`touchDriver`, `keyboard_drv`, `rotaryDriver`) — these are now managed by the adapter at `edgetx_lvgl_adapter.cpp`. Kept only `lv_indev_t*` device pointers.
3. Replaced `init_lvgl_drivers()` body with adapter calls:
   - `_lv_disp_get_refr_timer()` → `etx::lvgl::etx_lvgl_get_disp_refr_timer()`
   - `lv_timer_set_period()` → `etx::lvgl::etx_lvgl_timer_set_period()`
   - `lv_indev_drv_init()` + field assignment + `lv_indev_drv_register()` → `etx::lvgl::etx_lvgl_indev_create_pointer()` / `_keypad()` / `_encoder()`
   - `touchDriver.read_timer` → `etx::lvgl::etx_lvgl_indev_get_read_timer()`
4. `indev->group` → `etx::lvgl::etx_lvgl_indev_get_group(indev)` (get_focus_obj).
5. `lv_indev_read_timer_cb(indev->driver->read_timer)` → `etx::lvgl::etx_lvgl_indev_read_timer_cb(indev)` (manual timer read in block UI path).
6. Theme manual field setup → `etx::lvgl::etx_lvgl_theme_init()` + `etx_lvgl_disp_set_theme()`.
7. `disp->rendering_in_progress` / `disp->inv_p` → `etx::lvgl::etx_lvgl_disp_rendering_in_progress()` / `etx_lvgl_disp_inv_p()`.

## Not changed
- Callback signatures (`keyboardDriverRead`, `touchDriverRead`, `rotaryDriverRead`) remain using `lv_indev_drv_t *` / `lv_indev_data_t *`.
- Timing constants (`TOUCH_INDEV_READ_PERIOD_MS`, etc.) unchanged.
- `lv_group_get_focused`, `lv_indev_reset`, `lv_indev_wait_release`, `lv_indev_get_next`, `lv_indev_get_scroll_obj`, `lv_indev_get_scroll_dir` — to be migrated later.
- `lcdInitDisplayDriver()` call kept as-is (uses internal display init, not adapter).

## Verification
- `git diff --check` — no whitespace errors.
- `nix develop -c tools/edge16-cpp-lsp check radio/src/gui/colorlcd/LvglWrapper.cpp` — exit code 0 (no errors).
