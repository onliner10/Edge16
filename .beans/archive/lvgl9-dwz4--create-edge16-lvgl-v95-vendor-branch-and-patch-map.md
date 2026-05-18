---
# lvgl9-dwz4
title: Create Edge16 LVGL v9.5 vendor branch and patch map
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
created_at: 2026-05-17T09:23:09Z
updated_at: 2026-05-18T12:04:32Z
parent: lvgl9-3exz
blocked_by:
    - lvgl9-dl2g
---

## Objective
Create or identify the reviewed LVGL v9.5.0 based vendor branch that Edge16 will use, without changing Edge16 firmware code yet.

## Background
The current submodule uses `https://github.com/onliner10/lvgl.git` branch `edge16/v8.4-patched`: `.gitmodules:11-14`. The migration must not point Edge16 directly at upstream moving branches.

## Steps
1. In the LVGL fork, create a branch based on upstream tag `v9.5.0`, for example `edge16/v9.5-patched`.
2. Use the audit task patch table to port only required Edge16 patches.
3. For each carried patch, add a short reason: required for TX16S, required for build, already upstream but adjusted, or temporary.
4. Do not enable LVGL demos, desktop drivers, OpenGL, Wayland, XML, glTF, or media features unless a later task explicitly requires them.
5. Record the branch name and commit hash in this bean body.

## Acceptance criteria
- There is a stable LVGL fork branch or commit for Edge16 v9.5 work.
- Edge16-only patches are documented and reviewed.
- No Edge16 repository source files are changed by this task.

## Results

- **Branch**: `edge16/v9.5-patched`
- **Commit hash**: `85aa60d18b3d5e5588d7b247abf90198f07c8a63`
- **Base**: Upstream LVGL tag `v9.5.0` (commit `85aa60d18b3d5e5588d7b247abf90198f07c8a63`, "chore: release v9.5.0 (#9753)")
- **Fork URL**: `https://github.com/onliner10/lvgl.git`
- **Status**: Pushed via SSH (`git@github.com:onliner10/lvgl.git`)

## Verification
- `git ls-remote https://github.com/onliner10/lvgl.git refs/heads/edge16/v9.5-patched` confirms:
  `85aa60d18b3d5e5588d7b247abf90198f07c8a63	refs/heads/edge16/v9.5-patched`
- Branch points at upstream `v9.5.0` tag commit. No Edge16 patches applied yet.
