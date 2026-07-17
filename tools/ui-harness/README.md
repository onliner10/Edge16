# EdgeTX UI Harness

This harness gives agents and CI a repeatable control plane for TX16S-class
simulator UI work.

The current backend starts the SDL simulator with `--automation-stdio`. The
automation protocol runs inside the simulator process and uses `simuSetKey`,
`simuRotaryEncoderEvent`, `simuTouchDown`, `simuTouchUp`, and `simuLcdCopy`.
Screenshots are copied from the simulator framebuffer, then converted to PNG by
the Python harness.

## Usage

```sh
nix develop
tools/ui-harness/edgetx-ui build tx16s
tools/ui-harness/edgetx-ui build tx16smk3
tools/ui-harness/edgetx-ui smoke --target tx16s
tools/ui-harness/edgetx-ui run-flow tools/ui-harness/flows/tx16s-smoke.json
tools/ui-harness/edgetx-ui review-report --shot build/ui-harness/screenshots/home.png::"Home screen looks correct"
tools/ui-harness/edgetx-mcp
```

`edgetx-mcp` is a stdio MCP server. It exposes build/start/stop, key, rotary,
touch, wait, screenshot, status, live UI-tree, screen summary, selector click,
selector long-click, visibility assertion, wait-for, storage-warning skip,
run-flow, and screenshot review-report tools. It uses the same Python core as
the CLI.

### Headless by default

On Linux the harness launches the simulator with `SDL_VIDEODRIVER=dummy`: all
interaction happens over the automation protocol and screenshots come from the
simulator framebuffer, so no window is needed. This avoids a WSLg failure mode
where an unresponsive compositor blocks SDL inside `XIfEvent` during window
mapping (before the automation loop starts), which surfaces as
`timed out waiting for simulator response to 'status'` — especially with
several agents running simulators concurrently. To watch the simulator window
while the harness drives it, set `EDGETX_UI_SHOW_WINDOW=1`; an explicitly set
`SDL_VIDEODRIVER` is always respected.

### Agent-friendly patterns

Prefer `edgetx_orient` for the default low-token route-first summary, and use
`edgetx_sitemap` once per session to learn stable navigable routes. Fall back to
`edgetx_screen` or `edgetx_ui_tree mode=summary` only when route/focus context is
not enough. Click/assert results are compact by default; use `verbose=true` for
full node details. Route navigation is available through `edgetx_goto` for
stable firmware-owned pages; direct actions remain limited to visible,
user-reachable controls and radio inputs.

```text
edgetx_start_simulator target=tx16s
edgetx_status                                 # if startup_blocker is present, use its skip_tool
edgetx_skip_storage_warning_if_present        # bounded ENTER/action attempts for storage warnings
edgetx_sitemap                               # one-time route graph + editor templates
edgetx_orient                                # tiny route/title/focus/capabilities summary
edgetx_goto route=model.mixes                # route-first navigation
edgetx_screen                                # richer accessibility-style summary when needed
edgetx_ui_tree mode=summary actionable_only=true  # only clickable nodes
edgetx_activate automation_id=model.model2.yml
edgetx_adjust_field label=Cells target_value=4
edgetx_type_text text=PACK1                  # only when a virtual keyboard is visible
edgetx_scroll direction=down amount=page    # real touch-drag on visible scrollable content
edgetx_wait_for text_contains="Throttle" timeout_ms=3000
edgetx_screenshot name=after-model-click
```

### Tool reference

`edgetx_sitemap` — Firmware-owned route graph plus any verified parameterized
editor route templates. Fetch once per session and cache it.

`edgetx_orient` — Smallest recommended state read: `{route,title,mode,focus,can,
hash,blocker?}`. Use after most actions instead of re-reading the full screen.

`edgetx_goto` — Navigate by stable firmware route id, for example
`route=model.mixes` or `route=radio.hardware`. This avoids repeated
observe-click-observe loops.

`edgetx_inspect` — Targeted current-screen details instead of the full tree:
fields, actions, and/or visible text with optional filtering.

`edgetx_ui_tree` — LVGL tree. mode=summary returns labeled/actionable nodes with
compact fields. mode=full returns the full tree. Use `actionable_only`,
`text_contains`, `limit`, `verbose` to filter.

