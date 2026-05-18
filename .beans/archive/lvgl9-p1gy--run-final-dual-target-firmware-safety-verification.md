---
# lvgl9-p1gy
title: Run final dual-target firmware safety verification and rollback review
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:11Z
updated_at: 2026-05-18T12:04:28Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-gjk3
    - lvgl9-66nm
---

## Objective
Complete the LVGL v9 migration with full Edge16 safety verification and a clear rollback path.

## Required checks
1. `git diff --check`.
2. `nix develop -c python3 tools/check-safe-division.py`.
3. `nix develop -c python3 tools/check-ui-escape-hatches.py`.
4. `nix develop -c python3 tools/check-repeated-if-invariants.py radio/src/gui/colorlcd/libui`.
5. `tools/commit-tests.sh` for `FLAVOR=tx16s` with warnings as errors and safety checks.
6. `tools/commit-tests.sh` for `FLAVOR=tx16smk3` with warnings as errors and safety checks.
7. Strict firmware build for `tx16s`.
8. Strict firmware build for `tx16smk3`.
9. Semgrep firmware policy scan excluding third-party, translations, and tests as in project guidelines.

## Rollback review
- Confirm the LVGL submodule pointer can be reverted to the previous patched v8.4 commit.
- Confirm all adapter changes are either compatible with v8 or isolated enough to revert cleanly.
- Confirm no control-path files were touched.

## Acceptance criteria
- All required checks pass or any skipped check has a clear reason and exact command for a human to run.
- The final diff is limited to LVGL integration, UI/Lua/widget code, simulator flows, build config, and the LVGL submodule pointer.
- The safety assessment is `SAFE` or the merge is blocked.

## Verification
- Paste command outputs or CI links into this bean before marking completed.
- Mark this bean completed only after both supported targets are verified.
