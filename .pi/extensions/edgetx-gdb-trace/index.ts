import { spawn, spawnSync, type ChildProcessWithoutNullStreams } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";

type TraceMode = "sample" | "breakpoints" | "hybrid";

type StartParams = {
	action?: string;
	target?: string;
	pid?: number;
	mode?: TraceMode;
	interval_ms?: number;
	duration_ms?: number;
	output_dir?: string;
	functions?: string[];
	allocs?: boolean;
	backtrace_depth?: number;
	use_nix?: boolean;
	label?: string;
	timed?: boolean;
	timing_sample_rate?: number;
	max_timing_events?: number;
};

type TraceRun = {
	id: string;
	pid: number;
	target: string;
	mode: TraceMode;
	startedAt: string;
	traceDir: string;
	intervalMs: number;
	backtraceDepth: number;
	functions: string[];
	allocs: boolean;
	label?: string;
	timed: boolean;
	timingSampleRate: number;
	maxTimingEvents: number;
	samples: number;
	events: number;
	nextSample: number;
	sampleInFlight: boolean;
	lastSample?: Promise<void>;
	stopping: boolean;
	sampler?: ReturnType<typeof setInterval>;
	deadline?: ReturnType<typeof setTimeout>;
	gdb?: ChildProcessWithoutNullStreams;
	lastError?: string;
};

type LockCandidate = { target: string; pid: number; path: string; mtimeMs: number; alive: boolean };

const DEFAULT_FUNCTIONS = [
	"lv_timer_handler",
	"lv_refr_now",
	"lv_obj_update_layout",
	"drawMainPots",
	"drawStatusLine",
	"refreshDisplay",
];
const ALLOC_FUNCTIONS = ["malloc", "calloc", "realloc", "free", "operator new", "operator delete"];
const MAX_FUNCTIONS = 64;
const MAX_DEPTH = 64;
let activeRun: TraceRun | undefined;

function record(value: unknown): Record<string, unknown> {
	return value && typeof value === "object" && !Array.isArray(value) ? (value as Record<string, unknown>) : {};
}

function clampInt(value: unknown, fallback: number, min: number, max: number): number {
	const n = typeof value === "number" ? value : Number(value ?? fallback);
	return Number.isFinite(n) ? Math.min(Math.max(Math.trunc(n), min), max) : fallback;
}

function projectRoot(): string {
	return process.cwd();
}

function isoStamp(date = new Date()): string {
	return date.toISOString().replace(/[:.]/g, "-");
}

function slug(value: unknown): string {
	return String(value ?? "")
		.trim()
		.toLowerCase()
		.replace(/[^a-z0-9_.-]+/g, "-")
		.replace(/^-+|-+$/g, "")
		.slice(0, 80);
}

function isAlive(pid: number): boolean {
	try {
		process.kill(pid, 0);
		return true;
	} catch {
		return false;
	}
}

function discoverLockfiles(target?: string): LockCandidate[] {
	const prefix = "edgetx-simulator-";
	const candidates: LockCandidate[] = [];
	for (const name of readdirSync(tmpdir()).filter((entry) => entry.startsWith(prefix) && entry.endsWith(".lock"))) {
		try {
			const path = join(tmpdir(), name);
			const pid = Number(readFileSync(path, "utf8").trim());
			const targetPart = name.slice(prefix.length).replace(/-\d+\.lock$/, "");
			const alive = Number.isInteger(pid) && isAlive(pid);
			if (alive && (!target || targetPart === target)) candidates.push({ target: targetPart, pid, path, mtimeMs: statSync(path).mtimeMs, alive });
		} catch {
			// Lockfiles are best-effort breadcrumbs; ignore stale/racy files.
		}
	}
	return candidates.sort((a, b) => b.mtimeMs - a.mtimeMs);
}

