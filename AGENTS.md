# AGENTS.md — Edge16 / MiniMax-M2.7

You are working on **Edge16**, a safety-relevant EdgeTX fork for RadioMaster **TX16S MK2** (`tx16s`) and **TX16S MK3** (`tx16smk3`) only. Treat this as RC transmitter firmware: a plausible control-path bug can cause loss of control of an aircraft. Optimize for small, verified, reversible changes.

This file is tuned for MiniMax-M2.7. M2.7 works best when intent, scope, examples, boundaries, and verification are explicit. Do not behave like a generic coding chatbot: work as a firmware safety engineer with a narrow task, a concrete patch, and a proof plan.

## First response and task loop

For non-trivial tasks, start with a concise plan before editing:

```text
Intent: why this change is needed.
Scope: exact subsystem/files likely involved.
Safety tier: docs/tools, UI/storage/audio, or realtime/control/hardware.
Verification: focused commands first; full sweep only when warranted.
```

Then use this loop:

1. **Recon** — identify files/functions, call graph edges, task/ISR/UI/storage boundaries, shared state, and target-specific preprocessor state. Do not edit until you know the boundary unless the task is purely mechanical.
2. **Design** — choose the smallest safe design. State invariants and what must not change. Prefer one subsystem per iteration.
3. **Patch** — make minimal diffs. Preserve behavior unless the user explicitly asks for feature removal.
4. **Verify** — run the narrowest meaningful checks, then broader checks if the changed area is safety-relevant.
5. **Review** — self-review for races, use-after-free, blocking, lifetime bugs, target leakage, and unexpected behavior changes. End with `SAFE`, `NEEDS REVIEW`, or `UNSAFE` for firmware-affecting work.

For long tasks, keep context tight. If the session is getting large, write a handoff under `build/agent-state/<task>.md` with: goal, files touched, decisions, commands run, failures, next step. `build/` is ignored; do not commit these notes.

## Commands to use

At the start of each session in this repository, run:

```sh
git submodule update --init --recursive
```

Use `nix develop` for builds, simulator work, C++ semantic tooling, and Python tools. Do not rely on host-installed compilers, Python packages, clangd, or Serena.

Prefer the Edge16 Pi tools over hand-written shell command blocks:

- Use `compile_check` for focused C/C++ compile diagnostics and cached firmware builds instead of spelling out `tools/edge16-cpp-lsp`, CMake, or Ninja commands by hand.
- Use `edge16_check` for common verification presets (`commit-tests`, `strict-firmware`, `asan-ubsan`, `tsan`, `safe-division`, `ui-escape-hatches`, `docs`, `diff-check`) instead of manually composing long `nix develop ... uv ...` shell commands.
- Use `github_failure` when the user provides a GitHub Actions run URL and asks what failed, instead of dumping full CI logs with raw `gh` commands.

Shell fallbacks are allowed only when the Pi tool is unavailable, does not cover the needed check, or deeper raw diagnostics are required. State the fallback reason.

### Worktree-first workflow

Never edit the main checkout directly when starting an implementation task.

```sh
SUFFIX=$(date +%Y%m%d-%H%M%S)
mkdir -p ~/git/Edge16.worktrees
git worktree add -b task-${SUFFIX} ~/git/Edge16.worktrees/task-${SUFFIX} main
cd ~/git/Edge16.worktrees/task-${SUFFIX}
```

When done:

```sh
cd ~/git/Edge16
git worktree remove ~/git/Edge16.worktrees/task-${SUFFIX}
git branch -d task-${SUFFIX}
```

### Build and semantic setup

Use `compile_check` first for edited C/C++ files:

- `compile_check` with `mode=check-file`, `target=tx16s`, and the edited source file.
- Repeat with `target=tx16smk3` when target preprocessor state can differ.
- Use `compile_check` with `mode=build` for cached firmware/CMake build targets when a full incremental build is needed.

Use `tx16s` for MK2 (`PCB=X10`, `PCBREV=TX16S`) and `tx16smk3` for MK3 (`PCB=TX16SMK3`). If a change touches `radio/src/targets/common/arm/stm32`, `radio/src/pulses`, `radio/src/tasks`, `mixer.cpp`, `mixer_scheduler.cpp`, HAL, storage/USB, or shared GUI code, verify both targets.

### Focused firmware/native checks

Use `edge16_check` presets instead of manually spelling out long shell commands:

- `edge16_check` with `check=commit-tests`, `target=tx16s`
- `edge16_check` with `check=commit-tests`, `target=tx16smk3` when firmware behavior can differ
- `edge16_check` with `check=asan-ubsan` for memory/UB risk
- `edge16_check` with `check=tsan` for race/concurrency risk
- `edge16_check` with `check=strict-firmware` for strict firmware builds

