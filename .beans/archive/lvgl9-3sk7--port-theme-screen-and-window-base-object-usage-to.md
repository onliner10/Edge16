---
# lvgl9-3sk7
title: Port theme, screen, and Window base object usage to LVGL v9
status: completed
type: task
priority: normal
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:10Z
updated_at: 2026-05-18T12:04:28Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-cvny
---

## Objective
Port the basic Edge16 UI object model to LVGL v9 after display and input work.

## Current areas to inspect
- Manual theme setup in `radio/src/gui/colorlcd/LvglWrapper.cpp:409-426`.
- Main window loading in `radio/src/gui/colorlcd/LvglWrapper.cpp:400-432`.
- Window base LVGL object class in `radio/src/gui/colorlcd/libui/window.cpp:153-168`.
- Window event handling in `radio/src/gui/colorlcd/libui/window.cpp:185-220`.

## Steps
1. Replace v8 theme initialization with v9-supported theme/default style setup.
2. Replace default screen APIs with v9 equivalents where needed.
3. Update `window_base_class` initialization for LVGL v9 object class structure.
4. Keep `Window::window_event_cb` and user-data lookup behavior safe under v9.
5. Verify object deletion and event callbacks cannot access destroyed `Window` objects.

## Acceptance criteria
- MainWindow loads successfully.
- A basic page with nested windows renders.
- Delete events do not call into destroyed C++ windows.
- No new global ownership leaks are introduced.

## Verification
- Run `nix develop -c python3 tools/check-ui-escape-hatches.py`.
- Run simulator and navigate through at least two screens.
- Run `git diff --check`.
