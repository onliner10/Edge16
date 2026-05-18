---
# lvgl9-3exz
title: Migrate Edge16 UI from patched LVGL 8.4 to LVGL 9.5
status: completed
type: epic
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - tx16s
    - tx16smk3
created_at: 2026-05-17T09:23:07Z
updated_at: 2026-05-18T12:04:26Z
---

## Goal
Upgrade Edge16 from its current patched LVGL 8.4 integration to LVGL v9.5.0 with small, reversible changes and safety gates after every phase.

## Context for a junior developer
- Edge16 currently uses a patched LVGL fork, not plain upstream LVGL: `.gitmodules:11-14`.
- The local LVGL config is explicitly for v8.4.0: `radio/src/gui/colorlcd/lv_conf.h:1-3`.
- The build includes a hand-maintained LVGL source list, including a firmware-only STM32 DMA2D file: `radio/src/gui/colorlcd/CMakeListsLVGL.txt:1-31`, `radio/src/gui/colorlcd/CMakeListsLVGL.txt:54-212`.
- Display setup currently uses v8 driver structs and direct/full refresh settings: `radio/src/gui/colorlcd/lcd.cpp:62-64`, `radio/src/gui/colorlcd/lcd.cpp:372-426`.
- Input setup currently uses v8 input driver structs: `radio/src/gui/colorlcd/LvglWrapper.cpp:37-45`, `radio/src/gui/colorlcd/LvglWrapper.cpp:366-398`.
- The UI task has bounded LVGL pumping that must stay bounded: `radio/src/tasks.cpp:65-119`.

## Non-negotiable safety rules
- Do not change mixer, pulses, telemetry timing, trainer, ADC, watchdog, emergency shutdown, or RF/module output code for this epic.
- Do not disable UI, Lua widgets, touch, rotary, keyboard input, or DMA2D just to make the build pass.
- Keep each task as a small pull request or commit series with a clear rollback path.
- Run the task-specific verification before marking a task complete.
- If a task cannot be completed exactly, stop and write down the blocker in the bean before continuing.

## Definition of done for the epic
- LVGL submodule points to an Edge16-reviewed v9.5.0 based branch.
- `tx16s` and `tx16smk3` firmware builds pass with warnings as errors and safety checks enabled.
- Simulator smoke flows pass and include both UI tree checks and framebuffer screenshots.
- Touch, keyboard, rotary, scroll, dialogs, text input, Lua widgets, and shutdown UI are manually or automatically checked.
- DMA2D is either fully verified on firmware targets or explicitly isolated behind a safe software-rendering fallback with a follow-up blocker.
- All rollback instructions are clear enough to revert to the patched v8.4 line.