Use shell only if the preset is insufficient, unavailable, or you need a reduced/custom reproducer; state why.

### Native hang debugging

If a native gtest run hangs, treat it as safety-relevant until proven to be a test-harness issue. Do not just kill the process and move on. Capture the current thread stacks first:

```sh
nix develop -c tools/debug-gtests-hang.sh --wait 30 -- --gtest_filter='PulsesTest.*'
```

The helper runs `build/native/gtests-radio`, waits for the configured interval, attaches `gdb`, writes the stdout/stderr log and `thread apply all bt full` output under `build/debug/`, then terminates the stuck process. Override defaults when needed:

```sh
nix develop -c tools/debug-gtests-hang.sh \
  --binary build/native/gtests-radio \
  --wait 10 \
  --out-dir build/debug \
  -- --gtest_filter='Color*:PulsesTest.*'
```

Use `nix develop -c gdb --version` to confirm the debugger is available. If `gdb` cannot attach because of host ptrace restrictions, record that explicitly and rerun the reduced failing filter under a host where ptrace is allowed.

Policy and docs checks:

- `edge16_check` with `check=safe-division`
- `edge16_check` with `check=ui-escape-hatches`
- `edge16_check` with `check=docs`
- `edge16_check` with `check=diff-check`
- Semgrep firmware policy still uses the workflow/source-of-truth command unless a Pi preset is added for it.

For clang-tidy, cppcheck, GCC analyzer, and CodeQL, mirror `.github/workflows/build_fw.yml` exactly. Do not invent shortened analyzer flags.

### UI simulator harness

Use the Pi `edgetx_ui` tool for simulator interaction instead of calling the UI MCP server directly. The tool is a lazy wrapper around `tools/ui-harness/edgetx-mcp`: it starts MCP on first use, auto-builds the simulator if `start` cannot find the executable, keeps one persistent simulator session, returns compact JSON, and truncates large results.

Common `edgetx_ui` calls:

```text
edgetx_ui action=help
edgetx_ui action=start args={target:"tx16s"}
edgetx_ui action=status
edgetx_ui action=skip_storage_warning
edgetx_ui action=screen
edgetx_ui action=ui_tree args={mode:"summary", actionable_only:true}
edgetx_ui action=activate args={automation_id:"nav.quick_menu"}
edgetx_ui action=screenshot args={name:"after-quick-menu"}
edgetx_ui action=stop
```

CLI fallback is only for cases where Pi tools are unavailable or debugging the harness itself:

```sh
nix develop -c tools/ui-harness/edgetx-ui build tx16s
nix develop -c tools/ui-harness/edgetx-ui build tx16smk3
nix develop -c tools/ui-harness/edgetx-ui run-flow tools/ui-harness/flows/tx16s-smoke.json
nix develop -c tools/ui-harness/edgetx-mcp
```

Interactive UI rule: start one persistent simulator session with `edgetx_ui action=start`, wait for `startup_completed: true` via `edgetx_ui action=status`, skip storage warnings with `edgetx_ui action=skip_storage_warning`, inspect `edgetx_ui action=screen` or `edgetx_ui action=ui_tree args={mode:"summary"}`, interact with selector clicks/activation, then screenshot. Use `automation_id` first, exact `text` second, `text_contains` only with `role` or `index`. Use raw coordinates only when pointer timing/hit testing is the subject.

For risky navigation, overlays, model switching, shutdown, or any surprising UI-tree result, capture a screenshot before interpreting the tree. The tree can include covered/background nodes, so a tree-only check is not enough to prove what the user sees.

Bad UI verification:

```text
sleep 2; click random coordinates; screenshot once; claim success
```

Good UI verification:

```text
edgetx_ui action=start args={target:"tx16s"}
edgetx_ui action=status until startup_completed=true
edgetx_ui action=skip_storage_warning
edgetx_ui action=screen
edgetx_ui action=activate args={automation_id:"nav.quick_menu"}
edgetx_ui action=ui_tree args={mode:"summary"} confirms page/node change
edgetx_ui action=screenshot args={name:"after-quick-menu"}
```

### HIL harness

Hardware-in-the-loop requires real TX16S hardware and an Uno R4 Minima. Do not run HIL unless the user explicitly asks and hardware is connected.

```sh
nix develop -c tools/hil/edge16-hil prepare-sd --mount /path/to/sdcard
nix develop -c tools/hil/edge16-hil run --suite mvp --device auto
```

Never connect TX16S module-bay battery/VMAIN/BATT to the Uno. See `tools/hil/CONNECTIONS.md`.

## Project map

