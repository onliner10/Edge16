import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { existsSync } from "node:fs";
import { mkdtemp, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";

type JsonRpcResponse = {
	jsonrpc: "2.0";
	id: number;
	result?: unknown;
	error?: { code: number; message: string; data?: unknown };
};

type McpTool = {
	name: string;
	description?: string;
	inputSchema?: Record<string, unknown>;
};

type PendingCall = {
	resolve: (value: unknown) => void;
	reject: (error: Error) => void;
	timer: ReturnType<typeof setTimeout>;
	onAbort?: () => void;
	signal?: AbortSignal;
};

type FormattedResult = {
	content: string;
	details: Record<string, unknown>;
};


const ACTION_ALIASES: Record<string, string> = {
	help: "help",
	stop: "stop",
	build: "edgetx_build_simulator",
	start: "edgetx_start_simulator",
	status: "edgetx_status",
	press: "edgetx_press",
	long_press: "edgetx_long_press",
	rotate: "edgetx_rotate",
	touch: "edgetx_touch",
	drag: "edgetx_drag",
	scroll: "edgetx_scroll",
	scroll_to: "edgetx_scroll_to",
	wait: "edgetx_wait",
	tree: "edgetx_ui_tree",
	ui_tree: "edgetx_ui_tree",
	screen: "edgetx_screen",
	sitemap: "edgetx_sitemap",
	orient: "edgetx_orient",
	goto: "edgetx_goto",
	inspect: "edgetx_inspect",
	select_option: "edgetx_select_option",
	confirm: "edgetx_confirm",
	cancel: "edgetx_cancel",
	set_number: "edgetx_set_number",
	activate: "edgetx_activate",
	adjust_field: "edgetx_adjust_field",
	type_text: "edgetx_type_text",
	click: "edgetx_click",
	long_click: "edgetx_long_click",
	assert_visible: "edgetx_assert_visible",
	wait_for: "edgetx_wait_for",
	skip_storage_warning: "edgetx_skip_storage_warning_if_present",
	screenshot: "edgetx_screenshot",
	telemetry: "edgetx_set_telemetry",
	set_telemetry: "edgetx_set_telemetry",
	telemetry_streaming: "edgetx_set_telemetry_streaming",
	battery: "edgetx_set_batt_voltage",
	set_batt_voltage: "edgetx_set_batt_voltage",
	switch: "edgetx_set_switch",
	switch_sequence: "edgetx_switch_sequence",
	audio_history: "edgetx_audio_history",
	run_flow: "edgetx_run_flow",
	log: "edgetx_log",
};

const ACTION_HINTS: Record<string, string> = {
	edgetx_build_simulator: "Build simulator: args {target:'tx16s'|'tx16smk3'}.",
	edgetx_start_simulator: "Start persistent simulator: args {target:'tx16s'|'tx16smk3', sdcard?, settings?}.",
	edgetx_status: "Return simulator status. Defaults to args {compact:true} unless overridden.",
	edgetx_screen: "Best first inspection tool: compact page/context/actions/fields/visible_text summary.",
	edgetx_sitemap: "Return the firmware-owned route sitemap: stable routes plus verified templates.",
	edgetx_orient: "Small route-first state summary: current route/title, focus, capabilities, blocker, and hash.",
	edgetx_goto: "Navigate by stable firmware route id: args {route:'model.mixes'}.",
	edgetx_inspect: "Targeted current-screen details: args {fields?, actions?, visible_text?, editable_only?, text_contains?, limit?}.",
	edgetx_select_option: "Select an option from the current overlay/list, scrolling as needed: args {text:'CV1'}.",
	edgetx_confirm: "Confirm the current overlay/editor by clicking Ok or pressing ENTER.",
	edgetx_cancel: "Cancel the current overlay/editor by clicking Cancel or pressing EXIT.",
	edgetx_set_number: "Set active number-editor value with bounded rotary steps: args {value:0, label?, confirm?}.",
	edgetx_ui_tree: "LVGL tree. Prefer args {mode:'summary', actionable_only:true}; use mode:'full' sparingly.",
	edgetx_activate: "Activate visible node by automation_id/semantic_id/text/text_contains/role/id; args {automation_id:'...', action:'click'}.",
	edgetx_click: "Click visible node by selector; prefer automation_id over text/text_contains/role/raw coordinates.",
	edgetx_adjust_field: "Bounded field change: args {label:'Cells', target_value:'4', max_steps?, confirm?}.",
	edgetx_type_text: "Type into active visible field editor: args {text:'PACK1', submit:true}.",
	edgetx_scroll: "User-equivalent content scroll: args {direction:'down', amount:'page'}.",
	edgetx_wait_for: "Poll until visible: args {text_contains?, automation_id?, role?, timeout_ms?, poll_ms?}.",
	edgetx_skip_storage_warning_if_present: "Bounded helper for startup storage warnings; no args.",
	edgetx_screenshot: "Capture framebuffer PNG: args {name:'after-action', out_dir?}.",
	edgetx_press: "Press radio key: args {key:'ENTER'|'EXIT'|'MODEL'|..., duration_ms?}.",
	edgetx_rotate: "Rotary encoder steps: args {steps:1} or {steps:-1}.",
	edgetx_run_flow: "Run JSON flow: args {flow_path:'tools/ui-harness/flows/tx16s-smoke.json'}.",
	edgetx_log: "Return simulator log tail: args {filter?, tail?}.",
};

const DEFAULT_TIMEOUT_MS = 120_000;
const BUILD_TIMEOUT_MS = 20 * 60_000;
const MAX_RESULT_BYTES = 50 * 1024;
const MAX_RESULT_LINES = 2_000;

class McpClient {
	private child?: ChildProcessWithoutNullStreams;
	private startPromise?: Promise<void>;
	private nextId = 1;
	private buffer = "";
	private pending = new Map<number, PendingCall>();
	private stderrTail: string[] = [];
	private toolsCache?: McpTool[];

	constructor(private readonly cwd: string) {}

	isRunning(): boolean {
		return this.child !== undefined;
	}

	async listTools(signal?: AbortSignal): Promise<McpTool[]> {
		await this.ensureStarted();
		if (this.toolsCache) return this.toolsCache;
		const result = (await this.request("tools/list", {}, DEFAULT_TIMEOUT_MS, signal)) as { tools?: McpTool[] };
		this.toolsCache = result.tools ?? [];
		return this.toolsCache;
	}

	async callTool(name: string, args: Record<string, unknown>, timeoutMs: number, signal?: AbortSignal): Promise<string> {
		await this.ensureStarted();
		const result = (await this.request("tools/call", { name, arguments: args }, timeoutMs, signal)) as {
			content?: Array<{ type?: string; text?: string }>;
		};
		const text = result.content?.find((item) => item.type === "text")?.text;
		return typeof text === "string" ? text : JSON.stringify(result);
	}

	async shutdown(): Promise<void> {
		if (!this.child) return;
		try {
			await this.callTool("edgetx_stop_simulator", {}, 15_000);
		} catch {
			// Best-effort: the simulator may not have been started.
		} finally {
			await this.stopProcess();
		}
	}

	private async ensureStarted(): Promise<void> {
		if (this.child && this.child.stdin.writable) return;
		if (this.startPromise) return this.startPromise;
		this.startPromise = this.startProcess().finally(() => {
			this.startPromise = undefined;
		});
		return this.startPromise;
	}

	private async startProcess(): Promise<void> {
		this.toolsCache = undefined;
		const spec = this.commandSpec();
		const child = spawn(spec.command, spec.args, {
			cwd: this.cwd,
			shell: spec.shell,
			stdio: ["pipe", "pipe", "pipe"],
			env: { ...process.env, PYTHONUNBUFFERED: "1" },
		});
		this.child = child;
		this.buffer = "";

		child.stdout.on("data", (chunk: Buffer) => this.onStdout(chunk));
		child.stderr.on("data", (chunk: Buffer) => this.onStderr(chunk));
		child.on("error", (error) => {
			if (this.child !== child) return;
			this.child = undefined;
			this.rejectAll(error);
		});
		child.on("exit", (code, signal) => {
			if (this.child !== child) return;
			this.child = undefined;
			this.rejectAll(new Error(`edgetx-mcp exited with code ${code ?? "?"}${signal ? ` signal ${signal}` : ""}${this.stderrSummary()}`));
		});

		await this.request("initialize", {
			protocolVersion: "2024-11-05",
			capabilities: {},
			clientInfo: { name: "pi-edgetx-ui-harness", version: "0.2.0" },
		});
		this.notify("notifications/initialized", {});
	}

	private commandSpec(): { command: string; args: string[]; shell: boolean } {
		const override = process.env.EDGETX_UI_MCP_COMMAND;
		if (override) return { command: override, args: [], shell: true };

		const local = join(this.cwd, "tools", "ui-harness", "edgetx-mcp");
		if (existsSync(local)) return { command: local, args: [], shell: false };
		return { command: "tools/ui-harness/edgetx-mcp", args: [], shell: false };
	}

	private request(method: string, params: unknown, timeoutMs = DEFAULT_TIMEOUT_MS, signal?: AbortSignal): Promise<unknown> {
		const child = this.child;
		if (!child || !child.stdin.writable) {
			return Promise.reject(new Error("edgetx-mcp is not running"));
		}
		if (signal?.aborted) return Promise.reject(new Error("cancelled"));

		const id = this.nextId++;
		const payload = { jsonrpc: "2.0", id, method, params };
		return new Promise((resolve, reject) => {
			const cleanup = () => {
				const pending = this.pending.get(id);
				if (pending?.signal && pending.onAbort) pending.signal.removeEventListener("abort", pending.onAbort);
				this.pending.delete(id);
			};
			const timer = setTimeout(() => {
				cleanup();
				reject(new Error(`${method} timed out after ${timeoutMs}ms${this.stderrSummary()}`));
			}, timeoutMs);
			const onAbort = signal
				? () => {
					cleanup();
					clearTimeout(timer);
					reject(new Error("cancelled"));
				}
				: undefined;
			if (signal && onAbort) signal.addEventListener("abort", onAbort, { once: true });
			this.pending.set(id, { resolve, reject, timer, onAbort, signal });
			child.stdin.write(`${JSON.stringify(payload)}\n`, (error) => {
				if (!error) return;
				cleanup();
				clearTimeout(timer);
				reject(error);
			});
		});
	}

	private notify(method: string, params: unknown): void {
		this.child?.stdin.write(`${JSON.stringify({ jsonrpc: "2.0", method, params })}\n`);
	}

	private onStdout(chunk: Buffer): void {
		this.buffer += chunk.toString("utf8");
		for (;;) {
			const newline = this.buffer.indexOf("\n");
			if (newline < 0) return;
			const line = this.buffer.slice(0, newline).trim();
			this.buffer = this.buffer.slice(newline + 1);
			if (!line) continue;
			this.handleLine(line);
		}
	}

	private handleLine(line: string): void {
		let response: JsonRpcResponse;
		try {
			response = JSON.parse(line) as JsonRpcResponse;
		} catch {
			this.stderrTail.push(`non-JSON stdout: ${line}`);
			this.stderrTail = this.stderrTail.slice(-20);
			return;
		}

		const pending = this.pending.get(response.id);
		if (!pending) return;
		if (pending.signal && pending.onAbort) pending.signal.removeEventListener("abort", pending.onAbort);
		this.pending.delete(response.id);
		clearTimeout(pending.timer);
		if (response.error) {
			pending.reject(new Error(response.error.message));
		} else {
			pending.resolve(response.result);
		}
	}

	private onStderr(chunk: Buffer): void {
		this.stderrTail.push(...chunk.toString("utf8").split(/\r?\n/).filter(Boolean));
		this.stderrTail = this.stderrTail.slice(-20);
	}

	private stderrSummary(): string {
		return this.stderrTail.length ? `\nstderr tail:\n${this.stderrTail.join("\n")}` : "";
	}

	private rejectAll(error: Error): void {
		for (const [id, pending] of this.pending) {
			if (pending.signal && pending.onAbort) pending.signal.removeEventListener("abort", pending.onAbort);
			clearTimeout(pending.timer);
			pending.reject(error);
			this.pending.delete(id);
		}
	}

	private async stopProcess(): Promise<void> {
		const child = this.child;
		if (!child) return;
		this.child = undefined;
		this.toolsCache = undefined;
		this.rejectAll(new Error("edgetx-mcp stopped"));
		await new Promise<void>((resolve) => {
			let done = false;
			const finish = () => {
				if (done) return;
				done = true;
				clearTimeout(killTimer);
				resolve();
			};
			const killTimer = setTimeout(() => {
				child.kill("SIGKILL");
				finish();
			}, 2_000);
			child.once("exit", finish);
			child.kill("SIGTERM");
		});
	}
}

async function normalizeAction(action: unknown, client: McpClient, signal?: AbortSignal): Promise<string> {
	const value = typeof action === "string" ? action.trim() : "";
	if (!value) return "help";
	if (ACTION_ALIASES[value]) return ACTION_ALIASES[value];
	if (value === "help" || value === "stop" || value === "edgetx_stop_simulator") return value;
	const toolNames = new Set((await client.listTools(signal)).map((tool) => tool.name));
	if (toolNames.has(value)) return value;
	const prefixed = `edgetx_${value}`;
	if (toolNames.has(prefixed)) return prefixed;
	return value;
}

function asRecord(value: unknown): Record<string, unknown> {
	return value && typeof value === "object" && !Array.isArray(value) ? (value as Record<string, unknown>) : {};
}

function timeoutFor(action: string, args: Record<string, unknown>, requested: unknown): number {
	const explicit = typeof requested === "number" ? requested : Number(requested ?? 0);
	if (Number.isFinite(explicit) && explicit > 0) return Math.min(Math.max(explicit, 1_000), 30 * 60_000);
	if (action === "edgetx_build_simulator") return BUILD_TIMEOUT_MS;
	if (action === "edgetx_start_simulator") return BUILD_TIMEOUT_MS + DEFAULT_TIMEOUT_MS;
	if (action === "edgetx_wait") {
		const ms = typeof args.ms === "number" ? args.ms : Number(args.ms ?? 0);
		return Math.max(DEFAULT_TIMEOUT_MS, ms + 10_000);
	}
	if (action === "edgetx_wait_for") {
		const ms = typeof args.timeout_ms === "number" ? args.timeout_ms : Number(args.timeout_ms ?? 2_000);
		return Math.max(DEFAULT_TIMEOUT_MS, ms + 10_000);
	}
	return DEFAULT_TIMEOUT_MS;
}

function parseJson(text: string): unknown | undefined {
	try {
		return JSON.parse(text);
	} catch {
		return undefined;
	}
}

function isMissingSimulatorExecutable(error: unknown): boolean {
	const message = error instanceof Error ? error.message : String(error);
	return message.includes("could not find built simulator executable") && message.includes("edgetx-ui build");
}

// --- ISON rendering (output only; MCP transport stays JSON) ---

function isonQuoteString(s: string): string {
	return '"' + s.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\n/g, "\\n").replace(/\t/g, "\\t").replace(/\r/g, "\\r") + '"';
}

