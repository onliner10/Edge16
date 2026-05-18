---
# lvgl93bv3
title: Add UI signal wrapper and Window-owned connection bags
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - lifetime
created_at: 2026-05-18T10:07:30Z
updated_at: 2026-05-18T12:04:29Z
parent: lvgl9nkq3
blocked_by:
    - lvgl9xbwo
---

## Objective
Hide third-party sigslot APIs behind Edge16 UI lifetime primitives so dangling UI subscriptions are impossible by default.

## Steps
1. Add `radio/src/gui/colorlcd/libui/ui_signal.h` with `UiSignal<T...>`, `UiScopedConnection`, and `UiConnectionBag` wrappers.
2. Make `UiScopedConnection` non-copyable and movable; destruction or reset disconnects exactly once.
3. Add `UiConnectionBag` ownership to `Window`.
4. Add `Window::trackUiConnection(...)` and `Window::disconnectUiConnections()`.
5. Call `disconnectUiConnections()` at the start of `Window::deleteLater()` and defensively in `Window::~Window()`.
6. Ensure code paths that rebuild child-heavy content can disconnect their old bindings before LVGL object deletion.
7. Add a guard or check preventing page/model code from including raw `sigslot` headers directly, if practical.

## Acceptance criteria
- Window-owned subscriptions disconnect before LVGL object deletion.
- Repeated disconnect is safe.
- Moving a connection between bags does not double-disconnect.
- Page code can subscribe without storing raw `sigslot` types.
- No realtime/control-path code includes the UI signal wrapper.

## Verification
- Add native gtests for move, reset, destruction, and `Window::deleteLater()` cleanup.
- Run `nix develop -c python3 tools/check-ui-escape-hatches.py` after Window lifetime edits.
- Run targeted native tests under ASAN.
