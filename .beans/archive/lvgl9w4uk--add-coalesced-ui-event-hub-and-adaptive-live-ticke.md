---
# lvgl9w4uk
title: Add coalesced UI event hub and adaptive live ticker
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - performance
created_at: 2026-05-18T10:07:54Z
updated_at: 2026-05-18T12:04:29Z
parent: lvgl9nkq3
blocked_by:
    - lvgl93bv3
---

## Objective
Add a UI-thread event hub that coalesces typed UI topics once per LVGL tick, plus a bounded live-value ticker that never calls from mixer/control paths into UI.

## Steps
1. Add `radio/src/gui/colorlcd/libui/ui_events.{h,cpp}`.
2. Define `UiTopic` values: `GlobalRefresh`, `ModelOutputsChanged`, `ModelFlightModesChanged`, `ModelGVarsChanged`, `SpecialFunctionsChanged`, `LiveChannelValues`, and `PageVisibilityChanged`.
3. Implement `UiEventHub::subscribe`, `publish`, optional legacy `emitNow`, and `flush`.
4. Coalesce repeated publishes by topic and store latest payload.
5. Make publishes during `flush()` defer to the next UI tick to prevent unbounded reentrancy.
6. Call the live ticker and `UiEventHub::flush()` from `MainWindow::runUiTick()` after model widget refresh and before the legacy `checkEvents()` pass.
7. Add `UiLiveTicker` that starts only while visible live bindings exist and publishes `LiveChannelValues` from UI-owned sampling.
8. Use adaptive rates: about 30 ms active, 50 ms idle, 100 ms armed+idle.

## Acceptance criteria
- Event processing happens on the UI thread only.
- Multiple dirty events before a frame cause one signal emission per topic.
- Publishing from a callback cannot recurse indefinitely.
- Live ticker stops when no visible live binding is registered.
- No mixer/control file includes or calls the UI event hub.

## Verification
- Add gtests for coalescing, deferred publish during flush, and live-subscriber reference counting.
- Run ASAN native tests.
- Run simulator and confirm regular UI interaction still works before any page conversion.