`edgetx_screen` — Richer accessibility-style summary:
`{route,route_valid,route_title,title,page,context,focused,screen_hash,actions,fields,scrollables,available_inputs,next_actions,visible_text,counts}`.
Text is normalized for matching and display. Use this when route/focus context
is not enough and you need visible values or actions. Blocking startup dialogs
report `context.type=blocking_dialog`, include `skip_tool`, and put
`edgetx_skip_storage_warning_if_present` first in `next_actions`. Visible form
rows are summarized as `fields`, for example
`{label:"Cells", value:"3", kind:"number", editable:true, runtime_id:"..."}`.
Focused fields use `context.type=field_focus`; ambiguous edit states use
`context.type=field_edit` with guidance for rotary, `ENTER`, `EXIT`, or text
entry.

`edgetx_status` — Simulator status and LCD geometry. If startup is not complete
because a storage warning is visible, the result includes `startup_blocker` with
`skip_tool=edgetx_skip_storage_warning_if_present` so agents know how to proceed
without repeatedly clicking the dialog.

`edgetx_activate` — User-equivalent semantic activation of a currently visible
node. It accepts `semantic_id`, `automation_id`, `id`/`runtime_id`, `role`,
`text`, or `text_contains`, and invokes only a declared `click` or `long_click`
action. It does not infer nearby controls, walk to hidden screens, or perform
multi-step navigation. If a label is visible but not actionable, activation
fails with the visible node context. Dialog actions labeled "Press any key" warn
that `edgetx_skip_storage_warning_if_present` or `edgetx_press ENTER` may be
more reliable than pointer clicks. If activation does not visibly change the
screen, the result includes `changed=false` and a warning, with field-specific
guidance when the target looks like a form value.

`edgetx_adjust_field` — Bounded user-equivalent field adjustment. It requires a
visible field label and target value, activates the visible value, uses rotary
steps until the target value is visible, and optionally confirms with `ENTER`.
It does not write model data directly and returns `ok=false` if the value does
not converge within `max_steps`. Numeric fields require numeric-only target
values, for example use `target_value="3000"` for a `Capacity` field whose
`kind` is `number`; `3000mAh` is rejected before any UI input is sent.

`edgetx_type_text` — Text entry through the active color-LCD keyboard/editor
path. It only runs when `edgetx_screen.context.type=field_edit`, replaces the
active editor text, and optionally submits the editor. If no field edit context
is visible, it fails with guidance instead of modifying model data directly.

`edgetx_scroll` — User-equivalent scroll gesture. By default it drags the main
visible content viewport, which avoids noisy LVGL internals such as tiny labels
that are technically scrollable. A selector can target a specific node when that
is under test. It reports the gesture and screen hashes; it does not call hidden
LVGL scroll setters or navigate to unreachable screens. If the gesture does not
change the screen, the result includes `changed=false` and a warning.

`edgetx_wait_for` — Poll until a node appears. Combines wait+ui_tree+assert.
Returns `found`, `node`, `screen_hash`, `changed` — avoids repeated
wait/tree/assert loops.

`edgetx_skip_storage_warning_if_present` — Bounded helper for startup storage
warnings. It prefers the radio-key path (`ENTER`), may try the visible dialog
action as a fallback, and returns `ok=false` plus `final_blocker` instead of
leaving agents to guess after repeated failed attempts.

`edgetx_click` / `edgetx_long_click` — Compact result by default:
`{ok, matched:{role,text,automation_id,actions,runtime_id}, runtime_id}`. Full
node with `verbose=true`.

`edgetx_status compact=true` — Removes temporary `sdcard` and `settings` paths
from status output.

JSON is compact (no indentation) for large results (ui_tree, screen, screenshot,
log, audio_history, run_flow). Other tools return pretty JSON.

Selector preference: `automation_id` > exact `text` > `text_contains` >
`role` > raw coordinates. `id` refers to the runtime pointer (lv:...).
`semantic_id` is an alias for `automation_id` when present, and `runtime_id` is
an alias for `id`. Selector `activate`, `click`, and `long_click` invoke on the
menu/UI thread without fixed sleeps; raw `touch` and `drag` remain available
when touch timing is under test. JSON flows are best treated as replay artifacts
after the interactive path is known.

Default sessions copy these fixtures into a temporary runtime directory before
starting the simulator, so smoke runs do not modify tracked fixture files. See
"Fixtures vs. fresh storage" below for the full contract.

The root `pyproject.toml` mirrors the Python build dependencies used by
EdgeTX's existing `requirements.txt`. Running inside the Nix dev shell gives
CMake a Python with Pillow, clang bindings, lz4, jinja2, and the other scripts
dependencies.

The harness writes simulator builds under `build/ui-harness` by default. Set
`EDGETX_UI_BUILD_ROOT=/tmp/edgetx-ui-build` to use a separate build root.

### Faster rebuilds with ccache

