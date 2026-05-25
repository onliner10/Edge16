#!/usr/bin/env python3
"""Check Edge16 YAML schema release notes against generated schema drift."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

VERSION_RE = re.compile(r"^##\s+([0-9]+\.[0-9]+(?:\.[0-9]+)?)\s*$", re.M)
SECTION_RE = re.compile(r"^###\s+(\S.*)$", re.M)
VOLATILE_SCHEMA_KEYS = {
    "$id",
    "x-edge16-generated-at",
    "x-edge16-generated-from-commit",
}


@dataclass
class ReleaseEntry:
    version: str
    body: str

    def section_body(self, section: str) -> str:
        matches = list(SECTION_RE.finditer(self.body))
        for index, match in enumerate(matches):
            if match.group(1).strip().lower() != section.lower():
                continue
            start = match.end()
            end = matches[index + 1].start() if index + 1 < len(matches) else len(self.body)
            return self.body[start:end].strip()
        return ""


def latest_release_entry(path: Path) -> ReleaseEntry:
    text = path.read_text()
    match = VERSION_RE.search(text)
    if not match:
        raise SystemExit(f"No YAML schema release heading found in {path}; expected '## x.y'")
    next_match = VERSION_RE.search(text, match.end())
    end = next_match.start() if next_match else len(text)
    body = text[match.end():end].strip()
    if not body:
        raise SystemExit(f"YAML schema release {match.group(1)} has empty notes")
    return ReleaseEntry(match.group(1), body)


def load_json(path: Path) -> Any:
    return json.loads(path.read_text())


def normalize(value: Any) -> Any:
    if isinstance(value, dict):
        return {key: normalize(val) for key, val in sorted(value.items()) if key not in VOLATILE_SCHEMA_KEYS}
    if isinstance(value, list):
        return [normalize(item) for item in value]
    return value


def schema_files(root: Path) -> list[Path]:
    return sorted(path for path in root.glob("*/*.schema.json") if path.is_file())


def load_schema_set(root: Path) -> dict[str, Any]:
    return {str(path.relative_to(root)): normalize(load_json(path)) for path in schema_files(root)}


def manifest_version(root: Path) -> str | None:
    manifest = root / "manifest.json"
    if not manifest.exists():
        return None
    data = load_json(manifest)
    version = data.get("schemaReleaseVersion")
    return str(version) if version is not None else None


def path_join(path: str, key: str) -> str:
    return f"{path}.{key}" if path else key


def compare_breaking(old: Any, new: Any, path: str, findings: list[str]) -> None:
    if isinstance(old, dict) and isinstance(new, dict):
        old_type = old.get("type")
        new_type = new.get("type")
        if old_type is not None and new_type is not None and old_type != new_type:
            findings.append(f"{path}: type changed {old_type!r} -> {new_type!r}")

        for key in ("minimum", "exclusiveMinimum"):
            if key in old and key in new and isinstance(old[key], (int, float)) and isinstance(new[key], (int, float)) and new[key] > old[key]:
                findings.append(f"{path}: {key} tightened {old[key]!r} -> {new[key]!r}")
        for key in ("maximum", "exclusiveMaximum"):
            if key in old and key in new and isinstance(old[key], (int, float)) and isinstance(new[key], (int, float)) and new[key] < old[key]:
                findings.append(f"{path}: {key} tightened {old[key]!r} -> {new[key]!r}")
        for key in ("maxItems", "maxLength", "maxProperties"):
            if key in old and key in new and isinstance(old[key], int) and isinstance(new[key], int) and new[key] < old[key]:
                findings.append(f"{path}: {key} reduced {old[key]!r} -> {new[key]!r}")

        old_enum = old.get("enum")
        new_enum = new.get("enum")
        if isinstance(old_enum, list) and isinstance(new_enum, list):
            removed = sorted(set(old_enum) - set(new_enum))
            if removed:
                findings.append(f"{path}: enum values removed {removed!r}")

        old_required = old.get("required")
        new_required = new.get("required")
        if isinstance(old_required, list) and isinstance(new_required, list):
            added = sorted(set(new_required) - set(old_required))
            if added:
                findings.append(f"{path}: required properties added {added!r}")

        old_props = old.get("properties")
        new_props = new.get("properties")
        if isinstance(old_props, dict) and isinstance(new_props, dict):
            removed_props = sorted(set(old_props) - set(new_props))
            for prop in removed_props:
                findings.append(f"{path}: property removed {prop!r}")
            for prop in sorted(set(old_props) & set(new_props)):
                compare_breaking(old_props[prop], new_props[prop], path_join(path, f"properties.{prop}"), findings)

        old_defs = old.get("$defs")
        new_defs = new.get("$defs")
        if isinstance(old_defs, dict) and isinstance(new_defs, dict):
            for name in sorted(set(old_defs) - set(new_defs)):
                findings.append(f"{path}: definition removed {name!r}")
            for name in sorted(set(old_defs) & set(new_defs)):
                compare_breaking(old_defs[name], new_defs[name], path_join(path, f"$defs.{name}"), findings)

        for key in ("items", "additionalProperties"):
            if key in old and key in new:
                compare_breaking(old[key], new[key], path_join(path, key), findings)

        for key in ("anyOf", "oneOf", "allOf"):
            old_seq = old.get(key)
            new_seq = new.get(key)
            if isinstance(old_seq, list) and isinstance(new_seq, list):
                for index, (old_item, new_item) in enumerate(zip(old_seq, new_seq)):
                    compare_breaking(old_item, new_item, path_join(path, f"{key}[{index}]"), findings)
                if len(new_seq) < len(old_seq):
                    findings.append(f"{path}: {key} choices removed")
        return

    if type(old) is not type(new):
        findings.append(f"{path}: schema node kind changed {type(old).__name__} -> {type(new).__name__}")


def check_schema_drift(release_notes: Path, new_root: Path, previous_root: Path | None) -> int:
    entry = latest_release_entry(release_notes)
    new_version = manifest_version(new_root)
    if new_version != entry.version:
        print(f"Generated schema version {new_version!r} does not match latest release note {entry.version!r}", file=sys.stderr)
        return 1

    if previous_root is None or not previous_root.exists() or not (previous_root / "manifest.json").exists():
        print("No previous published schema found; skipping drift comparison")
        return 0

    old_version = manifest_version(previous_root)
    if old_version is None:
        print("Previous published schema has no schemaReleaseVersion; skipping drift comparison")
        return 0

    old_schemas = load_schema_set(previous_root)
    new_schemas = load_schema_set(new_root)
    changed = old_schemas != new_schemas
    if changed and old_version == new_version:
        print(f"Generated schemas changed but release note version stayed {new_version}", file=sys.stderr)
        return 1

    breaking: list[str] = []
    for relpath, old_schema in old_schemas.items():
        if relpath not in new_schemas:
            breaking.append(f"{relpath}: schema file removed")
            continue
        compare_breaking(old_schema, new_schemas[relpath], relpath, breaking)

    if breaking and (old_version == new_version or not entry.section_body("Breaking")):
        print("Breaking YAML schema changes require version bump and non-empty '### Breaking' release notes", file=sys.stderr)
        for finding in breaking[:50]:
            print(f"- {finding}", file=sys.stderr)
        if len(breaking) > 50:
            print(f"... {len(breaking) - 50} more", file=sys.stderr)
        return 1

    print(f"YAML schema release check ok: previous={old_version or 'none'} current={new_version} changed={changed} breaking={bool(breaking)}")
    return 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-notes", type=Path, required=True)
    parser.add_argument("--print-latest-release-version", action="store_true")
    parser.add_argument("--new-root", type=Path)
    parser.add_argument("--previous-root", type=Path)
    args = parser.parse_args()

    if args.print_latest_release_version:
        print(latest_release_entry(args.release_notes).version)
        return

    if args.new_root is None:
        parser.error("--new-root is required unless --print-latest-release-version is used")
    raise SystemExit(check_schema_drift(args.release_notes, args.new_root, args.previous_root))


if __name__ == "__main__":
    main()
