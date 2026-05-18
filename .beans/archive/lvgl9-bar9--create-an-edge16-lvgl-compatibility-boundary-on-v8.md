---
# lvgl9-bar9
title: Create an Edge16 LVGL compatibility boundary on v8.4
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
    - lvgl9-dl2g
---

## Results

Created two adapter files:

**edgetx_lvgl_adapter.h** (270 lines) in `radio/src/gui/colorlcd/`:
- `etx::lvgl` namespace with ~30 inline pass-through wrappers
- 7 function declarations implemented in .cpp

**edgetx_lvgl_adapter.cpp** (115 lines) in `radio/src/gui/colorlcd/`:
- Static internal storage for 1 display descriptor and 3 indev drivers
- `etx_lvgl_disp_create()` — combined buffer init + driver init + field assignment + registration
- `etx_lvgl_indev_create_pointer/keypad/encoder()` — indev wrappers
- `etx_lvgl_refr_now()`, `etx_lvgl_get_disp_refr_timer()`, `etx_lvgl_indev_read_timer_cb()`

Also added `edgetx_lvgl_adapter.cpp` to `radio/src/gui/colorlcd/CMakeLists.txt` line 57.

## API surface covered
- Display: create, flush_ready, flush_is_last, refr_now, get_disp_refr_timer, disp_get_default, get_scr_act
- Input devices: create_pointer, create_keypad, create_encoder, get_group, read_timer_cb, get_read_timer
- Timer/anim: timer_handler, set_period, anim_count_running
- Display state: rendering_in_progress, inv_p
- Screen: screen_active, remove_style_all
- Theme: theme_t (alias), disp_set_theme, theme_init, palette_main
- Indev iteration/scroll: get_next, get_scroll_obj, get_scroll_dir, reset, wait_release
- Type aliases: indev_data_t, state constants, key codes
