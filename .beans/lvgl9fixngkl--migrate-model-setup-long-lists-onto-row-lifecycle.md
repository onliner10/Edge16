---
# lvgl9fixngkl
title: Migrate model setup long lists onto row lifecycle
status: completed
type: task
priority: critical
tags:
    - lvgl9
    - model-setup
    - lists
created_at: 2026-05-18T12:07:20Z
updated_at: 2026-05-18T12:52:39Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixnbiz
---

## Objective
Move existing model setup long lists onto the new ListLineButton lifecycle so they render immediately and scroll smoothly.

## Implementation requirements
- Inputs: deterministic source/weight/options/flight-mode content goes in onLineRefresh; no required static content may wait for live events.
- Mixes: deterministic content goes in onLineRefresh; group height adjustment goes in onLineAfterRefresh.
- Flight Modes: child label/widget creation goes in onLineLoaded; value population goes in onLineRefresh.
- Outputs and Global Variables: static children in onLineLoaded, model data in onLineRefresh, live channel/value changes only in onLineLiveUpdate.
- Audit similar long rows, including Special Functions where applicable, and migrate any matching pattern.

## Acceptance criteria
- Inputs, Mixes, Outputs, Flight Modes, Global Variables, and matching long-list pages show complete initial content within the first visible frame.
- Scrolling remains responsive because offscreen rows do not perform live work.
- No duplicated per-screen workaround for dropped refresh remains.

## Verification
UI harness screenshots/tree checks for each listed page immediately after opening and during scroll.
