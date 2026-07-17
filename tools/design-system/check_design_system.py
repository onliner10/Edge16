#!/usr/bin/env python3
"""Design-system guard for the EdgeTX color LCD GUI (see DESIGN_SYSTEM.md).

Raw spacing/position styling is only allowed inside the design-system layer
(`radio/src/gui/colorlcd/libui/`). Everywhere else, screens must compose the
`ds::` components, which own all spacing, sizing and touch-target floors.

This guard scans screen code for forbidden raw-styling patterns and enforces
a per-file BASELINE RATCHET over the ~100 legacy screens:

  * a file may never have MORE violations than its recorded baseline;
  * files not in the baseline must be clean;
  * when a file improves, the run reports that the ratchet can tighten —
    re-record with --update-baseline and commit the lowered baseline
    together with the cleanup, locking the improvement in.

Line-level escape hatch: append `// ds-allow: <reason>` to a line to suppress
it. Suppressions are themselves counted and ratcheted, so they cannot grow
silently.

Exit codes: 0 = clean / within baseline, 1 = new violations, 2 = usage error.

Intended wiring: a gtest (TEST(DesignSystem, GuardClean)) shells this script,
and CI runs it directly, so violations fail the build.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOT = REPO_ROOT / "radio" / "src" / "gui" / "colorlcd"
BASELINE_PATH = Path(__file__).resolve().parent / "ds_baseline.json"

# The ONLY place (with etx_lv_theme.cpp, which lives inside it) allowed to
# use raw spacing/position styling.
ALLOWLIST_PREFIXES = (
    "libui/",
)

SOURCE_SUFFIXES = {".cpp", ".h"}

FORBIDDEN = [
    ("raw-pad", re.compile(r"lv_obj_set_style_pad\w*\s*\(")),
    ("raw-margin", re.compile(r"lv_obj_set_style_margin\w*\s*\(")),
    ("raw-pos", re.compile(r"lv_obj_set_pos\s*\(")),
    ("window-pad", re.compile(r"\bpad(All|Top|Bottom|Left|Right|Row|Column)\s*\(")),
    ("pad-token", re.compile(
        r"\bPAD_(ZERO|TINY|SMALL|MEDIUM|LARGE|THREE|SCROLL|TABLE_V|TABLE_H|OUTLINE|BORDER)\b")),
    ("adhoc-scaled-const", re.compile(r"\bLAYOUT_(VAL|SIZE|ORIENTATION)\w*\s*\(")),
    ("layout-scale", re.compile(r"\bLAYOUT_SCALE\s*\(")),
]

ALLOW_RE = re.compile(r"//\s*ds-allow:\s*\S")
COMMENT_RE = re.compile(r"^\s*(//|\*|/\*)")


def scan_file(path: Path) -> tuple[list[tuple[int, str, str]], int]:
    """Return ([(lineno, rule, line)], suppression_count)."""
    findings: list[tuple[int, str, str]] = []
    suppressions = 0
    try:
        text = path.read_text(errors="replace")
    except OSError as e:
        print(f"warning: cannot read {path}: {e}", file=sys.stderr)
        return findings, suppressions
    for lineno, line in enumerate(text.splitlines(), 1):
        if COMMENT_RE.match(line):
            continue
        hit = next((rule for rule, rx in FORBIDDEN if rx.search(line)), None)
        if hit is None:
            continue
        if ALLOW_RE.search(line):
            suppressions += 1
            continue
        findings.append((lineno, hit, line.strip()))
    return findings, suppressions


def scan_tree() -> tuple[dict[str, list[tuple[int, str, str]]], dict[str, int]]:
    per_file: dict[str, list[tuple[int, str, str]]] = {}
    suppr: dict[str, int] = {}
    for path in sorted(SCAN_ROOT.rglob("*")):
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        rel = path.relative_to(SCAN_ROOT).as_posix()
        if any(rel.startswith(p) for p in ALLOWLIST_PREFIXES):
            continue
        findings, suppressions = scan_file(path)
        if findings:
            per_file[rel] = findings
        if suppressions:
            suppr[rel] = suppressions
    return per_file, suppr


def load_baseline() -> dict:
    if not BASELINE_PATH.exists():
        return {"files": {}, "suppressions": {}, "total": 0}
    return json.loads(BASELINE_PATH.read_text())


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--update-baseline", action="store_true",
                    help="record current violation counts as the new baseline")
    ap.add_argument("--verbose", action="store_true",
                    help="list every violating line, not just new ones")
    args = ap.parse_args()

    if not SCAN_ROOT.is_dir():
        print(f"error: scan root not found: {SCAN_ROOT}", file=sys.stderr)
        return 2

    per_file, suppr = scan_tree()
    counts = {rel: len(f) for rel, f in per_file.items()}
    total = sum(counts.values())

    if args.update_baseline:
        baseline = {
            "comment": "Design-system guard ratchet. Counts may only go DOWN. "
                       "See DESIGN_SYSTEM.md.",
            "files": counts,
            "suppressions": suppr,
            "total": total,
        }
        BASELINE_PATH.write_text(json.dumps(baseline, indent=2, sort_keys=True) + "\n")
        print(f"baseline updated: {len(counts)} files, {total} violations, "
              f"{sum(suppr.values())} suppressions -> {BASELINE_PATH}")
        return 0

    baseline = load_baseline()
    base_files: dict[str, int] = baseline.get("files", {})
    base_suppr: dict[str, int] = baseline.get("suppressions", {})

    failed = False
    improved: list[str] = []

    for rel, n in sorted(counts.items()):
        allowed = base_files.get(rel, 0)
        if n > allowed:
            failed = True
            print(f"FAIL {rel}: {n} raw-styling violations (baseline {allowed})")
            shown = 0
            for lineno, rule, line in per_file[rel]:
                print(f"    {rel}:{lineno}: [{rule}] {line}")
                shown += 1
                if not args.verbose and shown >= 10:
                    print("    ... (use --verbose for all)")
                    break
        elif n < allowed:
            improved.append(f"{rel}: {allowed} -> {n}")
        elif args.verbose:
            for lineno, rule, line in per_file[rel]:
                print(f"    {rel}:{lineno}: [{rule}] {line}")

    for rel, allowed in sorted(base_files.items()):
        if counts.get(rel, 0) == 0 and allowed > 0:
            improved.append(f"{rel}: {allowed} -> 0")

    total_suppr = sum(suppr.values())
    base_total_suppr = sum(base_suppr.values())
    if total_suppr > base_total_suppr:
        failed = True
        print(f"FAIL: ds-allow suppressions grew: {total_suppr} "
              f"(baseline {base_total_suppr})")
        for rel, n in sorted(suppr.items()):
            if n > base_suppr.get(rel, 0):
                print(f"    {rel}: {n} (baseline {base_suppr.get(rel, 0)})")

    if failed:
        print("\nDesign-system guard FAILED. Do not add raw spacing/position "
              "styling in screen code -- use the ds:: components "
              "(radio/src/gui/colorlcd/libui/ds_core.h). See DESIGN_SYSTEM.md.")
        return 1

    if improved:
        print("Ratchet can tighten (violations decreased):")
        for line in improved:
            print(f"    {line}")
        print("Run with --update-baseline and commit the lowered baseline.")

    print(f"design-system guard OK: {total} legacy violations across "
          f"{len(counts)} files (baseline total {baseline.get('total', 0)}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
