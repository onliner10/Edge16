/**
 * lib-docs — Embedded library documentation search for Edge16
 *
 * Provides a `lib_docs` tool that searches locally-available documentation
 * for the specific library versions used in this project:
 *
 *   - LVGL v9.5.0        (radio/src/thirdparty/lvgl)
 *   - FreeRTOS V11.1.0   (radio/src/thirdparty/FreeRTOS)
 *   - FatFs              (radio/src/thirdparty/FatFs)
 *   - STM32F4 HAL        (radio/src/thirdparty/STM32F4xx_HAL_Driver)
 *   - Lua                (radio/src/thirdparty/Lua)
 *
 * Like Dash on macOS, but local and project-version-aware.
 *
 * Features fuzzy search: missing separators ("lvbtn" → "lv_button"),
 * transpositions ("xTaksCreate" → "xTaskCreate"), and abbreviations
 * ("xQSend" → "xQueueSend") are handled automatically when exact
 * search yields no results, or on demand with fuzzy=true.
 *
 * Usage:
 *   LLM: call lib_docs(query, library?, fuzzy?, maxResults?, contextLines?)
 *   Interactive: /docs <query> [--lib lvgl|freertos|...] [--fuzzy]
 *               /docs --list-libs
 */

import { execSync } from "node:child_process";
import { existsSync } from "node:fs";
import { mkdtemp, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, relative } from "node:path";
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import {
  DEFAULT_MAX_BYTES,
  DEFAULT_MAX_LINES,
  formatSize,
  truncateHead,
  withFileMutationQueue,
} from "@earendil-works/pi-coding-agent";
import { Text } from "@earendil-works/pi-tui";
import { Type } from "typebox";

// ──────────────────────────────────────────────
// Library definitions
// ──────────────────────────────────────────────

interface LibraryDef {
  label: string;
  version: string;
  /** Paths relative to project root searched by rg. May include file-glob suffixes. */
  searchPaths: string[];
  /** Docs-only paths (RST, MD, etc.) — used for fuzzy search to reduce noise.
   *  When absent, searchPaths is used for both exact and fuzzy. */
  docsPaths?: string[];
  /** Brief description shown in tool output. */
  description: string;
}

const LIBRARIES: Record<string, LibraryDef> = {
  lvgl: {
    label: "LVGL",
    version: "9.5.0",
    searchPaths: [
      "radio/src/thirdparty/lvgl/docs/src",
      "radio/src/thirdparty/lvgl/src",
    ],
    docsPaths: ["radio/src/thirdparty/lvgl/docs/src"],
    description: "Full-featured embedded graphics library v9.5.0 (onliner10/edge16 fork)",
  },
  freertos: {
    label: "FreeRTOS Kernel",
    version: "V11.1.0",
    searchPaths: [
      "radio/src/thirdparty/FreeRTOS/include",
      "radio/src/thirdparty/FreeRTOS/tasks.c",
      "radio/src/thirdparty/FreeRTOS/queue.c",
      "radio/src/thirdparty/FreeRTOS/timers.c",
      "radio/src/thirdparty/FreeRTOS/event_groups.c",
      "radio/src/thirdparty/FreeRTOS/stream_buffer.c",
    ],
    description: "Real-time kernel for microcontrollers V11.1.0",
  },
  fatfs: {
    label: "FatFs",
    version: "(included in EdgeTX)",
    searchPaths: ["radio/src/thirdparty/FatFs"],
    description: "FAT file system module — ff.c, ff.h, diskio.h",
  },
  stm32_hal: {
    label: "STM32F4 HAL",
    version: "(included in EdgeTX)",
    searchPaths: [
      "radio/src/thirdparty/STM32F4xx_HAL_Driver/Inc",
      "radio/src/thirdparty/STM32F4xx_HAL_Driver/Src",
    ],
    description: "STM32F4 Hardware Abstraction Layer drivers",
  },
  lua: {
    label: "Lua",
    version: "(included in EdgeTX)",
    searchPaths: ["radio/src/thirdparty/Lua/src"],
    description: "Lua 5.x scripting engine",
  },
};

const LIBRARY_NAMES = Object.keys(LIBRARIES);

