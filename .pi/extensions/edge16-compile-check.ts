import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { spawn } from "node:child_process";
import { resolve } from "node:path";
import { Type, type Static } from "typebox";

const compileCheckSchema = Type.Object({
	mode: Type.Optional(
		Type.Union([Type.Literal("check-file"), Type.Literal("build")], {
			description: "check-file runs fast clangd compiler diagnostics for one source file; build runs an incremental Ninja build.",
		}),
	),
	target: Type.Optional(
		Type.Union([Type.Literal("tx16s"), Type.Literal("tx16smk3")], {
			description: "Supported Edge16 hardware target. Use both targets when shared target/control code can differ.",
		}),
	),
	sourceFile: Type.Optional(
		Type.String({ description: "Repository-relative C/C++ source file for mode=check-file, e.g. radio/src/crc.cpp." }),
	),
	buildTarget: Type.Optional(
		Type.String({ description: "CMake/Ninja target for mode=build. Defaults to firmware." }),
	),
	reconfigure: Type.Optional(
		Type.Boolean({ description: "Force CMake reconfigure before checking. Default false; cached build dirs are reused." }),
	),
	timeoutSeconds: Type.Optional(
		Type.Integer({ description: "Timeout for the check. Defaults to 120 for check-file and 900 for build." }),
	),
	maxLines: Type.Optional(
		Type.Integer({ description: "Maximum diagnostic lines returned to the model. Defaults to 160." }),
	),
});

type CompileCheckInput = Static<typeof compileCheckSchema>;

interface RunResult {
	exitCode: number | null;
	timedOut: boolean;
	output: string;
	command: string[];
}

function limitLines(lines: string[], maxLines: number): string {
	if (lines.length <= maxLines) return lines.join("\n");
	const head = Math.floor(maxLines * 0.65);
	const tail = maxLines - head;
	return [
		...lines.slice(0, head),
		`... [${lines.length - maxLines} diagnostic/output lines omitted] ...`,
		...lines.slice(lines.length - tail),
	].join("\n");
}

function summarizeOutput(output: string, maxLines: number): string {
	const normalized = output.replace(/\r\n/g, "\n").trim();
	if (!normalized) return "(no output)";

	const allLines = normalized.split("\n");
	const interestingPattern =
		/(^|[^A-Za-z])(error|fatal error|warning|undefined reference|FAILED:|ninja:|CMake Error|Traceback|No such file|not found)([^A-Za-z]|$)/i;
	const selected = new Set<number>();

	for (let i = 0; i < allLines.length; i++) {
		if (!interestingPattern.test(allLines[i])) continue;
		for (let j = Math.max(0, i - 2); j <= Math.min(allLines.length - 1, i + 6); j++) {
			selected.add(j);
		}
	}

	if (selected.size > 0) {
		const withContext: string[] = [];
		let previous = -2;
		for (const index of Array.from(selected).sort((a, b) => a - b)) {
			if (index > previous + 1) withContext.push("...");
			withContext.push(allLines[index]);
			previous = index;
		}
		return limitLines(withContext, maxLines);
	}

	return limitLines(allLines.slice(-maxLines), maxLines);
}