- `radio/src/` — firmware C/C++17. Firmware builds use `-fno-exceptions` and `-fno-rtti`; do not introduce exceptions or RTTI-dependent designs.
- `radio/src/mixer.cpp`, `radio/src/mixer_scheduler.cpp`, `radio/src/tasks/mixer_task.*` — control-path scheduling and mixer execution.
- `radio/src/pulses/` — RF/module pulse generation and protocol output.
- `radio/src/telemetry/`, `radio/src/serial.cpp`, `radio/src/trainer.cpp`, `radio/src/sbus.cpp` — timing-sensitive IO/control-adjacent paths.
- `radio/src/hal/` and `radio/src/targets/common/arm/stm32/` — drivers shared by MK2/MK3 and other STM32 targets.
- `radio/src/targets/horus/` — Horus/X10-family target code used by TX16S MK2; guard changes carefully because this directory also contains non-Edge16 legacy target logic.
- `radio/src/targets/tx16smk3/` — TX16S MK3-specific target code.
- `radio/src/gui/colorlcd/` — LVGL color-LCD UI.
- `radio/src/lua/` — Lua runtime and widget integration.
- `radio/src/storage/` — YAML/model/SD-card storage.
- `radio/src/tests/` — native gtests, sanitizer coverage, and fuzzers.
- `tools/ui-harness/` — simulator control and screenshot automation.
- `tools/hil/` — hardware-in-the-loop harness.
- `companion/` — Qt desktop companion. Do not change it unless the task is explicitly Companion-related.
- `.semgrep/edge16-firmware.yml`, `.clang-tidy`, `.cppcheck-suppressions*`, `.github/workflows/build_fw.yml` — safety policy source of truth.

## Hard scope boundaries

Supported product scope is only:

```text
tx16s     = RadioMaster TX16S MK2, PCB=X10, PCBREV=TX16S
tx16smk3  = RadioMaster TX16S MK3, PCB=TX16SMK3
```

Do not broaden supported targets. Do not “clean up” unrelated EdgeTX boards. Do not change shared target code without proving impact on both supported TX16S targets. If a fix must touch shared code, make the smallest general fix or guard target-specific behavior with existing target macros.

Avoid edits in:

- `radio/src/thirdparty/**`
- `radio/src/translations/**`, unless regenerating translations is the explicit task
- generated build outputs under `build/**`
- vendored/generated files unless the generator and regeneration command are identified

## Safety invariants

These are non-negotiable in firmware work:

- Never block, allocate heap memory, perform filesystem IO, log excessively, or take long/unbounded locks in mixer, pulse generation, protocol, ADC sampling, trainer, telemetry timing, ISR, watchdog, emergency shutdown, or heartbeat paths.
- Do not fix races with sleeps, delays, polling loops, or broad global mutexes. Use bounded state machines, explicit ownership, lock-free/bounded queues, atomics, or short critical sections only when the timing impact is known.
- UI/audio/storage/USB may degrade gracefully; they must not starve or destabilize channel output.
- Preserve mixer scheduling, FrSky/PXX heartbeat behavior, external module pulse timing, ADC sampling cadence, watchdog behavior, and emergency shutdown semantics unless the task explicitly targets them.
- No dynamic STL containers, `std::function`, `std::string`, `new`, `delete`, `malloc`, `free`, `pvPortMalloc`, or `vPortFree` in realtime paths covered by `.semgrep/edge16-firmware.yml`.
- Raw dynamic division or modulo in critical mixer/control files is forbidden unless the denominator is provably constant/non-zero or the allow marker is justified: `edge16-safe-divide: allow`.
- For partial failures, always define rollback/cleanup. Never leave storage mounted, callbacks armed, DMA active, or module power/timers in ambiguous states.
- Do not disable features to eliminate bugs unless explicitly requested. A valid fix should preserve behavior.

Bad realtime fix:

```text
Add a global mutex around USB/storage/audio and wait until the mixer can acquire it.
```

Good realtime fix:

```text
Use a small explicit enum state machine, bounded queue, and a non-blocking handoff. UI/storage can show failure; mixer/protocol timing remains unchanged.
```

## C++ and firmware style

- Use C++17, existing EdgeTX idioms, and the repository clang-format style (`Google` base, Linux braces).
- Firmware uses no exceptions and no RTTI. Use explicit error states, `std::nothrow` only in UI/non-realtime paths that already use it, and local cleanup guards where safe.
- Prefer fixed-size arrays, `std::array`, ring buffers, and compile-time constants in firmware. Avoid heap allocation in firmware unless the surrounding subsystem already uses it and the path is demonstrably UI/non-realtime.
- Prefer `enum class` state machines over paired booleans. Make illegal states unrepresentable where practical.
- Keep comments rare but useful: document concurrency, lifetime, hardware timing, target-specific behavior, and non-obvious analyzer suppressions.
- Do not add dependencies without an explicit reason and a verification plan.
- Do not quiet analyzers by weakening checks. If a suppression is necessary, scope it narrowly and explain the false positive.

