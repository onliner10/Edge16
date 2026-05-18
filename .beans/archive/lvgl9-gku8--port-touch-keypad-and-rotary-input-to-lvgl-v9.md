---
# lvgl9-gku8
title: Port touch, keypad, and rotary input to LVGL v9
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:10Z
updated_at: 2026-05-18T12:04:27Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-cxba
    - lvgl9-f0dk
    - lvgl9-thjq
---

## Objective
Make all Edge16 user input devices work with LVGL v9 through the adapter.

## Current behavior to preserve
- Keyboard event conversion: `radio/src/gui/colorlcd/LvglWrapper.cpp:123-154`.
- Keyboard fallback to EdgeTX windows: `radio/src/gui/colorlcd/LvglWrapper.cpp:156-219`.
- Touch cancel, backlight disabled, disable-touch function, and release behavior: `radio/src/gui/colorlcd/LvglWrapper.cpp:221-293`.
- Rotary diff and acceleration behavior: `radio/src/gui/colorlcd/LvglWrapper.cpp:316-358`.
- Adaptive work detection for input and scrolling: `radio/src/gui/colorlcd/LvglWrapper.cpp:529-563`.

## Steps
1. Implement v9 adapter functions for pointer, keypad, and encoder creation.
2. Port read callbacks without changing their logic.
3. Replace any v8 direct field access such as `indev->group` or `indev->driver->read_timer` with v9-safe adapter calls.
4. Confirm scroll detection still works or replace it with an equivalent v9 public API.
5. Preserve touch, keyboard, and rotary active-until timing.

## Acceptance criteria
- Keypad navigation opens and closes menus correctly.
- Touch press, release, cancel, and disabled-touch states behave as before.
- Rotary navigation and acceleration still work.
- Adaptive UI pump still detects input and scroll work.

## Verification
- Run simulator smoke flow.
- Use UI automation to press keys and open at least one menu.
- Use touch/rotary simulator actions if available.
- Capture UI tree and screenshot after navigation.
