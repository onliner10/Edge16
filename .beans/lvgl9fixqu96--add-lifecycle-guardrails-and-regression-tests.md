---
# lvgl9fixqu96
title: Add lifecycle guardrails and regression tests
status: completed
type: task
priority: high
tags:
    - lvgl9
    - tests
    - guardrails
created_at: 2026-05-18T12:07:20Z
updated_at: 2026-05-18T12:52:45Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixzbwa
    - lvgl9fixnbiz
---

## Objective
Add guardrails so future developers cannot easily reintroduce lifecycle misuse.

## Implementation requirements
- Extend or add checks for forbidden direct lv_obj_del from page/screen code where Window deletion should be used.
- Ensure tools/check-ui-escape-hatches.py still protects raw lv_obj_t leakage outside low-level widgets.
- Keep tools/check-repeated-if-invariants.py passing for libui after lifecycle helpers are introduced.
- Add tests around subscriptions: deleting a window disconnects owned UI/event subscriptions and prevents callbacks after destruction.
- Document the lifecycle rules near Window/ListLineButton APIs, not in scattered screen comments.

## Acceptance criteria
- A junior adding a new long-list row has one obvious hook API and cannot accidentally drop first refresh by omitting a base delayedInit call.
- Static UI construction depending on live-value events is caught by tests or review-local checks.

## Verification
Run check-ui-escape-hatches, check-repeated-if-invariants, and the new lifecycle tests.
