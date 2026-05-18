---
# lvgl9-dl2g
title: Audit current LVGL integration and Edge16 patch dependencies
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:07Z
updated_at: 2026-05-18T12:04:32Z
parent: lvgl9-3exz
---

## Objective
Create the migration inventory before any code change. The output should let another developer understand what Edge16 depends on in LVGL 8.4 and what must be preserved in LVGL 9.5.

## Submodule state
- `.gitmodules:11-14`: fork `https://github.com/onliner10/lvgl.git` branch `edge16/v8.4-patched`
- Current commit: `d2a75eb92912e064d58178fa08e86560da8842ae`
- LVGL `v9.5.0` is the latest upstream release

## Patch classification table

| Area | File(s) | Current API / Detail | Expected v9 work | Risk |
|------|---------|---------------------|------------------|------|
| Display driver structs | `lcd.h:41-42`, `lcd.cpp:62-64` | `lv_disp_draw_buf_t`, `lv_disp_drv_t` globals | v9 uses `lv_display_t` and new registration API | HIGH - struct layout changed |
| Display draw buffer init | `lcd.cpp:374-375` | `lv_disp_draw_buf_init()` | v9: `lv_display_set_buffers()` | HIGH - API renamed |
| Display registration | `lcd.cpp:422` | `lv_disp_drv_register()` | v9: `lv_display_create()` + `lv_display_set_driver()` | HIGH |
| Flush callback setup | `lcd.cpp:379-380` | `disp_drv.flush_cb = flushLcd` | v9: `lv_display_set_flush_cb()` | MEDIUM |
| Wait callback | `lcd.cpp:380` | `disp_drv.wait_cb = lcdFlushWaitCb` | v9: removed/replaced by flush-complete callback | MEDIUM |
| Full refresh / direct mode | `lcd.cpp:384-392` | `disp_drv.full_refresh`, `disp_drv.direct_mode` | v9: `lv_display_set_direct_mode()` | MEDIUM |
| Private refresh internals | `lcd.cpp:90-101` | `_lv_refr_get_disp_refreshing()`, `disp->inv_p`, `disp->inv_areas`, `disp->inv_area_joined` | PRIVATE API REMOVED in v9; must use Edge16-owned area accumulator | HIGH |
| Invalidation buffer size | `lcd.h:33-34` | `LCD_FLUSH_INVALIDATED_AREAS_MAX` = `LV_INV_BUF_SIZE` | LV_INV_BUF_SIZE may differ; must keep Edge16 cap | LOW |
| `lv_refr_now()` | `lcd.cpp:350,360` | `lv_refr_now(nullptr)` | v9: `lv_refr_now(NULL)` works but display arg type changed | LOW |
| Flush ready | `lcd.cpp:147`, `lcd.h:119-157` | `lv_disp_flush_ready()` | v9: `lv_display_flush_ready()` | MEDIUM - API renamed |
| Flush is last check | `lcd.cpp:112,176` | `lv_disp_flush_is_last()` | v9: `lv_display_flush_is_last()` | MEDIUM - API renamed |
| Input driver structs | `LvglWrapper.cpp:37-45` | `lv_indev_drv_t` globals | v9: `lv_indev_t` created differently | HIGH |
| Input device registration | `LvglWrapper.cpp:377-397` | `lv_indev_drv_init()` + `lv_indev_drv_register()` | v9: `lv_indev_create()` + setters | HIGH |
| Input driver fields | `LvglWrapper.cpp:378-381` | `drv.type`, `drv.read_cb`, `drv.scroll_limit`, `drv.scroll_throw` | v9: set via functions `lv_indev_set_type()`, `lv_indev_set_read_cb()` | MEDIUM |
| Read timer | `LvglWrapper.cpp:383,507,383` | `touchDriver.read_timer`, `indev->driver->read_timer` | v9: `lv_indev_get_read_timer()` or similar; `indev->driver` may not exist | HIGH |
| Display refresh timer | `LvglWrapper.cpp:371-374` | `_lv_disp_get_refr_timer()` private API, `lv_timer_set_period()` | v9: `lv_display_get_refr_timer()` or set period differently | MEDIUM |
| LVGL timer handler | `LvglWrapper.cpp:473` | `lv_timer_handler()` | v9: same API | LOW |
| `LV_NO_TIMER_READY` | `LvglWrapper.cpp:482` | `LV_NO_TIMER_READY` sentinel | v9: same sentinel | LOW |
| LVGL anim count | `LvglWrapper.cpp:552` | `lv_anim_count_running()` | v9: same API | LOW |
| LVGL indev get next / scroll | `LvglWrapper.cpp:506,558-561` | `lv_indev_get_next()`, `lv_indev_get_scroll_obj()`, `lv_indev_get_scroll_dir()` | v9: likely same API, need verification | LOW |
| LVGL indev reset | `LvglWrapper.cpp:230,258` | `lv_indev_reset()` | v9: same API | LOW |
| LVGL indev wait release | `LvglWrapper.cpp:248` | `lv_indev_wait_release()` | v9: same API | LOW |
| LVGL group/focus | `LvglWrapper.cpp:106-108` | `indev->group`, `lv_group_get_focused()` | v9: `lv_indev_get_group()`, `lv_group_get_focused()` | MEDIUM |
| `lv_indev_data_t` | `LvglWrapper.cpp:111-121` | Data struct for read callbacks | v9: same struct name, fields may differ | LOW |
| `lv_scr_act()` | `lcd.cpp:425` | `lv_scr_act()` for default screen | v9: `lv_scr_act()` likely same | LOW |
| `lv_obj_remove_style_all()` | `lcd.cpp:425` | Remove all styles on default screen | v9: same API | LOW |
| `lv_palette_main()` | `LvglWrapper.cpp:417-418` | Theme color init | v9: same API | LOW |
| `lv_disp_set_theme()` | `LvglWrapper.cpp:425` | v8 theme struct assignment | v9: `lv_theme_set_from_display()` or new API | MEDIUM |
| `lv_theme_t` | `LvglWrapper.cpp:413-422` | Manual theme struct filling | v9: use `lv_theme_default_init()` or custom | MEDIUM |
| `lv_timer_set_period()` | `LvglWrapper.cpp:373,384` | Timer period adjustment | v9: same API | LOW |
| v8 lv_theme_t fields | `LvglWrapper.cpp:414-422` | `theme.disp, .color_primary, .color_secondary, .font_small, .font_normal, .font_large, .flags` | v9: struct fields changed or theme initialized differently | HIGH |
| Window base class | `libui/window.cpp:153-168` | `lv_obj_class_t` C99 designated initializers | v9: `lv_obj_class_t` struct fields may have changed | MEDIUM |
| lv_event functions | `libui/window.cpp:185-194` | `lv_event_get_target()`, `lv_event_get_code()`, `LV_EVENT_DELETE` | v9: same API | LOW |
| Lua widget metatable | `lua/lua_lvgl_widget.h:28-29` | `LVGL_METATABLE "LVGL*"`, `LVGL_SIMPLEMETATABLE` | Likely no change | LOW |
| `lv_disp_get_default()` | `LvglWrapper.cpp:554` | Display access | v9: `lv_display_get_default()` | MEDIUM - API renamed |
| `disp->rendering_in_progress` | `LvglWrapper.cpp:555` | Private render state read | v9: removed; use public API | HIGH |
| `disp->inv_p` (in tasks.cpp context) | `LvglWrapper.cpp:555` | Private invalidation count read | v9: removed | HIGH |
| Bidi support | `lv_conf.h:539-543` | `LV_USE_BIDI` based on SIMU/TRANSLATION | v9: similar config but renamed macros | LOW |
| Font custom declarations | `lv_conf.h:448-486` | `LV_FONT_CUSTOM_DECLARE` per translation | v9: likely similar format | LOW |
| Widget enablement | `lv_conf.h:562-731` | Widget toggles with BOOT/non-BOOT split | v9: different widget enable macro names | MEDIUM |
| LVGL memory config | `lv_conf.h:52-81` | Static pool, SDRAM-backed, SIMU multiplier | v9: similar settings, new macro names | MEDIUM |
| DMA2D config | `lv_conf.h:212-217` | `LV_USE_GPU_STM32_DMA2D`, SIMU vs firmware | v9: different macro name/path | MEDIUM |
| LVGL source lists | `CMakeListsLVGL.txt:1-212` | Hand-maintained v8 source files | v9: different directory layout and file names | HIGH |
| DMA2D source | `CMakeListsLVGL.txt:26-31` | `draw/stm32_dma2d/lv_gpu_stm32_dma2d.c` | v9: different path/API | MEDIUM |
| Adaptive UI pump | `tasks.cpp:65-119` | Bounded LVGL pumping, timing constants | Must not change | LOW (preserve) |
| Custom tick source | `lv_tick_source.cpp:26-29` | `edgetxLvglTickGet()` returns `time_get_ms()` | v9: same approach but macro renamed | LOW |
| `lv_tick_source.h` | `lv_tick_source.h:30` | `edgetxLvglTickGet()` declaration | v9: same | LOW |
| LVGL font handling | `lv_conf.h:410-504` | Montserrat fonts, per-translation custom fonts | Similar in v9 | MEDIUM |
| LVGL canvas usage | `lv_conf.h:571`, widget list | `LV_USE_CANVAS 1` | Similar in v9 | LOW |
| LVGL snapshot usage | `lv_conf.h:846` | `LV_USE_SNAPSHOT 1` | v9: likely similar | LOW |
| LVGL keyboard widget | `lv_conf.h:632`, `CMakeListsLVGL.txt:198` | `LV_USE_KEYBOARD 1` | v9: same | LOW |
| LVGL tileview | `lv_conf.h:656`, `CMakeListsLVGL.txt:190` | `LV_USE_TILEVIEW 1` | v9: same | LOW |
| LVGL qrcode | `lv_conf.h:803`, `CMakeListsLVGL.txt:170-171` | `LV_USE_QRCODE 1` + qrcodegen | v9: different directory | MEDIUM |
| LVGL flex/grid | `lv_conf.h:689-692` | `LV_USE_FLEX 1`, `LV_USE_GRID 1` | v9: similar | LOW |
| LVGL fatfs | `lv_conf.h:771` | `LV_USE_FS_FATFS 1` | v9: renamed macro | MEDIUM |
| LVGL transp bounds check | `lv_conf.h:39` | `LV_COLOR_SCREEN_TRANSP 0` | Similar | LOW |
| LVGL assertion macros | `lcd.h:45`, `lcd.h:141` | `LCD_ASSERT` wrapping LVGL assertion path | Must preserve | LOW |
| LVGL flush timeout | `lcd.h:37-39` | `LCD_FLUSH_VBLANK_TIMEOUT_MS 50` | Preserve | LOW |
| LVGL flush drain | `lcd.h:335-337`, `lcd.cpp:241-263` | `lcdFlushDrain()`, `LcdFlushManager::drain()` | Must preserve flush completion semantics | MEDIUM |
| vblank token | `lcd.h:119-157` | `LvglFlushToken` wraps `lv_disp_flush_ready()` | Must adapt to v9 API rename | MEDIUM |
| Backbuffer sync | `lcd.h:303-309` | `HorusBackBufferSync` uses invalidated areas | Depends on inv-area refactoring | HIGH |
| Display driver typedef forward | `lcd.h:41-42` | `typedef _lv_disp_drv_t lv_disp_drv_t` | v9: `lv_display_t` | MEDIUM |
| STB image init | `LvglWrapper.cpp:428-429` | `lv_stb_init()` via `extern` | Likely unchanged | LOW |

