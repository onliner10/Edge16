---
# lvgl9nkq3
title: Refactor LVGL UI updates to sigslot event bindings
status: completed
type: epic
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - performance
created_at: 2026-05-18T10:07:01Z
updated_at: 2026-05-18T12:04:28Z
---

## Goal
Finish the LVGL 9 migration performance/lifetime refactor by replacing broad recursive UI polling on long pages with sigslot-backed, window-owned event bindings.

Related migration epic: `lvgl9-3exz`.

## Why this matters
Outputs, Flight Modes, and Global Variables are still laggy after the LVGL 9 port because many list rows keep doing live UI work even when only a small viewport is visible. The fix should be architectural: use scoped event subscriptions, visible row bindings, and a bounded UI-owned live-value ticker instead of scattering per-page polling guards.

## Safety constraints
- This is UI-layer work only. Do not change mixer, pulses, telemetry timing, trainer, ADC, watchdog, emergency shutdown, or RF/module output paths.
- Mixer/radio control must never call into UI event code or synchronously emit UI events.
- UI live values may be sampled only from the UI side at bounded adaptive rates.
- Every subscription must be owned by an explicit UI lifetime owner and auto-disconnect on `Window::deleteLater()` and `Window::~Window()`.
- Do not expose raw sigslot APIs in page code; wrap them in Edge16 UI abstractions.

## Definition of done
- `palacaze/sigslot` is vendored under an MIT license note and verified under Edge16 firmware flags.
- Existing `Messaging` no longer uses an unsafe global raw-pointer list.
- New `UiEventHub`, `UiBinding`, and visible list lifecycle abstractions exist in `libui`.
- Outputs, Flight Modes, Global Variables, and Special Functions use bindings where they currently lag or risk stale polling.
- Visible live rows update correctly; offscreen rows do not sample live values.
- UI regression covers lists, number input, toggles, text input, bitmaps, splash/model images, and page switching latency.
- `tx16s` and `tx16smk3` simulator builds pass; strict firmware verification is run before final close.