function runCompileCheck(cwd: string, params: CompileCheckInput, signal?: AbortSignal): Promise<RunResult> {
	const mode = params.mode ?? (params.sourceFile ? "check-file" : "build");
	const args = ["tools/edge16-compile-check", mode, "--target", params.target ?? "tx16s"];

	if (params.reconfigure) args.push("--reconfigure");
	if (mode === "build") args.push("--build-target", params.buildTarget ?? "firmware");
	if (mode === "check-file") {
		if (!params.sourceFile) {
			throw new Error("compile_check mode=check-file requires sourceFile");
		}
		args.push(params.sourceFile);
	}

	const timeoutMs = Math.max(1, params.timeoutSeconds ?? (mode === "build" ? 900 : 120)) * 1000;
	const command = ["bash", ...args];

	return new Promise((resolveResult) => {
		let output = "";
		let timedOut = false;
		const child = spawn(command[0], command.slice(1), {
			cwd,
			env: {
				...process.env,
				EDGE16_THREADS: process.env.EDGE16_THREADS ?? "24",
				CMAKE_BUILD_PARALLEL_LEVEL: process.env.CMAKE_BUILD_PARALLEL_LEVEL ?? process.env.EDGE16_THREADS ?? "24",
			},
			stdio: ["ignore", "pipe", "pipe"],
		});

		const kill = () => {
			if (!child.killed) child.kill("SIGTERM");
			setTimeout(() => {
				if (!child.killed) child.kill("SIGKILL");
			}, 3000).unref();
		};

		const timer = setTimeout(() => {
			timedOut = true;
			output += `\n[compile_check] timed out after ${timeoutMs / 1000}s\n`;
			kill();
		}, timeoutMs);

		const onAbort = () => {
			output += "\n[compile_check] aborted by Pi\n";
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
			output += `\n[compile_check] failed to start: ${error.message}\n`;
		});
		child.on("close", (exitCode) => {
			clearTimeout(timer);
			signal?.removeEventListener("abort", onAbort);
			resolveResult({ exitCode, timedOut, output, command });
		});
	});
}

export default function edge16CompileCheck(pi: ExtensionAPI) {
	pi.registerTool({
		name: "compile_check",
		label: "Compile Check",
		description:
			"Run fast, cached Edge16 compilation diagnostics via direnv/nix. Use mode=check-file with a sourceFile for quick clangd compiler errors, or mode=build for an incremental cached Ninja build.",
		promptSnippet:
			"Use compile_check for concise Edge16 compile feedback instead of ad-hoc nix build commands; it reuses cached CMake/Ninja build directories.",
		promptGuidelines: [
			"Prefer compile_check mode=check-file for edited C/C++ files before broad builds.",
			"Use target=tx16s and target=tx16smk3 when a change can differ between supported TX16S targets.",
			"Use mode=build only when a full incremental target build is needed.",
		],
		parameters: compileCheckSchema,
		async execute(_toolCallId, params, signal, _onUpdate, ctx) {
			const maxLines = Math.max(20, Math.min(params.maxLines ?? 160, 400));
			const result = await runCompileCheck(ctx.cwd, params, signal);
			const summary = summarizeOutput(result.output, maxLines);
			const ok = result.exitCode === 0 && !result.timedOut;
			const text = [
				`compile_check ${ok ? "OK" : "FAILED"}`,
				`cwd: ${resolve(ctx.cwd)}`,
				`command: ${result.command.join(" ")}`,
				`exit: ${result.exitCode}${result.timedOut ? " (timeout)" : ""}`,
				"",
				summary,
			].join("\n");

			return {
				content: [{ type: "text", text }],
				details: {
					ok,
					exitCode: result.exitCode,
					timedOut: result.timedOut,
					command: result.command,
				},
				isError: !ok,
			};
		},
	});

	pi.registerCommand("compile-check", {
		description: "Run cached compile diagnostics: /compile-check <source-file> [tx16s|tx16smk3]",
		handler: async (args, ctx) => {
			const parts = args.trim().split(/\s+/).filter(Boolean);
			const sourceFile = parts.find((part) => !part.startsWith("--") && part !== "tx16s" && part !== "tx16smk3");
			const target = (parts.find((part) => part === "tx16s" || part === "tx16smk3") ?? "tx16s") as "tx16s" | "tx16smk3";
			if (!sourceFile) {
				ctx.ui.notify("Usage: /compile-check <source-file> [tx16s|tx16smk3]", "warning");
				return;
			}
			const result = await runCompileCheck(ctx.cwd, { mode: "check-file", sourceFile, target, maxLines: 80 });
			ctx.ui.notify(summarizeOutput(result.output, 80), result.exitCode === 0 ? "info" : "error");
		},
	});
}
