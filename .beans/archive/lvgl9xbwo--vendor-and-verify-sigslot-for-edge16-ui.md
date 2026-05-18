---
# lvgl9xbwo
title: Vendor and verify sigslot for Edge16 UI
status: completed
type: task
priority: high
tags:
    - lvgl
    - migration
    - ui
    - firmware-safety
    - dependency
created_at: 2026-05-18T10:07:17Z
updated_at: 2026-05-18T12:04:31Z
parent: lvgl9nkq3
---

## Objective
Vendor `palacaze/sigslot` as the event subscription core for the color LCD UI layer, then prove it works with Edge16 firmware constraints before any UI architecture depends on it.

## Steps
1. Add fixed upstream `sigslot` release, preferably latest stable `v1.2.3`, under `radio/src/thirdparty/sigslot`.
2. Include upstream MIT license and add `radio/src/thirdparty/sigslot/README.edge16.md` with upstream URL, tag/commit, license, and local verification notes.
3. Add the include path only to the color LCD UI build, not globally.
4. Compile a minimal `sigslot::signal<>` usage under Edge16 firmware flags.
5. Confirm it does not require exceptions, RTTI, unsupported compiler features, or runtime initialization unsuitable for firmware.
6. Stop and update this bean if the library fails the spike; do not silently replace it with custom code.

## Acceptance criteria
- `sigslot` is vendored with license/provenance notes.
- `tx16s` and `tx16smk3` builds can include and compile a minimal signal/slot use.
- No realtime/control-path file includes `sigslot`.
- No project code outside the UI wrapper directly depends on third-party signal types.

## Verification
- Run `nix develop -c tools/edge16-cpp-lsp setup tx16s` if compile database is stale.
- Run focused simulator or firmware compile that includes the minimal wrapper.
- Run `git diff --check` for the vendored/provenance files.