function resolvePid(params: StartParams): { pid: number; target: string; source: string } {
	if (params.pid) {
		if (!isAlive(params.pid)) throw new Error(`pid ${params.pid} is not alive`);
		return { pid: params.pid, target: params.target ?? "unknown", source: "explicit" };
	}
	const candidates = discoverLockfiles(params.target);
	if (!candidates.length) {
		throw new Error("No live EdgeTX simulator lockfile found. Start UI harness first with edgetx_ui action=start, or pass pid.");
	}
	return { pid: candidates[0].pid, target: candidates[0].target, source: candidates[0].path };
}

function gdbCommand(args: string[], useNix: boolean): { command: string; args: string[] } {
	if (!useNix) return { command: "gdb", args };
	return { command: "nix", args: ["develop", ".", "-c", "gdb", ...args] };
}

function writeManifest(run: TraceRun, extra: Record<string, unknown> = {}): void {
	writeFileSync(
		join(run.traceDir, "manifest.json"),
		JSON.stringify(
			{
				id: run.id,
				pid: run.pid,
				target: run.target,
				mode: run.mode,
				started_at: run.startedAt,
				updated_at: new Date().toISOString(),
				interval_ms: run.intervalMs,
				backtrace_depth: run.backtraceDepth,
				functions: run.functions,
				allocs: run.allocs,
				label: run.label,
				timed: run.timed,
				timing_sample_rate: run.timingSampleRate,
				max_timing_events: run.maxTimingEvents,
				samples: run.samples,
				events: run.events,
				last_error: run.lastError,
				...extra,
			},
			null,
			2,
		),
		"utf8",
	);
}

function appendJsonl(path: string, entry: Record<string, unknown>): void {
	writeFileSync(path, `${JSON.stringify(entry)}\n`, { flag: "a" });
}

function spawnGdbBatch(run: TraceRun, useNix: boolean): Promise<void> {
	if (run.sampleInFlight) return run.lastSample ?? Promise.resolve();
	run.sampleInFlight = true;
	const sampleIndex = run.nextSample++;
	const samplePath = join(run.traceDir, "samples", `${String(sampleIndex).padStart(6, "0")}.bt`);
	const eventsPath = join(run.traceDir, "events.jsonl");
	const depthArg = run.backtraceDepth > 0 ? String(run.backtraceDepth) : "";
	const args = [
		"-q",
		"-nx",
		"-batch",
		"-p",
		String(run.pid),
		"-ex",
		"set pagination off",
		"-ex",
		"set confirm off",
		"-ex",
		`thread apply all bt ${depthArg}`.trim(),
		"-ex",
		"detach",
		"-ex",
		"quit",
	];
	const spec = gdbCommand(args, useNix);
	const started = Date.now();
	const child = spawn(spec.command, spec.args, { cwd: projectRoot(), stdio: ["ignore", "pipe", "pipe"] });
	let stdout = "";
	let stderr = "";
	child.stdout.on("data", (chunk: Buffer) => (stdout += chunk.toString("utf8")));
	child.stderr.on("data", (chunk: Buffer) => (stderr += chunk.toString("utf8")));
	run.lastSample = new Promise((resolveDone) => {
		child.on("exit", (code, signal) => {
			const ended = Date.now();
			writeFileSync(samplePath, stdout + (stderr ? `\n--- stderr ---\n${stderr}` : ""), "utf8");
			run.samples = Math.max(run.samples, sampleIndex);
			run.sampleInFlight = false;
			if (code !== 0) run.lastError = `sample ${sampleIndex} gdb exit code ${code ?? "?"}${signal ? ` signal ${signal}` : ""}: ${stderr.trim()}`;
			appendJsonl(eventsPath, { type: "sample", sample: sampleIndex, ts: new Date().toISOString(), duration_ms: ended - started, path: samplePath, code, signal });
			writeManifest(run);
			resolveDone();
		});
	});
	return run.lastSample;
}