`edgetx-ui build` (and the underlying `configure_command()`) passes
`-DCMAKE_C_COMPILER_LAUNCHER=ccache` / `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`
automatically whenever `ccache` is on `PATH` (the Nix dev shell provides it).
This is opt-out, not opt-in, because agent worktrees are cloned fresh often
and a from-scratch simulator build takes minutes even on a capable machine;
measured on this repo, a clean `tx16s` simulator build took ~400s with a cold
ccache and ~33s for an identical clean rebuild with a warm ccache (100% cache
hit rate) -- see `proof/p3-ccache/` for the timing logs. Set
`EDGETX_UI_NO_CCACHE=1` to disable it (e.g. when deliberately measuring
cold-cache build time, or working around a broken ccache installation).

## Agent guide

This section is the accumulated set of "gotchas" from running this harness
against real UI work. Read it before assuming a failure is a harness bug.

### Verify UX with real touch, not click/activate

`edgetx_click` / `edgetx_long_click` / `edgetx_activate` resolve a selector to
an LVGL node and then synthesize `LV_EVENT_CLICKED` / `LV_EVENT_LONG_PRESSED`
directly on it (`radio/src/targets/simu/ui_automation.cpp`,
`lv_obj_send_event(node, ...)`). This **bypasses the real input pipeline
entirely** -- no coordinates, no hit-testing, no touch-down/up state machine,
no gesture recognizer. It is the right tool for reliable, coordinate-free test
automation (navigate to a screen, drive a flow, assert state), but it cannot
tell you whether a button's hit target is big enough, whether a swipe
gesture is recognized, or whether two adjacent controls are hard to hit
independently on real hardware.

For any question that is actually about touch UX -- "is this tappable",
"does this gesture register", "is the touch target big enough" -- use
`edgetx_touch` / `edgetx_drag` (or `edgetx_scroll`, which drives a real drag
gesture) with real screen coordinates, the same way `simuTouchDown` /
`simuTouchUp` are driven by an actual finger on hardware. Treat
`edgetx_click` results as "the app state changed correctly", not "a user
could actually do this".

### Rollers are swipe/drag controls, not click targets

LVGL rollers (`lv_roller`, used by the number-wheel/roller pickers in
`radio/src/gui/colorlcd/libui/number_wheel.cpp`) are spun by a vertical drag
gesture (or the rotary encoder), not by tapping an option directly. Do not
expect `edgetx_click` on a roller option node to behave like tapping a list
item -- use `edgetx_adjust_field` (which drives the field via rotary steps,
same as a physical radio) or a real `edgetx_drag`/`edgetx_scroll` gesture
targeting the roller if you specifically need to test the swipe interaction.

### Scroll-settle caution

`edgetx_scroll` drags the viewport (or a target node) and then does a single
fixed `wait(150ms)` before re-reading the tree to compute `changed` and
`scroll_after` (see `SdlAutomationSession.scroll()`). LVGL's kinetic
scroll/snap animation can still be in flight after 150ms for a large drag or
a snappy scroll-to-item transition. If a `scroll`/`scroll_to` result's
`scroll_after` looks like it undershot or the screen hash looks mid-transition,
issue an extra `edgetx_wait` (200-400ms) and re-read `edgetx_ui_tree` before
concluding the gesture didn't work -- don't assume `changed=false` means the
gesture failed.

### Fixtures vs. fresh storage

A session's `sdcard`/`settings` directories come from one of three places,
each with different mutation semantics:

- **Default (no `sdcard`/`settings` given):** the harness copies
  `tools/ui-harness/fixtures/sdcard-<target>` /
  `tools/ui-harness/fixtures/settings-<target>` (if present) into a fresh
  temp directory per session, or starts from an empty directory if no
  default fixture exists for that target (first-run/factory-default
  behavior). Tracked fixture files are never written to.
- **Named fixture** (a bare name, e.g. `settings="settings-tx16s"`,
  resolved against `tools/ui-harness/fixtures/`): same as above -- copied
  into a temp directory first. Safe to reuse across runs and safe for
  parallel sessions.
- **Explicit path** (an absolute path, or anything with more than one path
  component): used **directly, in place, with no copy**. The simulator
  reads *and writes* there -- this is intentional (e.g. to inspect the
  resulting radio.yml/model files after a flow), but it means the directory
  is genuinely mutable and, if the fixture fails to load (see below), can
  be destructively reformatted. Never point an explicit `--settings` path
  at something you care about keeping unless you've copied it first.

### Fixture contract: RADIO/radio.yml, MODELS/*.yml, labels

A settings-overlay fixture directory looks like:

```text
settings-<name>/
  RADIO/
    radio.yml       # optional: see auto-provisioning below
  MODELS/
    model1.yml       # any *.yml model files
    labels.yml        # optional: model labels/filtering metadata
```

