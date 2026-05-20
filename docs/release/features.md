# Edge16 v1.0.0-alpha.1 Features

Edge16 v1.0.0-alpha.1 is an experimental alpha release for RadioMaster TX16S MK2 and TX16S MK3.

> [!WARNING]
> This is experimental alpha firmware. It has been tested by the maintainer and works for them, but it has not had broad field testing. Back up radio and model settings before flashing. After updating, bench-test every model with propellers removed or aircraft restrained. Confirm failsafe, arming, RF output, telemetry, mixes, and battery alerts before flight, and be ready to roll back.

## Supported radios

| Radio | Build flavor | Firmware artifact |
|---|---|---|
| RadioMaster TX16S MK2 | `tx16s` | `tx16s-*.bin` |
| RadioMaster TX16S MK3 | `tx16smk3` | `tx16smk3-*.uf2` |

No other radio target is supported.

## Global top bar

Edge16 moves top-bar setup to radio-level configuration so status widgets can stay consistent across models. Top-bar widgets use compact layouts for TX16S color LCD and can show key flight-session state without taking home-screen space.

Typical widgets include:

- model/context status;
- clock/session status;
- RF/link indicators;
- battery status;
- volume/audio state.

## Battery monitor and guard

Battery monitor adds LiPo pack awareness around normal telemetry voltage flow.

Main capabilities:

- Pack library in radio setup with cell count and capacity.
- Per-model compatible pack selection.
- Voltage-driven pack matching from telemetry.
- Startup/replug confirmation prompt when voltage matches known packs.
- Runtime consumed-capacity and voltage alerts.
- Optional arming block until configured battery checks are satisfied.
- Safer fallback behavior when no monitor is enabled or no compatible pack exists.

## UI refresh

Edge16 v1.0.0-alpha.1 includes TX16S-focused color-LCD polish:

- focused dashboard home screen;
- clearer model cards with model context;
- 48 px minimum touch-target work in key screens;
- settings search and filtering;
- single-tier quick navigation/tab bar;
- swipe gesture navigation;
- state indicators on settings cards;
- cleaner date/time picker.

## A smoother, more modern screen experience

Edge16 includes a major refresh of the color-screen engine underneath the TX16S interface. The technical work includes an LVGL 9.5 upgrade, asynchronous screen drawing, font preloading, and rendering-path cleanup — but the important part is what you feel while using the radio:

- screens open with less hesitation;
- scrolling and touch interaction feel more fluid;
- first-time text and icon drawing is less likely to stutter;
- status widgets can update without making the whole interface feel busy;
- heavier visual elements are drawn more efficiently;
- the UI is built on newer foundations for future polish.

In plain language: Edge16 should feel less like a firmware menu and more like a modern touch device, while still staying focused on transmitter safety.

## Release packaging

Release assets are intentionally narrow:

- TX16S MK2 firmware only;
- TX16S MK3 firmware only;
- Companion artifacts with TX16S MK2/MK3 simulator/WASM support;
- no unsupported upstream radio firmware artifacts.