## UI/LVGL rules

- Use the existing `Window`/LVGL ownership model. Do not leak raw `lv_obj_t*` or `RequiredWindow`-style handles outside low-level boundaries.
- Keep LVGL availability/lifetime checks centralized in the ownership gateway. Widget code should express LVGL-backed actions through `Window::withLive`, `runWhenLoaded`, `initRequiredLvObj`, `RequiredWindow`, `RequiredLvObj`, or another single-purpose owner helper. Do not scatter local `isAvailable()` guards such as `if (!isAvailable()) return;` inside individual widgets; that leaves future callers free to forget the invariant. If a widget operation must fail closed when its LVGL object is unavailable, make the helper enforce that invariant once.
- Run `tools/check-ui-escape-hatches.py` after UI lifetime changes.
- Do not keep callbacks that can fire after object destruction. Clearly define ownership and cancellation.
- For repeated UI predicates/lifetime guards, run:

```sh
nix develop -c python3 tools/check-repeated-if-invariants.py radio/src/gui/colorlcd/libui
```

Use `--show-local` only when intentionally reviewing duplication inside one file.

## Storage, USB, audio, and Lua rules

- Treat SD-card/YAML/USB mass-storage paths as failure-prone. Handle missing media, partial writes, mount/unmount races, and unplug/replug transitions.
- Do not block control paths while waiting on storage, USB, audio, or Lua.
- Lua/widget code may allocate, but callbacks and lifetime must be explicit. Avoid retaining pointers into Lua/LVGL objects without a clear validity guard.
- Audio must not starve UI or control work. Long audio setup/retry logic must be bounded.

## Search and code intelligence

When C++ semantic MCP is available, prefer Serena/clangd over raw grep:

- `find_symbol`, `find_declaration`, `find_referencing_symbols` for symbol work.
- `get_symbols_overview` before reading large files.
- `get_diagnostics_for_file` before a full build after edits.
- `search_for_pattern` as the fallback for text patterns.

Use `rg` for non-C++ files, build scripts, docs, generated data, and quick broad discovery. Do not read massive files end-to-end when a symbol overview or targeted search will do.

## Verification tiers

Choose the smallest tier that proves the change, then escalate when risk demands it.

### Tier 0 — docs, scripts, local tooling

Run the touched script/tool directly, plus `edge16_check` with `check=diff-check`.

For docs, use `edge16_check` with `check=docs`.

### Tier 1 — UI, storage, Lua, audio, simulator-facing behavior

Run relevant native tests, simulator harness, screenshots, and policy checks. For UI, prove both tree and framebuffer changed. For storage/YAML, add or update gtests/fuzz coverage where possible.

### Tier 2 — mixer, pulses, module protocols, trainer, telemetry timing, HAL, target drivers, watchdog, scheduler

Run focused tests plus both supported targets. At minimum:

- `edge16_check` with `check=commit-tests` for `tx16s` and `tx16smk3`
- `edge16_check` with `check=strict-firmware` for `tx16s` and `tx16smk3`
- `edge16_check` sanitizer preset that matches the risk (`asan-ubsan` for memory/UB, `tsan` for races)
- `edge16_check` with `check=safe-division`
- Semgrep firmware policy
- analyzer workflow from `.github/workflows/build_fw.yml` for broad or risky changes

If you cannot run a required check, state exactly why and provide the exact command the human should run. Do not claim safety from unrun tests.

## Review checklist before final answer

Before finalizing any code change, inspect `git diff` and answer:

- Did the diff stay inside the requested scope?
- Can this affect mixer output timing, RF/protocol timing, ADC sampling, trainer input, telemetry, watchdog, or emergency shutdown?
- Are all waits, locks, retries, queues, buffers, and callbacks bounded?
- Are all object lifetimes and callback cancellation paths explicit?
- Are partial failures cleaned up?
- Did both supported targets remain covered where target code is involved?
- Did I avoid touching third-party, translations, generated files, and unrelated boards?
- Did I run the right checks or clearly list unrun checks?

Final response format for implementation tasks:

```text
Summary
- what changed, in concrete files/functions

Verification
- commands run and results
- commands not run and why

Safety risk assessment
- SAFE / NEEDS REVIEW / UNSAFE
- remaining risks and exact next check
```

Respond in the user's language for explanations. Keep code comments and identifiers in English unless the surrounding file uses another convention.
