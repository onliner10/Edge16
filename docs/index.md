# Edge16 Developer Documentation

Welcome to Edge16 developer documentation.

Edge16 is a TX16S MK2/MK3-only firmware fork based on EdgeTX. This site focuses on building, validating, and releasing Edge16 for RadioMaster TX16S MK2 (`tx16s`) and TX16S MK3 (`tx16smk3`).

> [!WARNING]
> Edge16 release firmware is only for RadioMaster TX16S MK2 and TX16S MK3. Do not flash Edge16 artifacts on other EdgeTX radios. v1.0.0-alpha.1 is experimental alpha firmware: it has been maintainer-tested, but users should back up, bench-test, and be ready to roll back.

## Edge16 v1.0.0-alpha.1 highlights

- Global radio-level top bar setup and compact status widgets.
- Battery monitor/guard with LiPo pack library, telemetry voltage matching, confirmation prompts, capacity alerts, and optional arming block.
- TX16S-focused UI refresh: dashboard, larger hit targets, state indicators, settings search/filter, single-tier navigation, and swipe gestures.
- TX16S color-LCD performance work: async rendering path, font preload, and widget/rendering cleanup.

Read [Edge16 v1.0.0-alpha.1 features](release/features.md) before testing or announcing a release.

## What's in this documentation

- **Release** — public feature summary, Edge16-vs-EdgeTX differences, and pilot safety notes.
- **Building Edge16** — build instructions and CMake options for TX16S MK2/MK3 firmware and the simulator.
- **Hardware Reference** — TX16S-focused hardware and IRQ/DMA reference.
- **Troubleshooting** — recovery notes for failed flashes.

## Quick links

| Section | Description |
|---|---|
| [Release features](release/features.md) | Edge16 v1.0.0-alpha.1 feature and safety summary |
| [Edge16 vs EdgeTX](release/edge16-vs-edgetx.md) | Product and technical differences from upstream EdgeTX |
| [Building — Linux (Ubuntu 24.04)](building/linux-ubuntu-24.04.md) | Recommended local build environment |
| [Unbrick your radio](troubleshooting/unbrick.md) | Recover from failed flash using STM32CubeProgrammer |
| [Radio Specifications](hardware/radio-specs.md) | TX16S MK2/MK3 hardware summary |
