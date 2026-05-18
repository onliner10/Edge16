---
# lvgl9-thjq
title: Rebuild the curated LVGL v9 CMake source lists
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
Create `radio/src/gui/colorlcd/CMakeListsLVGL-v9.txt` with v9-equivalent LVGL source paths, leaving the v8 file untouched.

## Summary
Created `radio/src/gui/colorlcd/CMakeListsLVGL-v9.txt` (270 lines) with:

### LVGL_SOURCES_MINIMAL_V9 — bootloader minimal list
Maps the v8 minimal sources to v9 paths:
- core/ — `lv_refr.c` (kept), `lv_display.c` (was `lv_disp.c` + `hal/lv_hal_disp.c`), `lv_tick.c` (was `hal/lv_hal_tick.c`), new `lv_global.c`
- draw/ — Image files renamed (`lv_draw_image.c`, `lv_image_buf.c`, `lv_image_cache.c`, `lv_image_decoder.c`), removed `lv_draw_mask.c` and `lv_draw_triangle.c`, new `lv_draw_buf.c`
- draw/sw/ — unchanged from v8
- draw/stm32_dma2d/ — kept with note about possible v9 rename
- font/ — unchanged
- misc/ — added new `lv_cache.c`, `lv_cache_entry.c`; kept all v8 files
- stdlib/ — commented out with notes (new v9 directory, to be enabled if linker requires)
- BIDI conditional — same as v8

### LVGL_SOURCES_V9 — full color LCD UI list
Extends the minimal list with:
- core/ — all v8 core files, `lv_indev.c` now absorbs `hal/lv_hal_indev.c`
- draw/ — `lv_draw_layer.c`, `lv_draw_arc.c`, `lv_draw_sw_gradient.c`
- misc/ — all v8 misc files preserved
- widgets/ — `lv_image.c` (was `lv_img.c`), `lv_button.c` (was `lv_btn.c`); tileview and keyboard moved from `extra/widgets/` to `widgets/tileview/` and `widgets/keyboard/`
- stdlib/snapshot/ — `lv_snapshot.c` (was `extra/others/snapshot/`)
- layouts/ — `lv_grid.c` and `lv_flex.c` at top level (were under `extra/layouts/`)
- libs/ — qrcode and fsdrv (were under `extra/libs/`); alternative stdlib/ paths noted
- All SDL, extra font, extra lib, other extra widget, and theme entries commented out as in v8, with paths updated to v9 locations

### Marked uncertainties (30+ comment annotations)
- The `misc/lv_cache*` system is new — exact set of required files (core + entry, plus LRU/RB variants) needs submodule verification
- `stdlib/lv_mem_core.c`, `stdlib/lv_string.c`, `stdlib/lv_sprintf.c` — commented out, may be required at link time
- QR code files may be under `libs/qrcode/` or `stdlib/qrcode/` — both paths documented
- DMA2D file may be renamed in v9 — alternative path noted
- `draw/lv_image_cache.c` may be fully superseded by `misc/lv_cache*` — needs verification

### File structure
- Base dir: `thirdparty/lvgl/src` (same as v8)
- `-O3` per-file compile flags preserved
- v8 file unchanged (`CMakeListsLVGL.txt` stays as-is)

## Acceptance criteria status
- [ ] CMake configure succeeds with the LVGL v9 submodule — **pending v9 submodule checkout**
- [ ] Build errors are real API porting issues, not missing source-list files — **pending verification**
- [ ] Bootloader/minimal and full UI source lists remain separate — **done**

## Next steps
- Check out v9 vendor branch commit 85aa60d18b3d5e5588d7b247abf90198f07c8a63
- Run CMake configure with `FLAVOR=tx16s` to verify all source paths exist
- Uncomment/remove commented entries based on actual file listing
- Run `git diff --check`
