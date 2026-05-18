import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { StringEnum } from "@earendil-works/pi-ai";
import { spawn } from "node:child_process";
import { resolve } from "node:path";
import { Type, type Static } from "typebox";

const checkSchema = Type.Object({
	check: StringEnum(["commit-tests", "strict-firmware", "asan-ubsan", "tsan", "safe-division", "ui-escape-hatches", "docs", "diff-check"] as const, {
		description: "Edge16 check preset to run.",
	}),
	target: Type.Optional(
		StringEnum(["tx16s", "tx16smk3"] as const, {
			description: "Radio target for target-specific checks. Defaults to tx16s.",
		}),
	),
	timeoutSeconds: Type.Optional(Type.Integer({ description: "Timeout. Defaults depend on the selected check." })),
	maxLines: Type.Optional(Type.Integer({ description: "Maximum output lines returned. Defaults to 180." })),
});

type CheckInput = Static<typeof checkSchema>;

interface RunResult {
	exitCode: number | null;
	timedOut: boolean;
	output: string;
	command: string[];
}

function defaultTimeout(check: CheckInput["check"]): number {
	switch (check) {
		case "commit-tests":
		case "asan-ubsan":
		case "tsan":
			return 900;
		case "strict-firmware":
			return 1200;
		case "docs":
			return 600;
		default:
			return 180;
	}
}

function checkCommand(params: CheckInput): string {
	const target = params.target ?? "tx16s";
	const threads = "${EDGE16_THREADS:-24}";
	const xdg = "${EDGE16_XDG_CONFIG_HOME:-/tmp/edge16-xdg}";
	const hardening = "bindnow format libcxxhardeningfast pic relro stackclashprotection stackprotector strictflexarrays1 strictoverflow zerocallusedregs";
	const envPrefix = `export EDGE16_THREADS=${threads}; export EDGE16_XDG_CONFIG_HOME=${xdg}; export EDGE16_NIX_HARDENING='${hardening}'; `;
	const baseEnv = `XDG_CONFIG_HOME="$EDGE16_XDG_CONFIG_HOME" NIX_HARDENING_ENABLE="$EDGE16_NIX_HARDENING" FLAVOR=${target} EDGE16_UV_ACTIVE=1 CMAKE_BUILD_PARALLEL_LEVEL="$EDGE16_THREADS"`;

	switch (params.check) {
		case "commit-tests":
			return `${envPrefix}nix develop -c env ${baseEnv} EXTRA_OPTIONS='-DWARNINGS_AS_ERRORS=YES -DEDGE16_SAFETY_CHECKS=ON -DDISABLE_COMPANION=ON' uv run --with-requirements requirements.txt bash ./tools/commit-tests.sh`;
		case "strict-firmware":
			return `${envPrefix}nix develop -c env ${baseEnv} GITHUB_REF=refs/heads/local-ci FIRMWARE_TARGET=firmware EXTRA_OPTIONS='-DWARNINGS_AS_ERRORS=YES -DEDGE16_SAFETY_CHECKS=ON' uv run --with-requirements requirements.txt bash ./tools/build-gh.sh`;
		case "asan-ubsan":
			return `${envPrefix}nix develop -c env ${baseEnv} EXTRA_OPTIONS='-DEDGE16_SANITIZERS=address,undefined -DEDGE16_SAFETY_CHECKS=ON -DDISABLE_COMPANION=ON' uv run --with-requirements requirements.txt bash ./tools/commit-tests.sh`;
		case "tsan":
			return `${envPrefix}nix develop -c env ${baseEnv} EXTRA_OPTIONS='-DEDGE16_SANITIZERS=thread -DEDGE16_SAFETY_CHECKS=ON -DDISABLE_COMPANION=ON' uv run --with-requirements requirements.txt bash ./tools/commit-tests.sh`;
		case "safe-division":
			return "nix develop -c python3 tools/check-safe-division.py";
		case "ui-escape-hatches":
			return "nix develop -c python3 tools/check-ui-escape-hatches.py";
		case "docs":
			return "nix develop -c uv run --with-requirements docs-requirements.txt mkdocs build --strict";
		case "diff-check":
			return "git diff --check";
	}
}

function limitOutput(output: string, maxLines: number): string {
	const normalized = output.replace(/\r\n/g, "\n").trim();
	if (!normalized) return "(no output)";
	const lines = normalized.split("\n");
	if (lines.length <= maxLines) return normalized;

	const interesting = /(^|[^A-Za-z])(error|fatal error|warning|FAILED:|FAIL|CMake Error|Traceback|No such file|not found|AddressSanitizer|UndefinedBehaviorSanitizer|ThreadSanitizer)([^A-Za-z]|$)/i;
	const selected = new Set<number>();
	for (let i = 0; i < lines.length; i++) {
		if (!interesting.test(lines[i])) continue;
		for (let j = Math.max(0, i - 3); j <= Math.min(lines.length - 1, i + 8); j++) selected.add(j);
	}
	if (selected.size > 0) {
		const out: string[] = [];
		let prev = -2;
		for (const i of Array.from(selected).sort((a, b) => a - b)) {
			if (i > prev + 1) out.push("...");
			out.push(lines[i]);
			prev = i;
		}
		if (out.length <= maxLines) return out.join("\n");
	}

	const head = Math.floor(maxLines * 0.25);
	const tail = maxLines - head;
	return [...lines.slice(0, head), `... [${lines.length - maxLines} lines omitted] ...`, ...lines.slice(lines.length - tail)].join("\n");
}

