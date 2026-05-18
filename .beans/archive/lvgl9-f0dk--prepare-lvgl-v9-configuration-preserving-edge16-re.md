---
# lvgl9-f0dk
title: Prepare LVGL v9 configuration preserving Edge16 resource limits
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
    - lvgl9-dwz4
---

## Objective

Create a v9-compatible LVGL configuration that preserves Edge16 memory, color, tick, and acceleration policies.

## Result

`radio/src/gui/colorlcd/lv_conf.h` was replaced with a v9.5-targeted config (1019 lines, down from 925) based on the upstream `lv_conf_template.h` from tag v9.5.0.

### Settings ported from the v8 config:

| Setting | v8 macro | v9 macro | Status |
|---|---|---|---|
| Color depth | LV_COLOR_DEPTH = 16 | LV_COLOR_DEPTH = 16 | Same macro |
| Color swap | LV_COLOR_16_SWAP = 0 | LV_COLOR_16_SWAP = 0 | Preserved |
| Screen transp | LV_COLOR_SCREEN_TRANSP = 0 | LV_COLOR_SCREEN_TRANSP = 0 | Preserved |
| Chroma key | LV_COLOR_CHROMA_KEY | LV_COLOR_CHROMA_KEY | Preserved |
| Memory (BOOT) | LV_MEM_CUSTOM=1 → stdlib | LV_USE_STDLIB_MALLOC = LV_STDLIB_CLIB | Ported to v9 enum |
| Memory (firmware) | LV_MEM_CUSTOM=0 + pool | LV_USE_STDLIB_MALLOC = LV_STDLIB_BUILTIN + LV_MEM_POOL_ALLOC | Ported to v9 stdlib wrappers |
| SDRAM pool size | LV_MEM_SIZE (2/4/8 MiB) | LV_MEM_SIZE (2/4/8 MiB, doubled in SIMU) | Preserved identically |
| Pool allocator | LV_MEM_POOL_ALLOC get_lvgl_mem | LV_MEM_POOL_ALLOC get_lvgl_mem | Same function |
| Tick custom | LV_TICK_CUSTOM=1 (lv_tick_source.h) | Dropped (v9 removed LV_TICK_CUSTOM) | Edge16 will call lv_tick_inc() externally |
| Display refresh | LV_DISP_DEF_REFR_PERIOD = 30 | LV_DEF_REFR_PERIOD = 30 | Renamed (macro simplified) |
| Input read period | LV_INDEV_DEF_READ_PERIOD = 30 | Dropped (per-indev in v9) | Handled by driver |
| DPI | LV_DPI_DEF = 130 | LV_DPI_DEF = 130 | Same |
| DMA2D (SIMU off, FW on) | LV_USE_GPU_STM32_DMA2D | LV_USE_DRAW_DMA2D | Renamed + restructured |
| DMA2D CMSIS include | LV_GPU_DMA2D_CMSIS_INCLUDE | LV_DRAW_DMA2D_HAL_INCLUDE | Renamed; H7/H7RS/F4 logic preserved |
| Draw complex (BOOT split) | LV_DRAW_COMPLEX | LV_DRAW_SW_COMPLEX | Renamed; 0 in BOOT, 1 otherwise |
| Shadow cache | LV_SHADOW_CACHE_SIZE = 0 | LV_DRAW_SW_SHADOW_CACHE_SIZE = 0 | Renamed |
| Circle cache | LV_CIRCLE_CACHE_SIZE = 4 | LV_DRAW_SW_CIRCLE_CACHE_SIZE = 4 | Renamed |
| Layer buffer size | LV_LAYER_SIMPLE_BUF_SIZE | LV_DRAW_LAYER_SIMPLE_BUF_SIZE | Renamed (same 24 KiB) |
| Font decls (per-lang) | LV_FONT_CUSTOM_DECLARE / LV_FONT_DEFAULT | Identical | Preserved verbatim |
| BIDI (SIMU on, FW conditional) | LV_USE_BIDI | LV_USE_BIDI | Preserved identically |
| Perf monitor (UI_PERF_MONITOR) | LV_USE_PERF_MONITOR | LV_USE_SYSMON → LV_USE_PERF_MONITOR | Ported to v9 hierarchy |
| Assert (null + malloc) | LV_USE_ASSERT_NULL/MALLOC = 1 | Same | Preserved |
| FATFS (BOOT split) | LV_USE_FS_FATFS | Same | Preserved |
| QRCODE (BOOT split) | LV_USE_QRCODE | Same | Preserved |
| Large coord | LV_USE_LARGE_COORD = 0 | Same | Preserved |
| Font fmt large | LV_FONT_FMT_TXT_LARGE = 0 | Same | Preserved |

### Widget enable/disable (BOOT vs non-BOOT) — all ported:
- Enabled in firmware: ARC, BAR, BUTTON, BUTTONMATRIX, CANVAS, CHECKBOX, IMAGE, LABEL, LINE, SLIDER, SWITCH, TEXTAREA, TABLE
- Extra: KEYBOARD, TILEVIEW
- Explicitly disabled: ANIMIMG, ARCLABEL, CALENDAR, CHART, DROPDOWN, IMAGEBUTTON, LED, LIST, LOTTIE, MENU, MSGBOX, ROLLER, SCALE, SPAN, SPINBOX, SPINNER, TABVIEW, WIN, 3DTEXTURE
- All disabled in BOOT

### New v9 features explicitly disabled:
- All drawing backends: SDL, X11, Wayland, fbdev, NuttX, DRM, TFT_eSPI, Lovyan_GFX, evdev, libinput, OpenGLES, GLFW, QNX, STM32 LTDC, Renesas GLCDC, NXP ELCDIF, Windows, UEFI
- All heavy libraries: FreeType, TinyTTF, RLottie, FFmpeg, GStreamer, ThorVG, NanoVG, glTF, SVG, WebP, libpng, lodepng, libjpeg-turbo, BMP, GIF, TJPGD
- Vector graphics, matrix transforms, 3D textures, Lottie
- All image decoders, file systems except FATFS
- All demos and examples
- Unused widgets: tabview, win, calendar, chart, animimg, etc.
- Observer, profiler, Pinyin, file explorer, font manager
- SDL, X11, Wayland, and all desktop/embedded display backends

## Verification
- `git diff --check` passes (no whitespace errors)
- File has correct header guard and `#if 1` content enable
- No unexpected macros enabled from v9 template