type LibraryName = (typeof LIBRARY_NAMES)[number];

// ──────────────────────────────────────────────
// Tool schema
// ──────────────────────────────────────────────

const LibDocsParams = Type.Object({
  query: Type.String({
    description:
      "Search term — function name, macro, type, or concept to look up. Supports regex patterns. Fuzzy matching is automatic when exact search yields no results.",
  }),
  library: Type.Optional(
    Type.String({
      description: `Library to search. One of: ${LIBRARY_NAMES.join(", ")}. Omit to search all.`,
    }),
  ),
  fuzzy: Type.Optional(
    Type.Boolean({
      description:
        "Force fuzzy matching: matches characters in order with anything in between. " +
        'Useful when you know the name but not exact spelling. "lvbtn" → matches "lv_button". '
        + "Automatically enabled when exact search finds nothing.",
      default: false,
    }),
  ),
  maxResults: Type.Optional(
    Type.Number({
      description: "Maximum context lines per matched file (default: 5).",
      default: 5,
    }),
  ),
  contextLines: Type.Optional(
    Type.Number({
      description: "Lines of context around each match (default: 2).",
      default: 2,
    }),
  ),
});

// ──────────────────────────────────────────────
// Extension
// ──────────────────────────────────────────────

export default function libDocsExtension(pi: ExtensionAPI) {
  pi.registerTool({
    name: "lib_docs",
    label: "Library Docs",
    description: `Search local documentation for embedded libraries used in the Edge16 project. Supports: ${LIBRARY_NAMES.join(", ")}. Searches RST docs, header-comment API docs, and source at the exact vendored versions. Truncated at ${DEFAULT_MAX_LINES} lines or ${formatSize(DEFAULT_MAX_BYTES)}.`,
    promptSnippet: "Search local library documentation (LVGL, FreeRTOS, FatFs, STM32 HAL, Lua)",
    promptGuidelines: [
      "Use lib_docs when you need API reference, widget docs, or RTOS concept details for the exact library versions used in this project — before guessing or writing code that depends on undocumented API behavior.",
      "Use lib_docs with library='lvgl' for LVGL widget docs, styles, events, animations, drawing.",
      "Use lib_docs with library='freertos' for FreeRTOS task/queue/timer/semaphore API details from header comments.",
    ],
    parameters: LibDocsParams,

    async execute(_toolCallId, params, _signal, _onUpdate, ctx) {
      const { query, library, fuzzy, maxResults = 5, contextLines = 2 } = params;

      // ── Detect project root ──────────────────────────────
      const root = findProjectRoot(ctx.cwd);
      if (!root) {
        return {
          content: [
            {
              type: "text",
              text: "Could not find Edge16 project root. Make sure you're working inside an Edge16 checkout with `radio/src/thirdparty` submodules checked out.\n\nHint: run `git submodule update --init --recursive` in the project root.",
            },
          ],
          details: { error: "project_root_not_found" },
        };
      }

      // ── Resolve libraries to search ──────────────────────
      const libsToSearch: LibraryName[] = library
        ? ([library] as LibraryName[]).filter((l) => LIBRARY_NAMES.includes(l))
        : ([...LIBRARY_NAMES] as LibraryName[]);

      if (libsToSearch.length === 0) {
        return {
          content: [
            {
              type: "text",
              text: `Unknown library "${library}". Choose from: ${LIBRARY_NAMES.join(", ")}`,
            },
          ],
          details: { error: "unknown_library", validLibraries: LIBRARY_NAMES },
        };
      }

      // ── Resolve search paths ─────────────────────────────
      const searchPaths = resolveSearchPaths(root, libsToSearch);
      if (searchPaths.length === 0) {
        return {
          content: [
            {
              type: "text",
              text: "No library documentation paths found. Ensure submodules are checked out:\n\n  git submodule update --init --recursive",
            },
          ],
          details: { error: "submodules_not_checked_out" },
        };
      }

      // ── Determine search mode ────────────────────────────
      // Three strategies in order of preference:
      //
      // 1. EXACT: case-insensitive substring via rg (fast, zero false positives)
      // 2. REGEX FUZZY: .*? between each cleaned-query character via rg
      //    (handles missing separators like "lvbtn"→"lv_button",
      //     abbreviations like "xQSend"→"xQueueSend", and most typos)
      // 3. FZF FUZZY: fzf -f with a broad anchor candidate set
      //    (handles transpositions like "xTaksCreate"→"xTaskCreate")

      const wantFuzzy = fuzzy === true;

      // Try exact/literal search first (unless fuzzy is forced)
      let usedFuzzy = false;
      let output: string | null = null;

      if (!wantFuzzy) {
        output = runRg(ctx.cwd, query, contextLines, searchPaths);
      }

      // If exact returned nothing (or fuzzy forced), try fuzzy
      const needsFuzzy = wantFuzzy || output === null || output.trim().length === 0;

      if (needsFuzzy) {
        // Primary fuzzy: .*? regex with no context (docs are prose, context isn't needed)
        const regexOut = runRegexFuzzySearch(ctx.cwd, query, 0, searchPaths);
        if (regexOut !== null && regexOut.trim().length > 0) {
          output = regexOut;
          usedFuzzy = true;
        } else {
          // Fallback fuzzy: fzf -f on broad anchor candidates
          const fzfOut = runFzfSearch(ctx.cwd, query, 0, searchPaths);
          if (fzfOut !== null && fzfOut.trim().length > 0) {
            output = fzfOut;
            usedFuzzy = true;
          }
        }
      }

      // If still nothing, give up
      if (!output || !output.trim()) {
        const mode = usedFuzzy ? "fuzzy" : "exact";
        return {
          content: [
            {
              type: "text",
              text: buildNoResultsMessage(query, libsToSearch, library, mode),
            },
          ],
          details: { query, libraries: libsToSearch, matchCount: 0, fuzzy: usedFuzzy },
        };
      }

      // ── Truncate ─────────────────────────────────────────
      const truncation = truncateHead(output, {
        maxLines: DEFAULT_MAX_LINES,
        maxBytes: DEFAULT_MAX_BYTES,
      });

      const formatted = formatResults(
        truncation.content,
        root,
        libsToSearch,
        maxResults,
        usedFuzzy,
      );
      const matchCount = output.split("\n").filter((l) => l.trim()).length;

      const details: Record<string, unknown> = {
        query,
        libraries: libsToSearch,
        matchCount,
        fuzzy: usedFuzzy,
      };

      let resultText = formatted;

      if (truncation.truncated) {
        const tempDir = await mkdtemp(join(tmpdir(), "pi-lib-docs-"));
        const tempFile = join(tempDir, "output.txt");
        await withFileMutationQueue(tempFile, async () => {
          await writeFile(tempFile, output, "utf8");
        });

        details.truncation = truncation;
        details.fullOutputPath = tempFile;

        const truncatedLines = truncation.totalLines - truncation.outputLines;
        const truncatedBytes = truncation.totalBytes - truncation.outputBytes;

        resultText += `\n\n[Output truncated: showing ${truncation.outputLines} of ${truncation.totalLines} lines`;
        resultText += ` (${formatSize(truncation.outputBytes)} of ${formatSize(truncation.totalBytes)}).`;
        resultText += ` ${truncatedLines} lines (${formatSize(truncatedBytes)}) omitted.`;
        resultText += ` Full output saved to: ${tempFile}]`;
      }

      return {
        content: [{ type: "text", text: resultText }],
        details,
      };
    },

    // ── Custom rendering ──────────────────────────────────
    renderCall(args, theme, _context) {
      let text = theme.fg("toolTitle", theme.bold("lib_docs ")) + " ";
      if (args.fuzzy) {
        text += theme.fg("warning", "[fuzzy] ");
      }
      text += theme.fg("accent", `"${args.query}"`);
      if (args.library) {
        text += theme.fg("muted", ` in ${args.library}`);
      } else {
        text += theme.fg("dim", " (all libs)");
      }
      return new Text(text, 0, 0);
    },

    renderResult(result, { expanded, isPartial }, theme, _context) {
      const details = result.details as Record<string, unknown> | undefined;

      if (isPartial) {
        return new Text(theme.fg("warning", "Searching library docs..."), 0, 0);
      }

      if (!details || details.matchCount === 0) {
        return new Text(theme.fg("dim", "No docs found"), 0, 0);
      }

      const error = details.error;
      if (error) {
        return new Text(theme.fg("error", `Docs search error: ${error}`), 0, 0);
      }

      let text = theme.fg("success", `${details.matchCount} matches`);

      if (details.fuzzy) {
        text += theme.fg("warning", " fuzzy");
      }

      if ((details.truncation as any)?.truncated) {
        text += theme.fg("warning", " (truncated)");
      }

      if (expanded && result.content[0]?.type === "text") {
        const lines = result.content[0].text.split("\n").slice(0, 25);
        for (const line of lines) {
          text += `\n${theme.fg("dim", line)}`;
        }
        if (result.content[0].text.split("\n").length > 25) {
          text += `\n${theme.fg("muted", "... (use read to see full results)")}`;
        }
      }

      return new Text(text, 0, 0);
    },
  });

  // ── Convenience /docs command ───────────────────────────
  pi.registerCommand("docs", {
    description: `Search library docs: /docs <query> [--lib ${LIBRARY_NAMES.join("|")}] [--fuzzy], or /docs --list-libs`,
    handler: async (args, ctx) => {
      const trimmed = args.trim();

      if (!trimmed) {
        ctx.ui.notify(
          `Usage: /docs <query> [--lib ${LIBRARY_NAMES.join("|")}] [--fuzzy]\n  /docs --list-libs  — show available libraries`,
          "info",
        );
        return;
      }

      // --list-libs flag
      if (trimmed === "--list-libs" || trimmed === "-l") {
        const lines = LIBRARY_NAMES.map((name) => {
          const def = LIBRARIES[name];
          const checked = def.searchPaths.some((sp) =>
            findProjectRoot(ctx.cwd)
              ? existsSync(join(findProjectRoot(ctx.cwd)!, sp))
              : false,
          );
          const status = checked ? "✓" : "✗ (submodule?)";
          return `  ${name.padEnd(14)} ${def.label.padEnd(18)} ${def.version.padEnd(24)} ${status}`;
        });
        const header =
          "Available libraries (use --lib <name> to narrow search):\n\n" +
          `  ${"Name".padEnd(14)} ${"Label".padEnd(18)} ${"Version".padEnd(24)} Status\n` +
          `  ${"─".repeat(14)} ${"─".repeat(18)} ${"─".repeat(24)} ${"─".repeat(12)}`;
        ctx.ui.notify(`${header}\n${lines.join("\n")}`, "info");
        return;
      }

      // Parse flags
      let query = trimmed;
      let library: string | undefined;
      let forceFuzzy = false;

      const libMatch = query.match(/--lib\s+(\S+)/);
      if (libMatch) {
        library = libMatch[1];
        query = query.replace(/--lib\s+\S+\s*/, "").trim();
      }

      if (/--fuzzy\b/.test(query)) {
        forceFuzzy = true;
        query = query.replace(/--fuzzy\s*/, "").trim();
      }

      if (!query) {
        ctx.ui.notify("Provide a search query, or --list-libs to list available libraries.", "warning");
        return;
      }

      // Run the search directly and show results inline
      const root = findProjectRoot(ctx.cwd);
      if (!root) {
        ctx.ui.notify("Edge16 project root not found. Are you inside an Edge16 checkout?", "error");
        return;
      }

      const libsToSearch: LibraryName[] = library
        ? ([library] as LibraryName[]).filter((l) => LIBRARY_NAMES.includes(l))
        : ([...LIBRARY_NAMES] as LibraryName[]);

      if (libsToSearch.length === 0) {
        ctx.ui.notify(
          `Unknown library "${library}". Choose from: ${LIBRARY_NAMES.join(", ")}`,
          "error",
        );
        return;
      }

      const searchPaths = resolveSearchPaths(root, libsToSearch);
      if (searchPaths.length === 0) {
        ctx.ui.notify("Submodules not checked out. Run: git submodule update --init --recursive", "error");
        return;
      }

      // Try exact, then fuzzy if needed
      let usedFuzzy = false;
      let output: string | null = null;

      if (!forceFuzzy) {
        output = runRg(ctx.cwd, query, 1, searchPaths);
      }

      const needsFuzzy = forceFuzzy || output === null || output.trim().length === 0;

      if (needsFuzzy) {
        // Primary fuzzy: .*? regex with no context
        const regexOut = runRegexFuzzySearch(ctx.cwd, query, 0, searchPaths);
        if (regexOut !== null && regexOut.trim().length > 0) {
          output = regexOut;
          usedFuzzy = true;
        } else {
          // Fallback: fzf on paths + content
          const fzfOut = runFzfSearch(ctx.cwd, query, 0, searchPaths);
          if (fzfOut !== null && fzfOut.trim().length > 0) {
            output = fzfOut;
            usedFuzzy = true;
          }
        }
      }

      if (!output || !output.trim()) {
        const mode = usedFuzzy ? "fuzzy" : "exact";
        ctx.ui.notify(`No results for "${query}" in ${library || "all libraries"} (${mode}).`, "warning");
        return;
      }

      // Truncate and show
      const truncation = truncateHead(output, {
        maxLines: 80,
        maxBytes: DEFAULT_MAX_BYTES,
      });

      const matchCount = output.split("\n").filter((l) => l.trim()).length;
      const formatted = formatResults(truncation.content, root, libsToSearch, 5, usedFuzzy);

      let display = formatted;
      if (truncation.truncated) {
        display += `\n\n(Showing ${truncation.outputLines} of ${truncation.totalLines} lines; use lib_docs tool for full results)`;
      }

      const modeLabel = usedFuzzy ? " 🔍 fuzzy" : "";
      ctx.ui.notify(`📖${modeLabel} ${matchCount} matches for "${query}":\n\n${display}`, "info");
    },
  });
}

