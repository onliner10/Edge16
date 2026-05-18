---
# lvgl9fixiet2
title: Root-fix LVGL9 UI lifecycle regressions
status: completed
type: epic
priority: critical
tags:
    - lvgl9
    - ui-lifecycle
    - safety
created_at: 2026-05-18T12:06:23Z
updated_at: 2026-05-18T15:04:49Z
---

## Intent
Fix the LVGL9 migration regressions at the UI framework lifecycle layer, not with per-screen hacks.

## Root causes
- Deferred widgets can drop refreshes before first visible load.
- Some screens depend on live-value events to create static UI content.
- Some onLiveCheckEvents overrides skip base traversal, so children never load.
- Window::deleteLater synchronously destroys LVGL objects from callbacks, which is unsafe in LVGL9.
- Model switching can close the selector while continuing to use selector-owned objects.

## Success criteria
- Model selector always shows model names on bitmap and no-picture cards.
- Inputs, Mixes, Outputs, Flight Modes, and Global Variables render initial content without waiting for live events.
- Long-list scrolling remains smooth.
- Repeated model switching and shutdown do not freeze.
- New row widgets use framework hooks that make refresh/lifetime mistakes hard to introduce.

## Safety constraints
No mixer, RF protocol, ADC, trainer, telemetry timing, watchdog, or emergency shutdown behavior changes. UI may coalesce/defer drawing, but control-path work always has priority.