Before the simulator is spawned, `SdlAutomationSession._spawn_process()`
always runs `_prepare_settings_directory()` on the resolved settings
directory (copied fixture, named fixture, *or* explicit path -- uniformly,
so this protection is never silently skipped):

1. **Line-ending normalization.** Every `*.yml` file is normalized to CRLF.
   Real-device radio.yml exports use CRLF and embed a CRC16 checksum
   computed over those CRLF bytes; a fixture saved with LF-only line
   endings (the default for most editors and for files committed to git)
   still declares the CRLF-computed checksum, so without this step the
   firmware's own checksum check fails at boot.
2. **Auto-provisioning.** If `MODELS/*.yml` is present but `RADIO/radio.yml`
   is missing, the harness copies a known-good default radio.yml from
   `tools/ui-harness/fixtures/settings-<target>/RADIO/radio.yml` into
   place. Without this, `resolveForRead()`
   (`radio/src/targets/simu/simufatfs.cpp`) silently falls through to
   whatever radio.yml the *SD-card* fixture happens to have (or none),
   which is unpredictable rather than a deliberate default. If no default
   fixture exists for the target, this is a no-op and the firmware's own
   first-run defaulting takes over (matching prior behavior).
3. **Checksum validation.** If `RADIO/radio.yml` exists, its embedded
   `checksum: N` line (if any) is verified in Python against a CRC16
   recomputed over the rest of the file, replicating
   `readYamlFile()`/`crc16()` (`radio/src/storage/sdcard_yaml.cpp`,
   `radio/src/crc.cpp`) exactly. A file with **no** checksum line is
   accepted unconditionally (the firmware treats that as a legacy file and
   skips the check); a file with `manuallyEdited: 1` is accepted despite a
   mismatch (the firmware forgives it the same way). Any other mismatch
   raises `HarnessError` **before the simulator is spawned**, naming the
   file and the declared vs. computed checksum.

   This validation exists because a checksum-rejected radio.yml is not just
   a failed boot: the firmware's recovery path
   (`storageReadAll()` -> `storageEraseAll()` -> `storageFormat()`)
   reformats the settings overlay *in place* and writes fresh
   factory-default radio + model data over whatever was there --
   including silently overwriting `MODELS/*.yml` with a synthesized
   default `MODEL01`. For an explicit (uncopied) `--settings` path, that
   is real, on-disk, irreversible loss of the fixture's actual model data,
   not merely a failed boot. See `proof/p6-fixture-overlay/` for a full
   before/after repro, including direct evidence of a fixture's
   `MODELS/model1.yml` being clobbered this way.

   **If you hand-craft a radio.yml fixture:** either omit the `checksum:`
   line entirely (simplest -- always accepted), or regenerate it correctly
   (boot the simulator once against an unmodified radio.yml, let it save,
   and diff the result). Do not hand-edit a radio.yml that already has a
   `checksum:` line without one of those two steps.

### Automation command catalog

Every `edgetx_*` MCP tool / `SdlAutomationSession` method sends one line of
the line-oriented `--automation-stdio` protocol implemented in
`radio/src/targets/simu/sdl_simu.cpp::automation_handle_command()`. Run-flow
step names match the Python method names, which are kept 1:1 with the raw
protocol command name unless noted.

General input/observation:
`press` (key), `long_press` (key), `rotate` (encoder), `touch`, `drag`,
`wait`, `status`, `ui_tree`, `ui_click`/`ui_long_click` (flow steps
`click`/`long_click`), `route_current`/`route_sitemap` (no flow step; used
by `edgetx_orient`/`edgetx_sitemap`), `route_goto` (flow step `goto`),
`text_input` (flow step `type_text`), `screenshot_ppm` (flow step
`screenshot`), `set_switch`, `switch_sequence`, `set_usb`, `audio_history`,
`stop`.

Test-precondition commands (added for P1 -- let a flow set up state directly
instead of dozens of UI taps):

