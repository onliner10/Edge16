# How Edge16 differs from EdgeTX

Edge16 is based on EdgeTX, but it is not a general-purpose all-radio EdgeTX distribution. Edge16 v1.0.0-alpha.1 is an experimental focused TX16S MK2/MK3 release with product scope, UI, battery-safety, performance, and verification changes layered on top of upstream EdgeTX.

> [!NOTE]
> This page describes the Edge16 release delta relative to the tracked upstream EdgeTX `main` branch at release-prep time. This alpha is experimental: it has been maintainer-tested, but users should treat it carefully until it has wider field feedback. Some items may later be upstreamed, cherry-picked, or replaced by upstream EdgeTX equivalents.

## Product scope and release policy

- Edge16 supports only RadioMaster TX16S MK2 (`tx16s`) and TX16S MK3 (`tx16smk3`).
- CI, nightly, release-draft, and firmware publishing workflows are constrained to TX16S MK2/MK3 artifacts.
- Public docs and issue templates are trimmed to Edge16 support channels and GitHub-only community links.
- Unsupported upstream radio docs remain archival source material, but are excluded from the published Edge16 docs navigation.

## Battery monitor and guard

Edge16 adds a LiPo-aware battery monitor flow beyond standard EdgeTX telemetry display:

- radio-level LiPo pack library with cell count and capacity;
- per-model compatible pack selection;
- telemetry-voltage matching against compatible packs;
- startup/replug confirmation prompts;
- confirmed-pack/session capacity baseline tracking;
- runtime capacity and voltage alerts;
- optional arming block until battery state is confirmed;
- explicit empty telemetry-source and manual fallback handling;
- UI access from radio setup, model setup, and home/status widgets.

## Arming and control-path safety

- `MODEL_ARMED` switch support with safety-critical idle/output gating.
- Arming throttle-gate fixes and confirmation flow integration with battery guard.
- Additional guards around channel counts, module frames, telemetry packet lengths, special functions, trainer overrides, and protocol edge cases.

## Global top bar and status widgets

- Radio-level top-bar setup rather than per-model-only status layout.
- Top-bar widget move/reorder actions and focus fixes.
- Compact TX16S status widgets, including time, volume, battery/status, RF/link, and model context refinements.
- Top-bar setup navigation fixes, including return-key behavior.

## TX16S UI/UX overhaul

- Focused dashboard-style home screen.
- Native widget card layout updates.
- Model cards that expose more model context.
- Larger touch targets in key flows.
- Settings search/filter work.
- Single-tier quick navigation/tab bar.
- Swipe gesture navigation.
- State indicators on settings cards.
- Semantic color system.
- Wheel-style date/time picker.
- Keyboard and long-list lifecycle refactors for more deterministic interaction.

## Rendering and performance

This is one of the biggest Edge16 differences. EdgeTX already runs on color radios, but Edge16 puts a lot of effort into making the TX16S screen feel calmer, quicker, and more polished.

### What pilots should notice

- Menus and pages should feel less hesitant when they open.
- Touch, scrolling, and widget updates should feel more immediate.
- The first time a screen uses a font or visual element, it is less likely to pause or stutter.
- Status widgets can keep updating without making the whole interface feel overloaded.
- The radio should feel more like a purpose-built modern touch device, not just a classic transmitter menu drawn on a color LCD.

### What changed underneath

- LVGL 9.5 integration work for the color-LCD UI — a newer screen engine for future UI polish.
- Asynchronous LVGL/DMA2D LCD flush path for TX16S/TX16S MK3 — screen drawing can happen with less waiting in the main UI flow.
- STM32 DMA2D configuration and simulator/native exclusions where hardware acceleration is unavailable.
- TX16S LCD rotation DMA buffering optimization.
- Font pre-decompression during boot splash to reduce first-use stutter.
- Canvas-based rendering replacements in heavy widgets.
- Pre-rendered color-editor gradients.
- Telemetry mixer lock-latency reduction.

## Reliability and hardening

- Color-LCD UI lifecycle guardrails and fail-closed construction paths.
- Allocation-failure handling for labels, windows, canvases, icons, bitmap buffers, Lua canvas/buffers, and layout widgets.
- UI raw-LVGL escape-hatch checker and centralized live UI access boundaries.
- Protocol/telemetry fuzz coverage and bounds checks for Crossfire, Ghost, PXX/PXX2, DSMP, Multi, HoTT, MLink, FlySky, FrSky, SBUS, PPM, and related paths.
- Spektrum RF false-alarm fix.
- USB MSC/SD-card/SDIO hardening and fail-stop recovery fixes.
- Simulator crash fixes and sanitizer-found crash fixes.

## Simulator, HIL, and developer tooling

- Curated simulator default setup for TX16S work.
- UI harness with screen summaries, screenshots, touch/key/rotary control, telemetry injection, battery voltage injection, switch sequences, audio history, and flow automation.
- Battery monitor automation support in the UI harness.
- Autonomous HIL PPM soak harness.
- Pi/Serena/compile-check/check presets and safety-review tooling for repeatable firmware verification.
- Additional static checks for UI escape hatches, repeated-if invariants, safe division, and firmware safety policies.

## Companion and packaging presentation

- Edge16 visible Companion/Simulator names, project links, issue links, and Linux desktop/AppImage labels.
- Companion support is release-scoped to TX16S MK2/MK3 simulator/WASM modules.

## Compatibility note

Edge16 v1.0.0-alpha.1 is the public alpha release label. Internal firmware/storage compatibility versioning remains upstream-compatible where it affects settings conversion, Companion capabilities, or YAML storage semantics. Do not change storage semver solely to match the public Edge16 tag without auditing conversion paths.
