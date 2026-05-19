# EdgeTX UI GDB Trace pi extension

Project-local pi extension exposing `edgetx_gdb_trace`.

## Flow

```text
edgetx_ui action=start args={target:"tx16s"}
edgetx_gdb_trace action=start target=tx16s mode=sample interval_ms=500 label=outputs-scroll
# drive simulator with edgetx_ui actions or flow
edgetx_gdb_trace action=stop
edgetx_gdb_trace action=analyze trace_dir="build/gdb-ui-traces/<run>"
```

## Modes

- `sample`: repeated `gdb -batch -p <pid>` snapshots with `thread apply all bt`. Best first pass. Briefly pauses simulator per sample.
- `breakpoints`: persistent gdb attach with Python breakpoints for selected functions. High overhead.
- `hybrid`: both. Highest overhead.

`timed:true` adds sampled gdb `FinishBreakpoint` probes for selected functions. Use `timing_sample_rate` (default 25) and `max_timing_events` to cap overhead. Timing is inclusive and perturbed by gdb, so compare idle/control/scroll scenarios instead of trusting one run.

`allocs:true` adds malloc/calloc/realloc/free/new/delete breakpoints. Treat allocation numbers as best-effort; optimized builds, allocator wrappers, and symbol visibility can hide calls.

## Output

Default root: `build/gdb-ui-traces/`.

Each run writes:

- `manifest.json` — target, pid, mode, parameters, counts.
- `events.jsonl` — sample markers and breakpoint/allocation events.
- `samples/*.bt` — raw gdb backtraces.
- `summary.json` — approximate hot-frame counts, breakpoint rates, and optional duration stats.
- `gdb*.log` — raw gdb stdout/stderr/logging.

## Notes

Run through `nix develop` by default (`use_nix:true`) so project gdb is used. Linux ptrace policy can block attach; if so set `kernel.yama.ptrace_scope=0` or run in allowed debug environment.