// ──────────────────────────────────────────────
// Helpers — project detection, fuzzy search, rg runner, formatting
// ──────────────────────────────────────────────

/**
 * Walk up from `start` looking for a directory that contains
 * `radio/src/thirdparty` with LVGL or FreeRTOS submodules.
 * Returns the project root path, or null.
 */
function findProjectRoot(start: string): string | null {
  let dir = start;
  for (let i = 0; i < 10; i++) {
    if (
      existsSync(join(dir, "radio", "src", "thirdparty", "lvgl")) ||
      existsSync(join(dir, "radio", "src", "thirdparty", "FreeRTOS"))
    ) {
      return dir;
    }
    const parent = join(dir, "..");
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

/**
 * Build a fuzzy regex pattern from a query string (PRIMARY fuzzy strategy).
 *
 * Strips non-alphanumeric separators, then inserts `.*?` between each character.
 * This creates a regex that matches the query characters in order with arbitrary
 * gaps, handling missing separators, most typos, and abbreviations.
 *
 * Examples:
 *   "lvbtn"    → l.*?v.*?b.*?t.*?n       matches "lv_button", "lv_btn", "lv_bar"
 *   "xQSend"   → x.*?Q.*?S.*?e.*?n.*?d   matches "xQueueSend", "xQueueSendFromISR"
 *   "HAL_GPIO" → H.*?A.*?L.*?G.*?P.*?I.*?O  matches "HAL_GPIO_WritePin", etc.
 */
function buildFuzzyPattern(query: string): string | null {
  // Strip non-alphanumeric (separators, spaces, etc.) so "lvbtn" can match "lv_button"
  const cleaned = query.replace(/[^a-zA-Z0-9]/g, "");
  if (cleaned.length < 2) return null;
  // Join with .*? and prepend \b to anchor at word boundary — reduces false positives
  return "\\b" + cleaned.split("").join(".*?");
}

/**
 * Check if fzf (the fuzzy-finder CLI) is available on the system.
 */
function hasFzf(): boolean {
  try {
    execSync("fzf --version", {
      stdio: ["ignore", "ignore", "ignore"],
      timeout: 2000,
    });
    return true;
  } catch {
    return false;
  }
}

/**
 * Build a broad anchor pattern from the query for the initial rg pass.
 * Unused in the current implementation (fzf phase 3 builds the anchor inline).
 * Kept for potential reuse.
 */
function buildBroadAnchor(query: string): string {
  const cleaned = query.replace(/[^a-zA-Z0-9]/g, "");
  if (cleaned.length <= 2) return cleaned || query[0] || "";
  const first2 = cleaned.substring(0, 2);
  const first4 = cleaned.substring(0, 4);
  if (first4.length >= 4 && /[A-Z]/.test(first4[1]) && /[a-z]/.test(first4[2])) {
    return first4;
  }
  return first2;
}

/**
 * Run fuzzy search using fzf -f (filter mode) — FALLBACK fuzzy strategy.
 *
 * Strategy (two-phase):
 * 1. Find candidate files via `find | fzf -f` (fzf excels at filename matching)
 * 2. Search within matched files using the query as a simple rg pattern
 *
 * Phase 1 is fast and targeted for docs (filenames contain widget names).
 * Phase 2 searches the shortlisted files.
 *
 * If Phase 1 yields no files, Phase 3 uses rg with a broad content anchor
 * piped through fzf -f as a last resort (handles transpositions).
 */
function runFzfSearch(
  cwd: string,
  query: string,
  _contextLines: number,
  searchPaths: string[],
): string | null {
  if (!hasFzf() || searchPaths.length === 0) return null;

  // Docs-first: filter to doc-like paths, fall back to all paths
  const fzfPaths = searchPaths.filter(
    (p) => p.includes("/docs/") || p.endsWith(".rst") || p.endsWith(".md"),
  );
  const effectivePaths = fzfPaths.length > 0 ? fzfPaths : searchPaths;

  // Phase 1: Find candidate files via path fuzzy matching.
  // fzf -f on filenames is fast and targeted for docs where
  // file paths contain widget/function names.
  const fileCmd =
    `find ${effectivePaths.join(" ")} -type f 2>/dev/null` +
    ` | fzf -f "${query.replace(/"/g, '\\"')}" --no-sort 2>/dev/null | head -15`;

  let files: string[] = [];
  try {
    const fileOutput = execSync(fileCmd, {
      cwd,
      encoding: "utf-8",
      maxBuffer: 10 * 1024 * 1024,
      timeout: 10_000,
      shell: "/bin/bash",
    });
    files = fileOutput
      .split("\n")
      .map((f) => f.trim())
      .filter((f) => f.length > 0);
  } catch {
    // fzf -f exits with 1 when no matches
  }

  if (files.length > 0) {
    // Phase 2: Search within the matched files using the regex fuzzy pattern
    const pattern = buildFuzzyPattern(query);
    if (pattern) {
      return runRg(cwd, pattern, 1, files);
    }
    // Fallback: use query directly
    return runRg(cwd, query, 1, files);
  }

  // Phase 3: Content-based fallback — use first 2 cleaned chars as
  // a broad rg anchor, pipe through fzf -f.
  // This handles transpositions where filenames don't help.
  const cleaned = query.replace(/[^a-zA-Z0-9]/g, "");
  const anchor = cleaned.length >= 2 ? cleaned.substring(0, 2) : (cleaned[0] || "");
  if (!anchor) return null;

  const contentCmd =
    `rg --line-number --color=never -i -s --no-ignore-vcs --max-columns 200` +
    ` "${anchor.replace(/"/g, '\\"')}" ${effectivePaths.join(" ")} 2>/dev/null` +
    ` | fzf -f "${query.replace(/"/g, '\\"')}" --no-sort 2>/dev/null | head -50`;

  try {
    const output = execSync(contentCmd, {
      cwd,
      encoding: "utf-8",
      maxBuffer: 100 * 1024 * 1024,
      timeout: 60_000,
      shell: "/bin/bash",
    });
    return output || null;
  } catch (err: any) {
    if (err.status === 1) return null;
    throw err;
  }
}

/**
 * Regex-based fuzzy search (PRIMARY fuzzy strategy).
 * Uses rg with .*? between each character of the cleaned query.
 *
 * To keep output manageable:
 * 1. Docs-only paths first (containing "/docs/" or .rst/.md)
 * 2. Best-match-per-file (shortest line per file — most specific API ref)
 * 3. Hard-capped at MAX_FUZZY_LINES total
 * 4. Sorted by specificity (shortest lines first = tightest API matches)
 */
const MAX_FUZZY_LINES = 30;

function runRegexFuzzySearch(
  cwd: string,
  query: string,
  _contextLines: number,
  searchPaths: string[],
): string | null {
  const pattern = buildFuzzyPattern(query);
  if (!pattern) return null;

  // Docs-first: filter to paths that look like documentation
  const docsPaths = searchPaths.filter(
    (p) => p.includes("/docs/") || p.endsWith(".rst") || p.endsWith(".md"),
  );
  const effectivePaths = docsPaths.length > 0 ? docsPaths : searchPaths;

  const raw = runRg(cwd, pattern, 0, effectivePaths);
  if (!raw || !raw.trim()) return null;

  // Group by file, pick best (shortest) matching line per file
  const lines = raw.split("\n").filter((l) => l.trim().length > 0 && l.includes(":"));
  const byFile = new Map<string, string>();

  for (const line of lines) {
    const idx = line.indexOf(":");
    if (idx <= 0) continue;
    const file = line.substring(0, idx);
    // Skip vendor/packaged files (jquery, fontawesome, bundled CSS, build scripts)
    if (file.includes("/_static/") || file.includes("/_ext/") || file.includes("/node_modules/")) continue;
    const existing = byFile.get(file);
    // Shorter line = more specific API reference, less prose noise
    if (!existing || line.length < existing.length) {
      byFile.set(file, line);
    }
  }

  // Sort: shortest matching lines first (tightest API name matches)
  const sorted = Array.from(byFile.entries())
    .sort((a, b) => a[1].length - b[1].length)
    .slice(0, MAX_FUZZY_LINES);

  if (sorted.length === 0) return null;

  return sorted.map(([, line]) => line).join("\n");
}

/**
 * Collect absolute search paths for the given libraries, filtering to
 * only those that exist on disk.
 */
function resolveSearchPaths(root: string, libs: LibraryName[]): string[] {
  const paths: string[] = [];
  for (const libName of libs) {
    const def = LIBRARIES[libName];
    for (const sp of def.searchPaths) {
      const ap = join(root, sp);
      if (existsSync(ap)) {
        paths.push(ap);
      }
    }
  }
  return paths;
}

/**
 * Run ripgrep with the given pattern and return output (or null on no match).
 */
function runRg(
  cwd: string,
  pattern: string,
  contextLines: number,
  searchPaths: string[],
): string | null {
  if (searchPaths.length === 0) return null;

  const parts: string[] = [
    "rg",
    "--line-number",
    "--color=never",
    "-i",
    "-s",
  ];

  if (contextLines > 0) {
    parts.push("-C", String(contextLines));
  }

  parts.push("--no-ignore-vcs");
  parts.push("--max-columns", "200");
  parts.push(`"${pattern.replace(/"/g, '\\"')}"`);

  const cmd = [...parts, ...searchPaths].join(" ");

  try {
    return execSync(cmd, {
      cwd,
      encoding: "utf-8",
      maxBuffer: 100 * 1024 * 1024,
      timeout: 30_000,
    });
  } catch (err: any) {
    if (err.status === 1) return null; // no matches
    // Re-throw unexpected errors (rg not found, permissions, etc.)
    throw err;
  }
}

/**
 * Build a no-results message with tailored tips.
 */
function buildNoResultsMessage(
  query: string,
  libs: LibraryName[],
  requestedLibrary?: string,
  mode?: string,
): string {
  const libsList = requestedLibrary ? `"${requestedLibrary}"` : "all libraries";
  const modeNote = mode ? ` (${mode} search)` : "";
  return [
    `No results for "${query}" in ${libsList}${modeNote}.`,
    "",
    "Tips:",
    "  - Try a shorter or different term (fuzzy search auto-expands missing separators).",
    "  - Pass fuzzy=true or use --fuzzy to force fuzzy matching.",
    "  - Check submodules: git submodule update --init --recursive",
    "  - LVGL: try widget names (\"button\", \"arc\", \"label\"), function prefixes",
    '    ("lv_obj_set_style", "lv_anim_start"), or concept terms ("style", "event").',
    "  - FreeRTOS: try function prefixes (\"xTaskCreate\", \"xQueueSend\"), types",
    '    ("TaskHandle_t", "SemaphoreHandle_t"), or concepts ("scheduler", "priority").',
    "  - Use /docs --list-libs to confirm submodule availability.",
  ].join("\n");
}

/**
 * Format rg results into per-library, per-file grouped output,
 * limiting context per file to maxResults lines.
 */
function formatResults(
  raw: string,
  root: string,
  libs: LibraryName[],
  maxResults: number,
  wasFuzzy?: boolean,
): string {
  const header: string[] = [];
  header.push("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  const fuzzyTag = wasFuzzy ? " 🔍 FUZZY" : "";
  header.push(`   Library Documentation Search Results${fuzzyTag}`);
  header.push("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  header.push("");

  const libsSearched = libs.map((l) => {
    const def = LIBRARIES[l];
    return `  ${def.label} ${def.version}`;
  });
  header.push(`Libraries searched:\n${libsSearched.join("\n")}`);
  if (wasFuzzy) {
    header.push("");
    header.push("🔍 Fuzzy mode: characters matched in order with gaps allowed.");
    header.push("   Try narrowing with a more specific query or library filter.");
  }
  header.push("");

  // Split raw rg output into file groups
  interface FileGroup {
    libName: string;
    relPath: string;
    lines: string[];
  }

  const fileGroups: FileGroup[] = [];
  let currentGroup: FileGroup | null = null;

  for (const line of raw.split("\n")) {
    // Detect a new file match by checking if the line starts with
    // a path (absolute or relative) followed by colon and line number.
    // rg format: /abs/path:line:content  or  relative/path:line:content
    const fileMatch = line.match(/^(.+?):(\d+):/);
    if (fileMatch) {
      const absPath = fileMatch[1];
      // Verify it looks like a project path
      let relPath: string;
      if (absPath.startsWith(root)) {
        relPath = relative(root, absPath);
      } else if (absPath.startsWith("/")) {
        // Absolute path outside project — not a project file, skip group detection
        relPath = absPath;
      } else {
        relPath = absPath; // already relative
      }

      const lib = findLibraryForPath(relPath, libs);
      const libLabel = lib ? LIBRARIES[lib].label : "Unknown";

      // Start new group if different file
      if (!currentGroup || currentGroup.relPath !== relPath) {
        if (currentGroup && currentGroup.lines.length > 0) {
          fileGroups.push(currentGroup);
        }
        currentGroup = { libName: libLabel, relPath, lines: [] };
      }

      if (currentGroup.lines.length < maxResults) {
        currentGroup.lines.push(line);
      }
      continue;
    }

    // Context/separator lines
    if (currentGroup && currentGroup.lines.length < maxResults) {
      currentGroup.lines.push(line);
    }
  }

  if (currentGroup && currentGroup.lines.length > 0) {
    fileGroups.push(currentGroup);
  }

  if (fileGroups.length === 0) {
    header.push("(raw results available, grouped display unavailable)");
    return header.join("\n") + "\n\n" + raw;
  }

  const body: string[] = [];
  for (const group of fileGroups) {
    body.push("");
    body.push(`  ── ${group.libName}: ${group.relPath}`);
    body.push("");
    for (const matchLine of group.lines) {
      if (matchLine.startsWith("--")) {
        body.push(`    ${matchLine}`);
        continue;
      }
      const m = matchLine.match(/^(.+?):(\d+):(.+)/);
      if (m) {
        const lineNum = m[2];
        const content = m[3];
        body.push(`    L${lineNum}: ${content}`);
      } else {
        body.push(`    ${matchLine}`);
      }
    }
    if (group.lines.length >= maxResults) {
      body.push(`    (... more matches in this file — narrow your query or use lib_docs tool)`);
    }
  }

  return header.join("\n") + "\n" + body.join("\n");
}

function findLibraryForPath(
  relPath: string,
  activeLibs: LibraryName[],
): LibraryName | null {
  for (const lib of activeLibs) {
    const def = LIBRARIES[lib];
    for (const sp of def.searchPaths) {
      if (relPath.startsWith(sp)) return lib;
    }
  }
  return null;
}
