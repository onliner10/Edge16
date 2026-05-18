---
# lvgl9-kn2o
title: Remove private LVGL invalidated-area reads before v9 port
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:08Z
updated_at: 2026-05-18T12:04:28Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-bar9
---

## Objective
Eliminate direct reads of LVGL private refresh/display internals while still on LVGL 8.4.

## Why this matters
`radio/src/gui/colorlcd/lcd.cpp:90-101` calls `_lv_refr_get_disp_refreshing()` and reads `disp->inv_p`, `disp->inv_area_joined`, and `disp->inv_areas`. These are private/internal v8 details and are likely to break or behave differently in LVGL 9.

## Files likely involved
- `radio/src/gui/colorlcd/lcd.cpp:90-146`
- `radio/src/gui/colorlcd/lcd.cpp:159-204`
- `radio/src/gui/colorlcd/lcd.h:58-80`
- `radio/src/gui/colorlcd/lcd.h:159-189`

## Design requirement
Collect invalidated or flushed rectangles through Edge16-owned state during the flush callback instead of reading LVGL internals. The final `LcdFlushChunk` must still carry enough area information for Horus/TX16S back-buffer sync to work correctly.

## Steps
1. Add an Edge16-owned accumulator for areas seen during a refresh cycle.
2. On each flush callback, add the flushed area to the accumulator.
3. When `lv_disp_flush_is_last` or adapter equivalent reports the final chunk, pass the accumulated areas into `LcdFlushChunk::finalChunk`.
4. Clear the accumulator after the final chunk or after a safe timeout path.
5. Preserve the existing overflow behavior from `LcdInvalidatedAreas::add` at `radio/src/gui/colorlcd/lcd.h:63-71`.

## Acceptance criteria
- No `_lv_refr_get_disp_refreshing` usage remains in Edge16 code outside third-party LVGL.
- No direct `disp->inv_*` reads remain in Edge16 code outside third-party LVGL.
- Framebuffer synchronization behavior is unchanged in simulator smoke testing.

## Verification
- Search for `_lv_refr_get_disp_refreshing` and direct `inv_p` usage outside `radio/src/thirdparty/lvgl`.
- Run `nix develop -c tools/edge16-cpp-lsp check radio/src/gui/colorlcd/lcd.cpp`.
- Run the tx16s simulator smoke flow if the build is available.
