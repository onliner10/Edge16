---
# lvgl9fixe4dj
title: Make deferred load and event traversal deterministic
status: completed
type: task
priority: high
tags:
    - lvgl9
    - ui-lifecycle
created_at: 2026-05-18T12:07:19Z
updated_at: 2026-05-18T12:52:39Z
parent: lvgl9fixiet2
blocked_by:
    - lvgl9fixzbwa
---

## Objective
Make first visible load deterministic and independent of live-value events.

## Implementation requirements
- Define delayLoad and delayLoadWhenVisible contracts in Window: delayedInit runs at most once, runs on first visible traversal for visibility-delayed widgets, and is canceled on deletion.
- Ensure Window::onLiveCheckEvents remains the default child traversal and visible-load mechanism.
- Audit overrides that own children; each must call base traversal unless the class is explicitly a leaf.
- Add focused checks/tests so child delayedInit cannot silently stop running when a parent overrides onLiveCheckEvents.

## Acceptance criteria
- A visible child using delayLoadWhenVisible loads without any UiEventHub live-value event.
- Missing a live event cannot leave static labels or card titles uncreated.

## Verification
Native test or harness proof for a parent with delayed children and no live events.
