---
# lvgl9-x93h
title: Port Edge16 widgets and Lua LVGL wrappers to LVGL v9 in batches
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:11Z
updated_at: 2026-05-18T12:04:31Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-gku8
    - lvgl9-3sk7
---

## Verification
All widget/Lua porting changes verified across 22 files.

### Changes applied
1. lv_obj_class_t field order (14 files) — reorder event_cb before user_data for v9 struct
2. lv_btn_class→lv_button_class (2 files) — type rename + sizeof
3. LV_IMG_CF_*→LV_COLOR_FORMAT_* (8 files) — ALPHA_8BIT→A8, TRUE_COLOR→RGB565, TRUE_COLOR_ALPHA→RGB565A8
4. lv_img_t→lv_image_t (2 files) — direct cast updates
5. lv_canvas_get_img()→lv_canvas_get_image() (2 files)
6. indev->driver->type→lv_indev_get_type() (2 files)
7. lv_disp_drv_t→lv_display_t (boot_lcd.cpp) — flush callback signature

### v8 API sweep
Searched 13 v8-only patterns across Edge16 code (excluding thirdparty/lvgl): all zero direct hits.

### Acceptance criteria met
- No lv_btn_class, lv_img_t, LV_IMG_CF, indev->driver, lv_disp_drv_t, lv_disp_draw_buf_t remain
- lua_lvgl_widget.h required no changes (uses EdgeTX wrappers)
- No widget class is silently disabled

## Status
Completed.
