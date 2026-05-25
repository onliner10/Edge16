#!/usr/bin/env python3
"""Generate JSON Schemas for Edge16 YAML storage data.

The source of truth is the generated YamlNode tables produced by
radio/util/generate_yaml.py. This script reads those tables and emits Draft
2020-12 JSON Schemas that are easy for editors and AI tools to fetch.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
TARGET_TABLES = {
    "tx16s": REPO_ROOT / "radio/src/storage/yaml/yaml_datastructs_x10.cpp",
    "tx16smk3": REPO_ROOT / "radio/src/storage/yaml/yaml_datastructs_tx16smk3.cpp",
}
ROOTS = {
    "model": "struct_ModelData",
    "radio": "struct_RadioData",
}
DRAFT = "https://json-schema.org/draft/2020-12/schema"
SCHEMA_FORMAT_VERSION = 1


@dataclass
class Node:
    kind: str
    tag: str | None = None
    bits: int = 0
    max_len: int = 0
    child: str | None = None
    max_items: int | None = None
    enum: str | None = None
    selector: str | None = None
    read: str | None = None
    write: str | None = None
    idx_custom: bool = False


def split_args(text: str) -> list[str]:
    args: list[str] = []
    current: list[str] = []
    in_string = False
    escape = False
    depth = 0
    for ch in text:
        if escape:
            current.append(ch)
            escape = False
            continue
        if ch == "\\" and in_string:
            current.append(ch)
            escape = True
            continue
        if ch == '"':
            in_string = not in_string
            current.append(ch)
            continue
        if not in_string:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            elif ch == "," and depth == 0:
                args.append("".join(current).strip())
                current = []
                continue
        current.append(ch)
    if current:
        args.append("".join(current).strip())
    return args


def strip_string(value: str) -> str:
    value = value.strip()
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    return value


def parse_int_expr(value: str) -> int | str:
    value = value.strip()
    try:
        return int(value, 0)
    except ValueError:
        return value


def extract_blocks(source: str) -> dict[str, str]:
    pattern = re.compile(r"static const struct YamlNode\s+(\w+)\[\]\s*=\s*\{(.*?)\n\};", re.S)
    return {name: body for name, body in pattern.findall(source)}


def extract_enums(source: str) -> dict[str, list[str]]:
    pattern = re.compile(r"const struct YamlIdStr\s+(enum_\w+)\[\]\s*=\s*\{(.*?)\n\};", re.S)
    enums: dict[str, list[str]] = {}
    for name, body in pattern.findall(source):
        values = []
        for label in re.findall(r"\{\s*[^,]+,\s*\"([^\"]*)\"\s*\}", body):
            if label:
                values.append(label)
        # The regex above intentionally keeps labels only. Keep stable unique order.
        deduped = []
        for label in values:
            if label not in deduped:
                deduped.append(label)
        enums[name] = deduped
    return enums


def parse_node_line(line: str) -> Node | None:
    line = line.strip().rstrip(",")
    if not line or line.startswith("//"):
        return None
    if line == "YAML_END":
        return Node("end")
    if line == "YAML_IDX":
        return Node("idx", tag="idx")
    m = re.match(r"YAML_IDX_CUST\((.*)\)$", line)
    if m:
        args = split_args(m.group(1))
        return Node("idx", tag=strip_string(args[0]), idx_custom=True,
                    read=args[1], write=args[2])
    m = re.match(r"YAML_PADDING\((.*)\)$", line)
    if m:
        bits = parse_int_expr(split_args(m.group(1))[0])
        return Node("padding", bits=bits if isinstance(bits, int) else 0)
    m = re.match(r"YAML_STRING\((.*)\)$", line)
    if m:
        args = split_args(m.group(1))
        return Node("string", tag=strip_string(args[0]), max_len=int(args[1], 0))
    m = re.match(r"YAML_(SIGNED|UNSIGNED)_CUST\((.*)\)$", line)
    if m:
        args = split_args(m.group(2))
        return Node(m.group(1).lower() + "_cust", tag=strip_string(args[0]),
                    bits=int(args[1], 0), read=args[2], write=args[3])
    m = re.match(r"YAML_(SIGNED|UNSIGNED)\((.*)\)$", line)
    if m:
        args = split_args(m.group(2))
        return Node(m.group(1).lower(), tag=strip_string(args[0]), bits=int(args[1], 0))
    m = re.match(r"YAML_ENUM\((.*)\)$", line)
    if m:
        args = split_args(m.group(1))
        return Node("enum", tag=strip_string(args[0]), bits=int(args[1], 0),
                    enum=args[2], selector=args[3])
    m = re.match(r"YAML_(ARRAY|STRUCT)\((.*)\)$", line)
    if m:
        args = split_args(m.group(2))
        if m.group(1) == "STRUCT":
            return Node("struct", tag=strip_string(args[0]), bits=int(args[1], 0),
                        max_items=1, child=args[2], selector=args[3])
        max_items_value = parse_int_expr(args[2])
        max_items = max_items_value if isinstance(max_items_value, int) else None
        return Node("array", tag=strip_string(args[0]), bits=int(args[1], 0),
                    max_items=max_items, child=args[3], selector=args[4])
    m = re.match(r"YAML_UNION\((.*)\)$", line)
    if m:
        args = split_args(m.group(1))
        return Node("union", tag=strip_string(args[0]), bits=int(args[1], 0),
                    child=args[2], selector=args[3])
    m = re.match(r"YAML_CUSTOM\((.*)\)$", line)
    if m:
        args = split_args(m.group(1))
        return Node("custom", tag=strip_string(args[0]), read=args[1], write=args[2])
    raise ValueError(f"Unsupported YamlNode line: {line}")


def parse_nodes(source: str) -> tuple[dict[str, list[Node]], dict[str, list[str]]]:
    blocks = extract_blocks(source)
    structs: dict[str, list[Node]] = {}
    for name, body in blocks.items():
        nodes: list[Node] = []
        for line in body.splitlines():
            node = parse_node_line(line)
            if node is None:
                continue
            if node.kind == "end":
                break
            nodes.append(node)
        structs[name] = nodes
    return structs, extract_enums(source)


def signed_range(bits: int) -> tuple[int, int]:
    if bits <= 0:
        return 0, 0
    return -(1 << (bits - 1)), (1 << (bits - 1)) - 1


def unsigned_range(bits: int) -> tuple[int, int]:
    if bits <= 0:
        return 0, 0
    return 0, (1 << bits) - 1


def custom_schema(node: Node) -> dict[str, Any]:
    schema: dict[str, Any] = {
        "description": "Custom Edge16 YAML field; exact surface syntax is implemented by firmware read/write hooks.",
        "x-edge16-custom-read": None if node.read == "nullptr" else node.read,
        "x-edge16-custom-write": None if node.write == "nullptr" else node.write,
        "anyOf": [{"type": "string"}, {"type": "integer"}, {"type": "boolean"}, {"type": "null"}],
    }
    return schema


def schema_for_node(node: Node, structs: dict[str, list[Node]], enums: dict[str, list[str]]) -> dict[str, Any]:
    if node.kind == "string":
        return {"type": "string", "maxLength": node.max_len}
    if node.kind == "signed":
        lo, hi = signed_range(node.bits)
        return {"type": "integer", "minimum": lo, "maximum": hi}
    if node.kind == "unsigned":
        lo, hi = unsigned_range(node.bits)
        return {"type": "integer", "minimum": lo, "maximum": hi}
    if node.kind in ("signed_cust", "unsigned_cust", "custom"):
        return custom_schema(node)
    if node.kind == "enum":
        values = enums.get(node.enum or "", [])
        if values:
            return {"type": "string", "enum": values}
        lo, hi = unsigned_range(node.bits)
        return {"type": "integer", "minimum": lo, "maximum": hi}
    if node.kind == "struct":
        return {"$ref": f"#/$defs/{node.child}"}
    if node.kind == "array":
        child = node.child or ""
        item_schema = {"$ref": f"#/$defs/{child}"}
        child_nodes = structs.get(child, [])
        indexed = bool(child_nodes and child_nodes[0].kind == "idx")
        if indexed:
            schema = {
                "type": "object",
                "propertyNames": {"type": "string", "minLength": 1},
                "additionalProperties": item_schema,
            }
            if node.max_items is not None:
                schema["maxProperties"] = node.max_items
            return schema
        schema = {"type": "array", "items": item_schema}
        if node.max_items is not None:
            schema["maxItems"] = node.max_items
        return schema
    if node.kind == "union":
        child = node.child or ""
        properties: dict[str, Any] = {}
        for member in structs.get(child, []):
            if not member.tag or member.kind in ("idx", "padding"):
                continue
            properties[member.tag] = schema_for_node(member, structs, enums)
        return {
            "type": "object",
            "description": "Tagged union selected by Edge16 firmware state.",
            "x-edge16-selector": None if node.selector == "nullptr" else node.selector,
            "properties": properties,
            "additionalProperties": False,
        }
    return {"description": f"Unsupported generated YAML node kind {node.kind}"}


def struct_schema(name: str, structs: dict[str, list[Node]], enums: dict[str, list[str]]) -> dict[str, Any]:
    properties: dict[str, Any] = {}
    for node in structs.get(name, []):
        if not node.tag or node.kind in ("idx", "padding"):
            continue
        properties[node.tag] = schema_for_node(node, structs, enums)
    return {"type": "object", "properties": properties, "additionalProperties": False}


def compatibility_scalar_schema() -> dict[str, Any]:
    return {
        "anyOf": [
            {"type": "string"},
            {"type": "integer"},
            {"type": "boolean"},
            {"type": "null"},
        ]
    }


def apply_compatibility_overrides(schema: dict[str, Any], root_name: str) -> None:
    defs = schema.get("$defs", {})
    custom_fn = defs.get("struct_CustomFunctionData")
    if isinstance(custom_fn, dict):
        props = custom_fn.setdefault("properties", {})
        if isinstance(props, dict):
            props["enabled"] = compatibility_scalar_schema()

    zone = defs.get("struct_ZonePersistentData")
    if isinstance(zone, dict):
        props = zone.get("properties")
        if isinstance(props, dict) and "widgetData" in props:
            props["widgetData"] = {"anyOf": [props["widgetData"], {"type": "null"}]}

    if root_name == "model":
        props = schema.get("properties")
        if isinstance(props, dict) and "switchWarning" in props:
            props["switchWarning"] = {
                "anyOf": [
                    props["switchWarning"],
                    {
                        "type": "object",
                        "description": "Legacy switch warning map accepted by Edge16 YAML reader.",
                        "additionalProperties": {
                            "type": "object",
                            "additionalProperties": True,
                        },
                    },
                ]
            }


def make_root_schema(
    target: str,
    root_name: str,
    root_struct: str,
    structs: dict[str, list[Node]],
    enums: dict[str, list[str]],
    commit: str,
    schema_release_version: str,
    generated_at: str,
    schema_id: str | None,
) -> dict[str, Any]:
    defs = {name: struct_schema(name, structs, enums) for name in sorted(structs)}
    schema = struct_schema(root_struct, structs, enums)
    schema["$defs"] = defs
    apply_compatibility_overrides(schema, root_name)
    schema.update({
        "$schema": DRAFT,
        "title": f"Edge16 {target} {root_name} YAML schema",
        "description": "Generated from Edge16 firmware YAML storage node tables.",
        "x-edge16-schema-format-version": SCHEMA_FORMAT_VERSION,
        "x-edge16-schema-release-version": schema_release_version,
        "x-edge16-target": target,
        "x-edge16-root": root_name,
        "x-edge16-generated-from-commit": commit,
        "x-edge16-generated-at": generated_at,
    })
    if schema_id:
        schema["$id"] = schema_id
    return schema


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")


def git_sha() -> str:
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True).strip()
    except Exception:
        return "unknown"


def build_manifest(commit: str, schema_release_version: str, generated_at: str, base_url: str | None) -> dict[str, Any]:
    targets: dict[str, Any] = {}
    for target in TARGET_TABLES:
        targets[target] = {
            root: {
                "path": f"{target}/{root}.schema.json",
                "url": f"{base_url.rstrip('/')}/{target}/{root}.schema.json" if base_url else None,
            }
            for root in ROOTS
        }
    return {
        "manifestVersion": 1,
        "schemaFormatVersion": SCHEMA_FORMAT_VERSION,
        "schemaReleaseVersion": schema_release_version,
        "generatedFromCommit": commit,
        "generatedAt": generated_at,
        "targets": targets,
    }


def generate(out_dir: Path, commit: str, schema_release_version: str, base_url: str | None) -> None:
    generated_at = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    out_dir.mkdir(parents=True, exist_ok=True)

    for target, source_path in TARGET_TABLES.items():
        source = source_path.read_text()
        structs, enums = parse_nodes(source)
        for root_name, root_struct in ROOTS.items():
            schema_id = None
            if base_url:
                schema_id = f"{base_url.rstrip('/')}/{target}/{root_name}.schema.json"
            schema = make_root_schema(target, root_name, root_struct, structs, enums,
                                      commit, schema_release_version, generated_at, schema_id)
            write_json(out_dir / target / f"{root_name}.schema.json", schema)

    write_json(out_dir / "manifest.json", build_manifest(commit, schema_release_version, generated_at, base_url))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="Output directory")
    parser.add_argument("--commit", default=git_sha(), help="Commit identifier used as generation provenance")
    parser.add_argument("--schema-release-version", required=True, help="Published YAML schema release version")
    parser.add_argument("--base-url", help="Base URL for schema IDs and manifest URLs")
    args = parser.parse_args()
    generate(args.out, args.commit, args.schema_release_version, args.base_url)


if __name__ == "__main__":
    main()
