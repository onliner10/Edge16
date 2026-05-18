---
# lvgl9m2ug
title: Audit remaining long-list polling and migrate matching pages
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - performance
created_at: 2026-05-18T10:08:51Z
updated_at: 2026-05-18T12:04:29Z
parent: lvgl9nkq3
blocked_by:
    - lvgl9lm8e
---

## Objective
Apply the shared event/binding design holistically by finding other long-list or live-update pages with the same polling/lag risk and migrating them through the shared infrastructure.

## Steps
1. Search for `delayLoad()`, `checkEvents()`, `onLiveCheckEvents()`, `ChannelBar`, live value reads, and list-row subclasses under `radio/src/gui/colorlcd`.
2. Identify pages that match the Outputs/Flight Modes/GVars pattern: many retained rows, expensive live updates, or hidden-row redraw work.
3. Migrate matching pages through `UiBinding` and visible-row lifecycle only when they share the same design problem.
4. Avoid broad unrelated rewrites and avoid touching non-colorlcd or control-path code.
5. Document any intentionally deferred page in this bean with the reason and risk.

## Acceptance criteria
- No obvious duplicated per-page polling fix remains for long retained lists.
- Matching pages use the shared lifecycle/binding mechanism.
- Deferred pages have written rationale.
- Scroll performance win is preserved across converted pages.

## Verification
- Run `rg` searches listed in the steps and record findings.
- Harness smoke representative converted pages beyond Outputs/Flight Modes/GVars/Special Functions if any are migrated.
- Run `check-repeated-if-invariants.py` for `libui`.