function gdbPythonList(items: string[]): string {
	return JSON.stringify(items).replace(/\\/g, "\\\\").replace(/'/g, "\\'");
}

function writeBreakpointScript(run: TraceRun): string {
	const path = join(run.traceDir, "trace-breakpoints.gdb");
	const functions = [...run.functions, ...(run.allocs ? ALLOC_FUNCTIONS : [])].slice(0, MAX_FUNCTIONS);
	const py = `
import gdb, json, time, traceback
out = open(${JSON.stringify(join(run.traceDir, "events.jsonl"))}, "a", buffering=1)
functions = ${gdbPythonList(functions)}
timed = ${run.timed ? "True" : "False"}
timing_sample_rate = ${run.timingSampleRate}
max_timing_events = ${run.maxTimingEvents}
backtrace_depth = ${run.backtraceDepth}
hit_counts = {}
timing_events = 0

class TimingFinishBreakpoint(gdb.FinishBreakpoint):
    def __init__(self, spec, started, frames):
        super().__init__(internal=True)
        self.spec = spec
        self.started = started
        self.frames = frames
        self.silent = True
    def stop(self):
        global timing_events
        try:
            duration_us = int((time.perf_counter() - self.started) * 1000000)
            timing_events += 1
            out.write(json.dumps({"type":"duration","ts":time.time(),"spec":self.spec,"duration_us":duration_us,"frames":self.frames}) + "\\n")
        except Exception as exc:
            out.write(json.dumps({"type":"duration_error","ts":time.time(),"spec":self.spec,"error":str(exc)}) + "\\n")
        return False

def frame_name(frame):
    try:
        return frame.name() or "?"
    except Exception:
        return "?"

def collect_frames():
    frames = []
    frame = gdb.newest_frame()
    depth = 0
    while frame is not None and depth < backtrace_depth:
        frames.append(frame_name(frame))
        frame = frame.older()
        depth += 1
    return frames

class TraceBreakpoint(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)
        self.spec = spec
        self.silent = True
    def stop(self):
        global timing_events
        try:
            frames = collect_frames()
            hit_counts[self.spec] = hit_counts.get(self.spec, 0) + 1
            args = {}
            if self.spec in ("malloc", "operator new"):
                try: args["size"] = int(gdb.parse_and_eval("$rdi"))
                except Exception: pass
            out.write(json.dumps({"type":"breakpoint","ts":time.time(),"spec":self.spec,"hit":hit_counts[self.spec],"frames":frames,"args":args}) + "\\n")
            if timed and timing_events < max_timing_events and hit_counts[self.spec] % timing_sample_rate == 0:
                try:
                    TimingFinishBreakpoint(self.spec, time.perf_counter(), frames)
                except Exception as exc:
                    out.write(json.dumps({"type":"finish_setup_error","ts":time.time(),"spec":self.spec,"error":str(exc)}) + "\\n")
        except Exception as exc:
            out.write(json.dumps({"type":"error","ts":time.time(),"spec":self.spec,"error":str(exc),"traceback":traceback.format_exc()}) + "\\n")
        return False

for spec in functions:
    try:
        TraceBreakpoint(spec)
    except Exception as exc:
        out.write(json.dumps({"type":"breakpoint_setup_error","ts":time.time(),"spec":spec,"error":str(exc)}) + "\\n")
out.write(json.dumps({"type":"breakpoint_tracer_ready","ts":time.time(),"functions":functions,"timed":timed,"timing_sample_rate":timing_sample_rate,"max_timing_events":max_timing_events}) + "\\n")
`;
	writeFileSync(
		path,
		[
			"set pagination off",
			"set confirm off",
			"set breakpoint pending on",
			"set print thread-events off",
			`set logging file ${join(run.traceDir, "gdb-breakpoints.log")}`,
			"set logging overwrite on",
			"set logging enabled on",
			"python",
			py.trimEnd(),
			"end",
			"continue",
		].join("\n") + "\n",
		"utf8",
	);
	return path;
}

function startBreakpointGdb(run: TraceRun, useNix: boolean): void {
	const scriptPath = writeBreakpointScript(run);
	const spec = gdbCommand(["-q", "-nx", "-p", String(run.pid), "-x", scriptPath], useNix);
	const child = spawn(spec.command, spec.args, { cwd: projectRoot(), stdio: ["pipe", "pipe", "pipe"] });
	run.gdb = child;
	child.stdout.on("data", (chunk: Buffer) => writeFileSync(join(run.traceDir, "gdb.stdout.log"), chunk, { flag: "a" }));
	child.stderr.on("data", (chunk: Buffer) => writeFileSync(join(run.traceDir, "gdb.stderr.log"), chunk, { flag: "a" }));
	child.on("exit", (code, signal) => {
		appendJsonl(join(run.traceDir, "events.jsonl"), { type: "gdb_exit", ts: new Date().toISOString(), code, signal });
		if (activeRun?.id === run.id && !run.stopping) run.lastError = `breakpoint gdb exited with code ${code ?? "?"}${signal ? ` signal ${signal}` : ""}`;
		writeManifest(run);
	});
}

function startTrace(params: StartParams): Record<string, unknown> {
	if (activeRun && !activeRun.stopping) throw new Error(`Trace already active: ${activeRun.id}. Stop it first.`);
	const mode = (params.mode ?? "sample") as TraceMode;
	if (!["sample", "breakpoints", "hybrid"].includes(mode)) throw new Error("mode must be sample, breakpoints, or hybrid");
	const resolved = resolvePid(params);
	const id = isoStamp();
	const root = resolve(projectRoot(), params.output_dir ?? join("build", "gdb-ui-traces"));
	const label = slug(params.label);
	const traceDir = join(root, `${id}${label ? `-${label}` : ""}-${resolved.target}-${resolved.pid}`);
	mkdirSync(join(traceDir, "samples"), { recursive: true });
	const functions = (params.functions?.length ? params.functions : DEFAULT_FUNCTIONS).slice(0, MAX_FUNCTIONS);
	const run: TraceRun = {
		id,
		pid: resolved.pid,
		target: resolved.target,
		mode,
		startedAt: new Date().toISOString(),
		traceDir,
		intervalMs: clampInt(params.interval_ms, 1000, 100, 60_000),
		backtraceDepth: clampInt(params.backtrace_depth, 24, 1, MAX_DEPTH),
		functions,
		allocs: params.allocs === true,
		label: label || undefined,
		timed: params.timed === true,
		timingSampleRate: clampInt(params.timing_sample_rate, 25, 1, 10000),
		maxTimingEvents: clampInt(params.max_timing_events, 5000, 1, 200000),
		samples: 0,
		events: 0,
		nextSample: 1,
		sampleInFlight: false,
		stopping: false,
	};
	activeRun = run;
	writeManifest(run, { pid_source: resolved.source, project_root: projectRoot(), git: gitRevision() });
	appendJsonl(join(traceDir, "events.jsonl"), { type: "trace_start", ts: new Date().toISOString(), pid: run.pid, target: run.target, mode });
	const useNix = params.use_nix !== false;
	if (mode === "sample" || mode === "hybrid") {
		void spawnGdbBatch(run, useNix);
		run.sampler = setInterval(() => {
			if (!run.stopping && isAlive(run.pid) && !run.sampleInFlight) void spawnGdbBatch(run, useNix);
		}, run.intervalMs);
	}
	if (mode === "breakpoints" || mode === "hybrid") startBreakpointGdb(run, useNix);
	const durationMs = clampInt(params.duration_ms, 0, 0, 24 * 60 * 60_000);
	if (durationMs > 0) run.deadline = setTimeout(() => void stopTrace("duration elapsed"), durationMs);
	return { ok: true, id: run.id, pid: run.pid, target: run.target, mode: run.mode, trace_dir: run.traceDir, pid_source: resolved.source };
}

async function stopTrace(reason = "requested"): Promise<Record<string, unknown>> {
	const run = activeRun;
	if (!run) return { ok: true, active: false };
	run.stopping = true;
	if (run.sampler) clearInterval(run.sampler);
	if (run.deadline) clearTimeout(run.deadline);
	if (run.lastSample) await run.lastSample.catch(() => undefined);
	if (run.gdb && run.gdb.exitCode === null) {
		run.gdb.kill("SIGINT");
		await new Promise((resolve) => setTimeout(resolve, 250));
		try {
			run.gdb.stdin.write("detach\nquit\n");
		} catch {}
		await new Promise<void>((resolveDone) => {
			const timer = setTimeout(() => {
				run.gdb?.kill("SIGKILL");
				resolveDone();
			}, 3000);
			run.gdb?.once("exit", () => {
				clearTimeout(timer);
				resolveDone();
			});
		});
	}
	const summary = analyzeTraceDir(run.traceDir, run.intervalMs);
	run.events = typeof summary.events === "number" ? summary.events : run.events;
	writeManifest(run, { stopped_at: new Date().toISOString(), stop_reason: reason, summary });
	activeRun = undefined;
	return { ok: true, stopped: true, reason, trace_dir: run.traceDir, summary };
}

function gitRevision(): string {
	const result = spawnSync("git", ["rev-parse", "--short", "HEAD"], { cwd: projectRoot(), encoding: "utf8" });
	return result.status === 0 ? result.stdout.trim() : "unknown";
}

function extractFrames(text: string): string[] {
	const frames: string[] = [];
	for (const line of text.split(/\r?\n/)) {
		const match = line.match(/^#\d+\s+(?:0x[0-9a-fA-F]+\s+in\s+)?([^\s(]+).*$/);
		if (match) frames.push(match[1]);
	}
	return frames;
}

function bump(map: Map<string, number>, key: string, by = 1): void {
	map.set(key, (map.get(key) ?? 0) + by);
}

function isIdleFrame(name: string): boolean {
	return /^(?:__syscall|__internal_syscall|__futex|pthread_cond|start_thread|__clone|poll$|ppoll$|select$|nanosleep$)/.test(name) ||
		name.includes("wait_until") || name.includes("condition_variable") || name.includes("pa_threaded_mainloop_wait");
}

function isProjectFrame(name: string): boolean {
	return /(?:ListLineButton|OutputLineButton|OutputChannelBar|ModelOutputsPage|Window::|MainWindow::|lv_|redraw|refreshDisplay|ChannelBar|Form|Page)/.test(name);
}

function percentile(values: number[], pct: number): number {
	if (!values.length) return 0;
	const sorted = [...values].sort((a, b) => a - b);
	const idx = Math.min(sorted.length - 1, Math.max(0, Math.ceil((pct / 100) * sorted.length) - 1));
	return sorted[idx];
}

function top(map: Map<string, number>, limit = 25, samples = 0, intervalMs = 0): Array<{ name: string; count: number; sample_pct?: number; estimated_ms?: number }> {
	return [...map.entries()]
		.sort((a, b) => b[1] - a[1])
		.slice(0, limit)
		.map(([name, count]) => ({
			name,
			count,
			...(samples > 0 ? { sample_pct: Number(((count / samples) * 100).toFixed(1)), estimated_ms: count * intervalMs } : {}),
		}));
}

function durationStats(values: number[], breakpointCount: number): Record<string, number> {
	const total = values.reduce((sum, value) => sum + value, 0);
	const avg = values.length ? total / values.length : 0;
	return {
		timed_count: values.length,
		total_us: Math.round(total),
		avg_us: Math.round(avg),
		p50_us: Math.round(percentile(values, 50)),
		p95_us: Math.round(percentile(values, 95)),
		max_us: Math.round(values.length ? Math.max(...values) : 0),
		estimated_total_us: Math.round(avg * breakpointCount),
	};
}

function analyzeTraceDir(dir: string, intervalMs = 0): Record<string, unknown> {
	const frameCounts = new Map<string, number>();
	const topFrameCounts = new Map<string, number>();
	const projectFrameCounts = new Map<string, number>();
	const projectSampleCounts = new Map<string, number>();
	const breakpointCounts = new Map<string, number>();
	const durations = new Map<string, number[]>();
	const eventTimes: number[] = [];
	let samples = 0;
	const samplesDir = join(dir, "samples");
	if (existsSync(samplesDir)) {
		for (const file of readdirSync(samplesDir).filter((name) => name.endsWith(".bt")).sort()) {
			samples += 1;
			const frames = extractFrames(readFileSync(join(samplesDir, file), "utf8"));
			const topNonIdle = frames.find((frame) => !isIdleFrame(frame));
			if (topNonIdle) bump(topFrameCounts, topNonIdle);
			const seenProject = new Set<string>();
			for (const frame of frames) {
				if (!isIdleFrame(frame)) bump(frameCounts, frame);
				if (isProjectFrame(frame)) {
					bump(projectFrameCounts, frame);
					seenProject.add(frame);
				}
			}
			for (const frame of seenProject) bump(projectSampleCounts, frame);
		}
	}
	const eventsPath = join(dir, "events.jsonl");
	let events = 0;
	let allocBytes = 0;
	if (existsSync(eventsPath)) {
		for (const line of readFileSync(eventsPath, "utf8").split(/\r?\n/)) {
			if (!line.trim()) continue;
			events += 1;
			try {
				const event = JSON.parse(line) as { type?: string; spec?: string; ts?: number | string; duration_us?: number; args?: { size?: number } };
				if (typeof event.ts === "number") eventTimes.push(event.ts);
				if (event.type === "breakpoint" && event.spec) bump(breakpointCounts, event.spec);
				if (event.type === "duration" && event.spec && typeof event.duration_us === "number") {
					const list = durations.get(event.spec) ?? [];
					list.push(event.duration_us);
					durations.set(event.spec, list);
				}
				if (event.args?.size) allocBytes += event.args.size;
			} catch {}
		}
	}
	const eventDurationS = eventTimes.length > 1 ? Math.max(...eventTimes) - Math.min(...eventTimes) : 0;
	const rates = top(breakpointCounts, 50).map((item) => ({ ...item, per_s: eventDurationS > 0 ? Number((item.count / eventDurationS).toFixed(1)) : 0 }));
	const timing = [...durations.entries()]
		.map(([name, values]) => ({ name, breakpoint_count: breakpointCounts.get(name) ?? 0, ...durationStats(values, breakpointCounts.get(name) ?? 0) }))
		.sort((a, b) => b.estimated_total_us - a.estimated_total_us);
	const summary = {
		samples,
		events,
		event_duration_s: Number(eventDurationS.toFixed(3)),
		top_frames: top(topFrameCounts, 25, samples, intervalMs),
		hot_frames_any_depth: top(frameCounts),
		project_frames_any_depth: top(projectFrameCounts),
		project_frame_sample_hits: top(projectSampleCounts, 25, samples, intervalMs),
		breakpoints: top(breakpointCounts),
		rates,
		timing,
		observed_alloc_bytes: allocBytes,
	};
	writeFileSync(join(dir, "summary.json"), JSON.stringify(summary, null, 2), "utf8");
	return summary;
}

function status(): Record<string, unknown> {
	return activeRun
		? { active: true, id: activeRun.id, pid: activeRun.pid, target: activeRun.target, mode: activeRun.mode, trace_dir: activeRun.traceDir, samples: activeRun.samples, last_error: activeRun.lastError }
		: { active: false, candidates: discoverLockfiles() };
}

const paramsSchema = {
	type: "object",
	additionalProperties: false,
	properties: {
		action: { type: "string", description: "help|discover|start|stop|status|analyze" },
		target: { type: "string", description: "tx16s or tx16smk3; used to find simulator lockfile" },
		pid: { type: "integer", description: "Attach to explicit simulator PID instead of lockfile discovery" },
		mode: { type: "string", enum: ["sample", "breakpoints", "hybrid"], description: "sample=periodic backtraces; breakpoints=function/allocation events; hybrid=both" },
		interval_ms: { type: "integer", minimum: 100, maximum: 60000, description: "Sampling interval for sample/hybrid modes" },
		duration_ms: { type: "integer", minimum: 0, maximum: 86400000, description: "Optional auto-stop duration" },
		output_dir: { type: "string", description: "Trace root, default build/gdb-ui-traces" },
		label: { type: "string", description: "Optional label included in trace directory name" },
		functions: { type: "array", items: { type: "string" }, description: "Function names for breakpoint tracing" },
		allocs: { type: "boolean", description: "Trace malloc/calloc/realloc/free/new/delete breakpoints; high overhead" },
		backtrace_depth: { type: "integer", minimum: 1, maximum: 64, description: "Frames per sample/event" },
		timed: { type: "boolean", description: "Sample function durations with gdb FinishBreakpoint in breakpoint/hybrid mode" },
		timing_sample_rate: { type: "integer", minimum: 1, maximum: 10000, description: "Create one duration probe every N hits per function (default 25)" },
		max_timing_events: { type: "integer", minimum: 1, maximum: 200000, description: "Cap duration probes to avoid runaway overhead (default 5000)" },
		use_nix: { type: "boolean", description: "Run gdb through nix develop (default true)" },
		trace_dir: { type: "string", description: "Directory to analyze" },
	},
	required: ["action"],
} as const;

function help(): Record<string, unknown> {
	return {
		usage: [
			"1. edgetx_ui action=start args={target:'tx16s'}",
			"2. edgetx_gdb_trace action=start mode=sample target=tx16s interval_ms=500",
			"3. Drive UI with edgetx_ui actions/flows",
			"4. edgetx_gdb_trace action=stop",
			"5. edgetx_gdb_trace action=analyze trace_dir='build/gdb-ui-traces/...'",
		],
		modes: {
			sample: "Periodic gdb attach + thread backtraces. Lower setup, pauses simulator briefly each sample.",
			breakpoints: "Persistent gdb attach with Python breakpoints for selected functions/allocation APIs. High overhead.",
			hybrid: "Both sample and breakpoint modes. Highest overhead.",
		},
		outputs: ["manifest.json", "events.jsonl", "samples/*.bt", "summary.json", "gdb*.log"],
	};
}

export default function edgeTxGdbTrace(pi: ExtensionAPI) {
	pi.registerTool({
		name: "edgetx_gdb_trace",
		label: "EdgeTX GDB Trace",
		description: "Collect gdb-backed traces from EdgeTX UI-harness simulator runs: periodic backtraces, selected function breakpoints, and best-effort allocation events. Writes reusable trace bundles under build/gdb-ui-traces.",
		promptSnippet: "Collect/analyze gdb traces for EdgeTX UI harness performance investigations",
		promptGuidelines: [
			"Use edgetx_gdb_trace after starting the simulator with edgetx_ui; prefer mode=sample first because breakpoint/allocation tracing is intrusive.",
			"Use edgetx_gdb_trace action=stop before declaring trace collection complete so manifest.json and summary.json are finalized.",
		],
		parameters: paramsSchema as any,
		executionMode: "sequential",
		async execute(_toolCallId, rawParams) {
			const params = record(rawParams) as StartParams & { trace_dir?: string };
			const action = String(params.action ?? "help");
			let result: Record<string, unknown>;
			if (action === "help") result = help();
			else if (action === "discover") result = { candidates: discoverLockfiles(params.target) };
			else if (action === "start") result = startTrace(params);
			else if (action === "stop") result = await stopTrace();
			else if (action === "status") result = status();
			else if (action === "analyze") {
				const dir = params.trace_dir ? resolve(projectRoot(), params.trace_dir) : activeRun?.traceDir;
				if (!dir) throw new Error("trace_dir is required when no trace is active");
				result = { trace_dir: dir, summary: analyzeTraceDir(dir) };
			} else {
				throw new Error(`Unknown edgetx_gdb_trace action '${action}'`);
			}
			return { content: [{ type: "text", text: JSON.stringify(result, null, 2) }], details: result };
		},
	});

	pi.registerCommand("edgetx-gdb-trace", {
		description: "Show EdgeTX UI gdb trace status/help",
		handler: async (args, ctx) => {
			const action = args.trim() || "status";
			const result = action === "help" ? help() : status();
			ctx.ui.notify(JSON.stringify(result, null, 2), "info");
		},
	});

	pi.on("session_shutdown", async () => {
		await stopTrace("session shutdown");
	});
}
