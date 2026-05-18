import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { spawn } from "node:child_process";
import { resolve } from "node:path";
import { Type, type Static } from "typebox";

const ghFailureSchema = Type.Object({
	runUrl: Type.Optional(
		Type.String({ description: "GitHub Actions run URL, e.g. https://github.com/OWNER/REPO/actions/runs/RUN_ID." }),
	),
	repo: Type.Optional(Type.String({ description: "GitHub repository as OWNER/REPO. Required when runUrl is not provided." })),
	runId: Type.Optional(Type.String({ description: "GitHub Actions run id. Required when runUrl is not provided." })),
	timeoutSeconds: Type.Optional(Type.Integer({ description: "Timeout for retrieving logs. Defaults to 240." })),
	maxLines: Type.Optional(Type.Integer({ description: "Maximum output lines returned to the model. Defaults to 220." })),
});

type GhFailureInput = Static<typeof ghFailureSchema>;

interface RunResult {
	exitCode: number | null;
	timedOut: boolean;
	output: string;
	command: string[];
}

function limitLines(output: string, maxLines: number): string {
	const normalized = output.replace(/\r\n/g, "\n").trim();
	if (!normalized) return "(no output)";
	const lines = normalized.split("\n");
	if (lines.length <= maxLines) return normalized;
	const head = Math.floor(maxLines * 0.75);
	const tail = maxLines - head;
	return [
		...lines.slice(0, head),
		`... [${lines.length - maxLines} failure-output lines omitted] ...`,
		...lines.slice(lines.length - tail),
	].join("\n");
}

function runGhFailure(cwd: string, params: GhFailureInput, signal?: AbortSignal): Promise<RunResult> {
	const args = ["tools/edge16-gh-failure"];
	if (params.runUrl) {
		args.push(params.runUrl);
	} else if (params.repo && params.runId) {
		args.push(params.repo, params.runId);
	} else {
		throw new Error("github_failure requires runUrl or both repo and runId");
	}

	const timeoutMs = Math.max(1, params.timeoutSeconds ?? 240) * 1000;
	const command = ["python3", ...args];

	return new Promise((resolveResult) => {
		let output = "";
		let timedOut = false;
		const child = spawn(command[0], command.slice(1), {
			cwd,
			env: process.env,
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
			output += `\n[github_failure] timed out after ${timeoutMs / 1000}s\n`;
			kill();
		}, timeoutMs);

		const onAbort = () => {
			output += "\n[github_failure] aborted by Pi\n";
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
			output += `\n[github_failure] failed to start: ${error.message}\n`;
		});
		child.on("close", (exitCode) => {
			clearTimeout(timer);
			signal?.removeEventListener("abort", onAbort);
			resolveResult({ exitCode, timedOut, output, command });
		});
	});
}

export default function edge16GhFailure(pi: ExtensionAPI) {
	pi.registerTool({
		name: "github_failure",
		label: "GitHub Failure",
		description: "Retrieve compact failed-job excerpts from a GitHub Actions workflow run using gh.",
		promptSnippet: "Use github_failure to inspect only the failure excerpts from a GitHub Actions run URL instead of dumping full CI logs.",
		promptGuidelines: [
			"Use github_failure when the user provides a GitHub Actions run URL and asks what failed.",
			"github_failure returns only failed jobs and compact failure context; use bash with gh only if deeper raw log inspection is needed.",
		],
		parameters: ghFailureSchema,
		async execute(_toolCallId, params, signal, _onUpdate, ctx) {
			const maxLines = Math.max(40, Math.min(params.maxLines ?? 220, 600));
			const result = await runGhFailure(ctx.cwd, params, signal);
			const commandFailed = result.exitCode !== 0 && result.exitCode !== 1;
			const toolFailed = commandFailed || result.timedOut;
			const text = [
				`github_failure ${toolFailed ? "FAILED" : "OK"}`,
				`cwd: ${resolve(ctx.cwd)}`,
				`command: ${result.command.join(" ")}`,
				`exit: ${result.exitCode}${result.timedOut ? " (timeout)" : ""}`,
				"",
				limitLines(result.output, maxLines),
			].join("\n");

			return {
				content: [{ type: "text", text }],
				details: {
					ok: !toolFailed,
					exitCode: result.exitCode,
					timedOut: result.timedOut,
					command: result.command,
				},
				isError: toolFailed,
			};
		},
	});

	pi.registerCommand("gh-failure", {
		description: "Show compact failed-job excerpts for a GitHub Actions run URL",
		handler: async (args, ctx) => {
			const runUrl = args.trim();
			if (!runUrl) {
				ctx.ui.notify("Usage: /gh-failure <github-actions-run-url>", "warning");
				return;
			}
			const result = await runGhFailure(ctx.cwd, { runUrl, maxLines: 160 });
			ctx.ui.notify(limitLines(result.output, 160), result.exitCode === 0 || result.exitCode === 1 ? "info" : "error");
		},
	});
}
