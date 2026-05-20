[![GitHub release (latest by date)](https://img.shields.io/github/v/release/onliner10/Edge16)](https://github.com/onliner10/Edge16/releases/latest)
[![GitHub all releases](https://img.shields.io/github/downloads/onliner10/Edge16/total)](https://github.com/onliner10/Edge16/releases)
[![GitHub license](https://img.shields.io/github/license/onliner10/Edge16)](https://github.com/onliner10/Edge16/blob/main/LICENSE)
[![Commit Tests](https://github.com/onliner10/Edge16/actions/workflows/build_fw.yml/badge.svg)](https://github.com/onliner10/Edge16/actions/workflows/build_fw.yml)
[![Conventional Commits](https://img.shields.io/badge/Conventional%20Commits-1.0.0-%23FE5196?logo=conventionalcommits&logoColor=white)](https://conventionalcommits.org)

# Welcome to Edge16

**Focused TX16S MK2/MK3 firmware fork based on EdgeTX.**

Edge16 is a RadioMaster TX16S MK2/MK3-only fork of EdgeTX. Goal: polished color-LCD UX, practical safety guardrails, and tighter release artifacts for pilots using these two radios.

> [!WARNING]
> Edge16 supports only RadioMaster TX16S MK2 (`tx16s`) and RadioMaster TX16S MK3 (`tx16smk3`). Do not flash Edge16 builds on any other EdgeTX radio.

> [!WARNING]
> Edge16 v1.0.0-alpha.1 is experimental alpha firmware. I have tested it myself and it works for me, but it has not had broad field testing. Back up your radio and model settings, bench-test every model, verify arming/failsafe/RF/telemetry/battery alerts, and be ready to roll back before flying.

## What is new in Edge16 v1.0.0-alpha.1

- **Global top bar setup** — configure persistent top-bar widgets once at radio level, including compact status widgets for time, model, RF, battery, and volume.
- **Battery monitor and guard** — define LiPo packs, match packs by telemetry voltage/cell count, confirm pack choice at startup/replug, track consumed capacity, alert on runtime limits, and block arming until battery state is confirmed when configured.
- **TX16S-focused UI refresh** — dashboard-style home screen, larger touch targets, clearer state indicators, settings search/filter, single-tier quick navigation, swipe gestures, and cleaner model cards.
- **Smoother, more modern screen feel** — Edge16 upgrades the color-screen engine to LVGL 9.5 and changes how drawing work is scheduled, so menus, widgets, and touch interaction feel more fluid on TX16S.
- **TX16S-only release assets** — release zips contain firmware for TX16S MK2/MK3 only plus matching Companion support.

See [Edge16 v1.0.0-alpha.1 features](docs/release/features.md) for details and safety notes, and [How Edge16 differs from EdgeTX](docs/release/edge16-vs-edgetx.md) for the broader delta.

## Build

Contributor builds should use `uv`; build scripts re-exec through `uv run --with-requirements requirements.txt` so CMake sees correct Python dependencies.

Example firmware build:

```sh
FLAVOR=tx16s \
EXTRA_OPTIONS="-DARM_TOOLCHAIN_DIR=/Applications/ArmGNUToolchain/14.2.Rel1/arm-none-eabi/bin/" \
tools/build-gh.sh
```

Supported flavors:

- `tx16s` — RadioMaster TX16S MK2 (`.bin`)
- `tx16smk3` — RadioMaster TX16S MK3 (`.uf2`)

## Project links

- [Releases](https://github.com/onliner10/Edge16/releases)
- [Issues](https://github.com/onliner10/Edge16/issues/new/choose)
- [Discussions](https://github.com/onliner10/Edge16/discussions)
- [Developer documentation](https://onliner10.github.io/Edge16/)

## Upstream and acknowledgements

Edge16 is forked from [EdgeTX](https://github.com/EdgeTX/edgetx). Many firmware, Companion, Lua, SD-card, and documentation components originate from EdgeTX and OpenTX contributors.

Some icon assets provided by [ICONS8](https://icons8.com). Lua documentation site powered with support from [GitBook](https://www.gitbook.com).
