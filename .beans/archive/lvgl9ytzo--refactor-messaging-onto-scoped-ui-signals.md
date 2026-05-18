---
# lvgl9ytzo
title: Refactor Messaging onto scoped UI signals
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - lifetime
created_at: 2026-05-18T10:07:40Z
updated_at: 2026-05-18T12:04:31Z
parent: lvgl9nkq3
blocked_by:
    - lvgl93bv3
---

## Objective
Replace the current global raw-pointer `Messaging` subscription list with scoped signal connections while preserving the existing public API and dispatch behavior.

## Steps
1. Refactor `radio/src/gui/colorlcd/libui/messaging.{h,cpp}` to use the UI signal wrapper internally.
2. Preserve `Messaging::subscribe(id, callback)`, `unsubscribe()`, `send(id)`, and `send(id, data)`.
3. Preserve current newest-subscriber-first dispatch order, or document and test any deliberate change before making it.
4. Ensure `Messaging::~Messaging()` disconnects automatically through the scoped connection path.
5. Ensure callback self-disconnect and callback deleting the owner are safe.
6. Search all existing `Messaging` uses and confirm no call site depends on stale global-list behavior.

## Acceptance criteria
- No `static std::list<Messaging*> subscriptions` or equivalent raw-owner global list remains.
- Existing `Messaging` call sites compile unchanged.
- Destroying a subscribed object cannot leave a dangling callback in the bus.
- Existing refresh/menu/color/module messages still work.

## Verification
- Add gtests for newest-first order, self-disconnect, owner deletion during dispatch, and destructor unsubscribe.
- Run focused native tests under ASAN.
- Run simulator smoke path that opens quick menu, color preview/update, and at least one page using `Messaging::REFRESH`.