function runCheck(cwd: string, params: CheckInput, signal?: AbortSignal): Promise<RunResult> {
	const commandText = checkCommand(params);
	const timeoutMs = Math.max(1, params.timeoutSeconds ?? defaultTimeout(params.check)) * 1000;
	const command = ["bash", "-lc", commandText];

	return new Promise((resolveResult) => {
		let output = "";
		let timedOut = false;
		const child = spawn(command[0], command.slice(1), { cwd, env: process.env, stdio: ["ignore", "pipe", "pipe"] });

		const kill = () => {
			if (!child.killed) child.kill("SIGTERM");
			setTimeout(() => {
				if (!child.killed) child.kill("SIGKILL");
			}, 3000).unref();
		};

		const timer = setTimeout(() => {
			timedOut = true;
			output += `\n[edge16_check] timed out after ${timeoutMs / 1000}s\n`;
			kill();
		}, timeoutMs);

		const onAbort = () => {
			output += "\n[edge16_check] aborted by Pi\n";
			kill();
		};
		signal?.addEventListener("abort", onAbort, { once: true });

		child.stdout.on("data", (chunk) => {
			output += chunk.toString();
		});
		child.stderr.on("data", (chunk) => {
			output += chunk.toString();
		});
		child.on("error", (error) => {
			output += `\n[edge16_check] failed to start: ${error.message}\n`;
		});
		child.on("close", (exitCode) => {
			clearTimeout(timer);
			signal?.removeEventListener("abort", onAbort);
			resolveResult({ exitCode, timedOut, output, command });
		});
	});
}

export default function edge16Checks(pi: ExtensionAPI) {
	pi.registerTool({
		name: "edge16_check",
		label: "Edge16 Check",
		description: "Run common Edge16 verification presets without spelling out long nix/uv commands.",
		promptSnippet: "Use edge16_check for commit-tests, sanitizer, strict firmware, docs, and policy checks instead of long ad-hoc shell commands.",
		promptGuidelines: [
			"Use edge16_check for Edge16 verification presets instead of manually composing the long nix develop commands.",
			"For firmware/UI changes, run edge16_check with target=tx16s and target=tx16smk3 when target behavior can differ.",
		],
		parameters: checkSchema,
		async execute(_toolCallId, params, signal, _onUpdate, ctx) {
			const maxLines = Math.max(40, Math.min(params.maxLines ?? 180, 600));
			const result = await runCheck(ctx.cwd, params, signal);
			const ok = result.exitCode === 0 && !result.timedOut;
			const text = [
				`edge16_check ${ok ? "OK" : "FAILED"}`,
				`cwd: ${resolve(ctx.cwd)}`,
				`check: ${params.check}${params.target ? ` (${params.target})` : ""}`,
				`command: ${result.command.join(" ")}`,
				`exit: ${result.exitCode}${result.timedOut ? " (timeout)" : ""}`,
				"",
				limitOutput(result.output, maxLines),
			].join("\n");

			return {
				content: [{ type: "text", text }],
				details: { ok, exitCode: result.exitCode, timedOut: result.timedOut, command: result.command, check: params.check, target: params.target },
			};
		},
	});

	pi.registerCommand("edge16-check", {
		description: "Run an Edge16 verification preset: /edge16-check <check> [tx16s|tx16smk3]",
		handler: async (args, ctx) => {
			const parts = args.trim().split(/\s+/).filter(Boolean);
			const check = parts[0] as CheckInput["check"] | undefined;
			const allowed = ["commit-tests", "strict-firmware", "asan-ubsan", "tsan", "safe-division", "ui-escape-hatches", "docs", "diff-check"];
			if (!check || !allowed.includes(check)) {
				ctx.ui.notify(`Usage: /edge16-check <${allowed.join("|")}> [tx16s|tx16smk3]`, "warning");
				return;
			}
			const target = parts.find((part) => part === "tx16s" || part === "tx16smk3") as CheckInput["target"] | undefined;
			const result = await runCheck(ctx.cwd, { check, target, maxLines: 100 });
			ctx.ui.notify(limitOutput(result.output, 100), result.exitCode === 0 && !result.timedOut ? "info" : "error");
		},
	});
}