| Flow step / method                  | Raw protocol command     | Purpose |
|--------------------------------------|---------------------------|---------|
| `battery_reset`                      | `battery_reset`           | Clear all battery packs/monitors to defaults |
| `battery_pack`                       | `battery_pack`             | Configure a global battery-pack library slot (1-16) |
| `battery_monitor`                    | `battery_monitor`          | Configure a model battery-monitor slot (0-3) + bind VFAS/Capa sensors |
| `battery_monitor_enable`             | `battery_monitor_enable`   | Enable/disable a battery-monitor slot |
| `battery_set_telemetry`              | `battery_set_telemetry`    | Inject a VFAS/Capa value and advance the battery session state machine |
| `battery_telemetry_lost`             | `battery_telemetry_lost`   | Simulate telemetry loss and advance the loss-swap window |
| `battery_tick`                       | `battery_tick`              | Advance the battery session state machine by N seconds |
| `battery_check_alerts`               | `battery_check_alerts`     | Run the flight-battery alert check N times |
| `battery_confirm`                    | `battery_confirm`           | Confirm a pack-selection prompt for a monitor |
| `battery_state`                      | `battery_state`             | Read-only: current flight-battery runtime state |
| `set_timer`                          | `set_timer`                 | Configure and set a model timer (0-2) directly |
| `add_telemetry_sensor`               | `add_telemetry_sensor`      | Register a telemetry sensor in a slot, independent of any battery-monitor binding |
| `set_telemetry`                      | `set_telemetry`             | Set a named sensor's raw value |
| `set_telemetry_streaming`            | `telemetry_streaming`       | Toggle continuous telemetry updates |
| `set_batt_voltage`                   | `set_batt_voltage`          | Set the simulated TX battery voltage (decivolts) |

All of the above are available as `edgetx_<name>` MCP tools and as
`run-flow` JSON steps (e.g. `{"battery_monitor": {"monitor": 0,
"battery_type": "lipo", "cells": 3, "capacity_mah": 2200}}`); both call the
same `HarnessService`/`SdlAutomationSession` methods in `core.py`. The
`edgetx-ui` CLI itself only exposes `build`/`start`/`smoke`/`run-flow` as
subcommands -- reach individual precondition commands through the MCP
server or a flow, not a dedicated CLI flag.

### Bounded retry on read-only queries (not mutations)

`SdlAutomationSession.command()` accepts a `retries` budget, applied **only**
on a bare timeout (no reply observed at all) and **never** on an actual error
reply. `ui_tree`, `audio_history`, and the internal route-read helpers pass
`retries=2` because the simulator can miss a reply window while mid a
storage-flush ("SD card write") burst even though it is otherwise healthy --
a resend of a *read* is always safe. Mutating commands (`press`, `touch`,
`set_switch`, the battery/timer precondition commands, etc.) default to
`retries=0` and must stay that way: resending a mutating command on a
timeout could double-apply it if the original request actually landed and
only the reply was delayed.

### Run-flow conventions

A flow JSON file is `{"target": "tx16s", "output": "<dir>", "sdcard": ...,
"settings": ..., "steps": [...]}`. `sdcard`/`settings` follow the same
fixture-name-or-explicit-path resolution as `edgetx_start_simulator` (see
"Fixtures vs. fresh storage" above). Each step is a single-key object, e.g.
`{"wait": {"ms": 500}}` or `{"click": "Model name"}`; see
`HarnessService._run_step()` for the exact shape each step key accepts.
Treat JSON flows as replay artifacts of an interaction you've already
verified interactively (via the MCP tools) -- authoring a flow blind and
debugging it step-by-step through JSON edits is much slower than driving the
same steps live first.

### Verify you're driving the tree you think you are

`edgetx_status` (and the result of `edgetx_start_simulator`) includes
`source_root` (the checked-out path the simulator was built from) and
`git_commit` (that tree's current short commit). The MCP server builds and
runs whichever source tree it was configured/started in, which is not
necessarily the worktree an agent is sitting in -- agents in other worktrees
have repeatedly ended up testing the wrong code this way. **Before trusting
a result, compare `status.git_commit` to `git rev-parse --short HEAD` in
your own worktree**; if they differ, you are observing a different tree
than the one you're editing.

### Persistent session mode: not implemented

There is no way to reattach a new `edgetx-mcp`/CLI process to an
already-running simulator: `--automation-stdio` is the simulator's only
transport, it is a stdin/stdout pipe pair owned exclusively by whichever
process spawned it, and the harness deliberately kills the simulator when
its owning process exits (`atexit`-registered orphan cleanup). Every new
MCP server/CLI invocation builds and starts its own simulator. This was
assessed and found to require either a firmware-level socket transport or
a daemon/thin-client redesign of the harness -- a real, multi-file
undertaking, not a small gap -- so it was scoped out rather than attempted
partially. See `proof/p5-persistent-session/assessment.txt` for the full
reasoning if picking this up later.

## Target

Supported targets:

- `tx16s`, configured as `PCB=X10` and `PCBREV=TX16S`
- `tx16smk3`, configured as `PCB=TX16SMK3`

Default fixtures are created on demand:

```text
tools/ui-harness/fixtures/sdcard-tx16s
tools/ui-harness/fixtures/settings-tx16s
```