function isonNeedsQuote(s: string): boolean {
	if (s === "") return true;
	if (/[\s"\\]/.test(s)) return true;
	if (s === "true" || s === "false" || s === "null") return true;
	if (/^-?[0-9]+$/.test(s)) return true;
	if (/^-?[0-9]+\.[0-9]+$/.test(s)) return true;
	if (s.startsWith(":")) return true;
	return false;
}

function isonFieldName(key: string): string {
	return key.replace(/[\s]/g, "_");
}

function isonFormatScalar(value: unknown): string {
	if (value === null || value === undefined) return "null";
	if (typeof value === "boolean") return value ? "true" : "false";
	if (typeof value === "number") return Number.isFinite(value) ? String(value) : "null";
	if (typeof value === "object") return isonQuoteString(JSON.stringify(value));
	const s = String(value);
	return isonNeedsQuote(s) ? isonQuoteString(s) : s;
}

function isonRootName(action: string): string {
	return isonFieldName(action.replace(/^edgetx_/, "") || "result");
}

function isonFlattenObjectLeaves(obj: Record<string, unknown>, prefix: string, out: Array<[string, unknown]>): void {
	for (const key of Object.keys(obj)) {
		const path = prefix ? `${prefix}.${isonFieldName(key)}` : isonFieldName(key);
		const v = obj[key];
		if (v !== null && typeof v === "object" && !Array.isArray(v)) {
			isonFlattenObjectLeaves(v as Record<string, unknown>, path, out);
		} else {
			out.push([path, v]);
		}
	}
}

function isonEmitObject(obj: Record<string, unknown>, name: string, blocks: string[]): void {
	const scalars: Array<[string, unknown]> = [];
	const collections: Array<[string, unknown]> = [];
	const walk = (o: Record<string, unknown>, prefix: string): void => {
		for (const key of Object.keys(o)) {
			const path = prefix ? `${prefix}.${isonFieldName(key)}` : isonFieldName(key);
			const v = o[key];
			if (Array.isArray(v)) {
				collections.push([isonFieldName(key), v]);
			} else if (v !== null && typeof v === "object") {
				walk(v as Record<string, unknown>, path);
			} else {
				scalars.push([path, v]);
			}
		}
	};
	walk(obj, "");
	if (scalars.length > 0) {
		blocks.push(`object.${name}\n${scalars.map(([p]) => p).join(" ")}\n${scalars.map(([, v]) => isonFormatScalar(v)).join(" ")}`);
	}
	for (const [childName, childVal] of collections) {
		isonEmit(childVal, childName, blocks);
	}
	if (scalars.length === 0 && collections.length === 0) {
		blocks.push(`object.${name}\n_empty true`);
	}
}

function isonEmitTable(rows: Array<Record<string, unknown>>, name: string, blocks: string[]): void {
	const fieldList: string[] = [];
	const fieldSet = new Set<string>();
	for (const row of rows) {
		for (const key of Object.keys(row)) {
			const f = isonFieldName(key);
			if (!fieldSet.has(f)) {
				fieldSet.add(f);
				fieldList.push(f);
			}
		}
	}
	const header = fieldList.join(" ");
	const dataLines = rows.map((row) => fieldList.map((f) => isonFormatScalar(row[f] ?? null)).join(" "));
	blocks.push(`table.${name}\n${header}\n${dataLines.join("\n")}`);
}

function isonEmit(value: unknown, name: string, blocks: string[]): void {
	if (Array.isArray(value)) {
		if (value.length === 0) {
			blocks.push(`object.${name}\n_length 0`);
			return;
		}
		if (value.every((v) => v !== null && typeof v === "object" && !Array.isArray(v))) {
			isonEmitTable(value as Array<Record<string, unknown>>, name, blocks);
		} else {
			blocks.push(`table.${name}\nvalue\n${value.map((v) => isonFormatScalar(v)).join("\n")}`);
		}
		return;
	}
	if (value !== null && typeof value === "object") {
		isonEmitObject(value as Record<string, unknown>, name, blocks);
		return;
	}
	blocks.push(`object.${name}\nvalue\n${isonFormatScalar(value)}`);
}

function jsonToIson(value: unknown, name: string): string {
	const blocks: string[] = [];
	isonEmit(value, name, blocks);
	return blocks.length ? blocks.join("\n\n") : `object.${name}\nvalue\nnull`;
}

async function formatResult(action: string, rawText: string): Promise<FormattedResult> {
	const parsed = parseJson(rawText);
	const rootName = isonRootName(action);
	const rendered = parsed === undefined ? rawText.trim() : jsonToIson(parsed, rootName);
	const lines = rendered.split("\n");
	const bytes = Buffer.byteLength(rendered, "utf8");
	if (lines.length <= MAX_RESULT_LINES && bytes <= MAX_RESULT_BYTES) {
		return { content: rendered || `object.${rootName}\nvalue\nnull`, details: { action, format: "ison", result: parsed ?? rendered } };
	}

	const tempDir = await mkdtemp(join(tmpdir(), "pi-edgetx-ui-"));
	const fullOutputPath = join(tempDir, `${action}.json`);
	await writeFile(fullOutputPath, JSON.stringify(parsed ?? rawText, null, 2), "utf8");

	let truncated = lines.slice(0, MAX_RESULT_LINES).join("\n");
	if (Buffer.byteLength(truncated, "utf8") > MAX_RESULT_BYTES) {
		truncated = Buffer.from(truncated, "utf8").subarray(0, MAX_RESULT_BYTES).toString("utf8");
	}
	truncated += `\n\n[Output truncated: ${lines.length} lines, ${bytes} bytes. Full JSON saved to: ${fullOutputPath}]`;
	return {
		content: truncated,
		details: { action, format: "ison", truncated: true, fullOutputPath, totalLines: lines.length, totalBytes: bytes },
	};
}

async function helpText(args: Record<string, unknown>, client: McpClient, signal?: AbortSignal): Promise<string> {
	const requested = await normalizeAction(args.action ?? args.tool ?? args.name, client, signal);
	const tools = await client.listTools(signal);
	if (args.full === true) {
		const selected = requested === "help" ? tools : tools.filter((tool) => tool.name === requested);
		return JSON.stringify({ tools: selected.length ? selected : tools });
	}
	if (requested !== "help") {
		const tool = tools.find((entry) => entry.name === requested);
		return JSON.stringify({
			action: requested,
			hint: ACTION_HINTS[requested] ?? tool?.description ?? "No compact hint is available. Use args {full:true, action:'...'} for the MCP schema.",
		});
	}
	return JSON.stringify({
		usage: "Call edgetx_ui with {action:'edgetx_screen'|'screen'|..., args:{...}}. Use {action:'help', args:{action:'edgetx_adjust_field'}} for a compact action hint or args.full=true for MCP schemas.",
		recommended_flow: [
			"edgetx_start_simulator target=tx16s or tx16smk3",
			"edgetx_status compact=true; if startup_blocker appears, call edgetx_skip_storage_warning_if_present",
			"Prefer edgetx_sitemap once per session, then edgetx_orient after most actions",
			"Use edgetx_screen or edgetx_ui_tree mode=summary/actionable_only only when route/focus context is insufficient",
			"Prefer automation_id or semantic_id selectors; use raw touch coordinates only for hit-testing/timing work",
			"Capture edgetx_screenshot for risky navigation, overlays, model switching, or surprising UI-tree results",
		],
		common_actions: tools.map((tool) => ({ action: tool.name, hint: ACTION_HINTS[tool.name] ?? tool.description ?? "Use args.full=true for schema." })),
		all_actions: ["help", "stop", ...tools.map((tool) => tool.name)],
	});
}

const EdgetxUiParams = {
	type: "object",
	additionalProperties: false,
	properties: {
		action: {
			type: "string",
			description: "UI harness action. Use MCP names like edgetx_screen/edgetx_sitemap/edgetx_start_simulator or short aliases like screen/sitemap/orient/goto/inspect/start/status/help/stop.",
		},
		args: {
			type: "object",
			additionalProperties: true,
			description: "Arguments passed to the selected action. Use action=help for compact hints or args.full=true for exact MCP schemas.",
			default: {},
		},
		timeout_ms: {
			type: "integer",
			description: "Optional per-call timeout. Defaults are bounded; builds get a longer timeout.",
			minimum: 1_000,
			maximum: 1_800_000,
		},
	},
	required: ["action"],
} as const;

export default function (pi: ExtensionAPI) {
	const client = new McpClient(process.cwd());

	pi.registerTool({
		name: "edgetx_ui",
		label: "EdgeTX UI Harness",
		description:
			"Interact with the EdgeTX TX16S UI harness through one compact lazy MCP wrapper. Actions include start/status/sitemap/orient/goto/inspect/select_option/confirm/cancel/set_number/screen/ui_tree/activate/click/adjust_field/type_text/scroll/wait_for/screenshot/log/run_flow/help/stop. MCP transport stays JSON; successful tool output is rendered to the agent as compact ISON (tables/blocks). Errors are plain text. The MCP process starts on first live discovery/call.",
		promptSnippet: "Interact with the EdgeTX UI harness via one compact action wrapper",
		promptGuidelines: [
			"Prefer edgetx_ui action=sitemap once per session and action=orient after most actions; use action=screen or action=ui_tree with args {mode:'summary'} when more detail is needed.",
			"Use edgetx_ui selectors in this order: automation_id or semantic_id, exact text, text_contains with role/index, raw coordinates only for hit-testing/timing.",
		],
		parameters: EdgetxUiParams as any,
		executionMode: "sequential",
		prepareArguments(raw) {
			const input = asRecord(raw);
			const action =
				typeof input.action === "string" ? input.action : typeof input.tool === "string" ? input.tool : typeof input.name === "string" ? input.name : undefined;
			if (!action) return raw;

			const args = { ...asRecord(input.args) };
			for (const [key, value] of Object.entries(input)) {
				if (!["action", "tool", "name", "args", "timeout_ms"].includes(key)) args[key] = value;
			}

			return input.timeout_ms === undefined ? { action, args } : { action, args, timeout_ms: input.timeout_ms };
		},

		async execute(_toolCallId, params, signal, onUpdate) {
			const input = asRecord(params);
			const action = await normalizeAction(input.action, client, signal);
			const args = { ...asRecord(input.args) };

			if (action === "help") {
				const text = await helpText(args, client, signal);
				const formatted = await formatResult(action, text);
				return { content: [{ type: "text", text: formatted.content }], details: formatted.details };
			}

			if (action === "stop" || action === "edgetx_stop_simulator") {
				await client.shutdown();
				return { content: [{ type: "text", text: JSON.stringify({ ok: true, stopped: true }) }], details: { action, stopped: true } };
			}

			const toolNames = new Set((await client.listTools(signal)).map((tool) => tool.name));
			if (!toolNames.has(action)) {
				throw new Error(`Unknown EdgeTX UI harness action '${String(input.action)}'. Call edgetx_ui with action='help' for supported actions.`);
			}

			if (action === "edgetx_status" && args.compact === undefined) args.compact = true;
			onUpdate?.({ content: [{ type: "text", text: `${client.isRunning() ? "Calling" : "Starting MCP and calling"} ${action}...` }] });

			let rawText: string;
			let autoBuild: unknown;
			try {
				rawText = await client.callTool(action, args, timeoutFor(action, args, input.timeout_ms), signal);
			} catch (error) {
				if (action !== "edgetx_start_simulator" || !isMissingSimulatorExecutable(error)) throw error;

				const target = typeof args.target === "string" ? args.target : "tx16s";
				onUpdate?.({ content: [{ type: "text", text: `Simulator for ${target} is not built; building it first...` }] });
				const buildText = await client.callTool("edgetx_build_simulator", { target }, BUILD_TIMEOUT_MS, signal);
				autoBuild = parseJson(buildText) ?? buildText;
				onUpdate?.({ content: [{ type: "text", text: `Build finished; starting ${target} simulator...` }] });
				rawText = await client.callTool(action, args, DEFAULT_TIMEOUT_MS, signal);
			}

			const formatted = await formatResult(action, rawText);
			if (autoBuild !== undefined) formatted.details.autoBuild = autoBuild;
			return { content: [{ type: "text", text: formatted.content }], details: formatted.details };
		},
	});

	pi.registerCommand("edgetx-ui-stop", {
		description: "Stop the EdgeTX UI harness simulator and lazy MCP process",
		handler: async (_args, ctx) => {
			await client.shutdown();
			ctx.ui.notify("EdgeTX UI harness stopped", "info");
		},
	});

	pi.on("session_shutdown", async () => {
		await client.shutdown();
	});
}