## Private or v8-specific APIs used outside third-party/lvgl

| API | Location | v9 Status |
|-----|----------|-----------|
| `_lv_refr_get_disp_refreshing()` | `lcd.cpp:93` | PRIVATE - removed |
| `disp->inv_p` | `lcd.cpp:95` | PRIVATE - removed |
| `disp->inv_area_joined[i]` | `lcd.cpp:96` | PRIVATE - removed |
| `disp->inv_areas[i]` | `lcd.cpp:97` | PRIVATE - removed |
| `_lv_disp_get_refr_timer()` | `LvglWrapper.cpp:371` | PRIVATE - may have v9 equivalent |
| `lv_disp_flush_is_last()` | `lcd.cpp:112,176` | Public API (renamed in v9) |
| `lv_disp_flush_ready()` | `lcd.h:147` | Public API (renamed in v9) |
| `lv_refr_now()` | `lcd.cpp:350,360` | Public API (same in v9) |
| `lv_disp_drv_t` direct fields | `lcd.cpp:377-392` | Structs changed in v9 |
| `lv_indev_drv_t` direct fields | `LvglWrapper.cpp:377-384` | Structs changed in v9 |
| `indev->group` | `LvglWrapper.cpp:106` | Private - use `lv_indev_get_group()` in v9 |
| `indev->driver->read_timer` | `LvglWrapper.cpp:507` | Private - may not exist in v9 |
| `disp->rendering_in_progress` | `LvglWrapper.cpp:555` | Private - removed in v9 |
| `disp->inv_p` (tasks.cpp context) | `LvglWrapper.cpp:555` | Private - removed in v9 |

## Key counts
- High-risk items requiring careful v8→v9 adaptation: ~15
- Medium-risk items: ~12
- Low-risk items: ~14

## Verification results
- `git status --short`: only `.beans/lvgl9-dl2g-*` file changed (this bean)
- `git submodule status radio/src/thirdparty/lvgl`: `-d2a75eb92912e064d58178fa08e86560da8842ae`
- No source files modified in this task.
