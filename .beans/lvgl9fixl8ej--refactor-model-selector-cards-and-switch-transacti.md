---
# lvgl9fixl8ej
title: Refactor model selector cards and switch transaction
status: completed
type: task
priority: critical
tags:
    - lvgl9
    - model-select
    - freeze
created_at: 2026-05-18T12:07:20Z
updated_at: 2026-05-18T12:52:39Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixzbwa
    - lvgl9fixe4dj
---

## Objective
Fix model selector names/images and make model switching lifetime-safe.

## Implementation requirements
- ModelButton owns its complete visual lifecycle: visible delayed load creates name label, placeholder/image state, and deterministic layering.
- Remove parent-side manual image loading from ModelsPageBody; parent traversal should load children normally.
- Ensure names are visible over both bitmap cards and no-picture placeholders.
- Move model switch sequencing into a helper that copies selected model identity before UI close starts.
- Switch helper order: confirm telemetry if needed, flush current model, load selected model, update current model filename/cell, reload custom screens/topbar, mark/check storage, then request selector close.
- No path may call closeHandler/onCancel and then continue using selector-owned this pointers.

## Acceptance criteria
- Model selector shows names on every card.
- Repeated A -> B -> A model switching does not freeze and active model state updates.
- Failed or canceled switch leaves previous model and UI coherent.

## Verification
UI harness model selector screenshots and repeated model-switch flow; native test where practical for close-during-callback safety.
