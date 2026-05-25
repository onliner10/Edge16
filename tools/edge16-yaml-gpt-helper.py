#!/usr/bin/env python3
"""Deterministic helper for the Edge16 Custom GPT YAML assistant.

This script is intended to run inside ChatGPT Advanced Data Analysis or locally.
It validates uploaded Edge16 YAML against published schemas, computes diffs, and
finalizes radio settings files by replacing the leading checksum line.
"""

from __future__ import annotations

import argparse
import difflib
import json
import re
import sys
import tempfile
import urllib.request
from pathlib import Path
from typing import Any

SCHEMA_BASE_URL = "https://raw.githubusercontent.com/onliner10/Edge16/yaml-schemas"
SCHEMA_URL_RE = re.compile(r"^#\s*yaml-language-server:\s*\$schema=(\S+)\s*$", re.M)
META_RE = re.compile(r"^#\s*(edge16-[A-Za-z0-9_-]+):\s*(\S+)\s*$", re.M)
CHECKSUM_RE = re.compile(r"^checksum:\s*([0-9]+)\r?\n")

# CRC16 table 0 from radio/src/crc.cpp (CCITT polynomial 0x1021).
CRC16TAB_1021 = [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
]


def require_yaml_module() -> Any:
    try:
        import yaml  # type: ignore
    except ImportError as exc:
        raise SystemExit("Missing Python package: pyyaml. Install with: pip install pyyaml") from exc
    return yaml


def require_jsonschema_module() -> Any:
    try:
        import jsonschema  # type: ignore
    except ImportError as exc:
        raise SystemExit("Missing Python package: jsonschema. Install with: pip install jsonschema") from exc
    return jsonschema


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="")


def strip_checksum_line(text: str) -> tuple[str, int | None]:
    match = CHECKSUM_RE.match(text)
    if not match:
        return text, None
    return text[match.end():], int(match.group(1))


def normalize_radio_body_line_endings(body: str) -> str:
    return body.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n")


def crc16_edge16(data: bytes, start: int = 0xFFFF) -> int:
    crc = start & 0xFFFF
    for byte in data:
        crc = ((crc << 8) ^ CRC16TAB_1021[((crc >> 8) ^ byte) & 0xFF]) & 0xFFFF
    return crc


def compute_radio_checksum(body_after_checksum_line: str) -> int:
    return crc16_edge16(body_after_checksum_line.encode("utf-8"), 0xFFFF)


def parse_metadata(text: str) -> dict[str, str]:
    metadata: dict[str, str] = {}
    schema_match = SCHEMA_URL_RE.search(text)
    if schema_match:
        metadata["schema_url"] = schema_match.group(1)
    for match in META_RE.finditer(text):
        metadata[match.group(1)] = match.group(2)
    return metadata


def fetch_json(url: str) -> Any:
    with urllib.request.urlopen(url, timeout=30) as response:  # nosec B310 - user/GPT requested schema URL
        charset = response.headers.get_content_charset() or "utf-8"
        return json.loads(response.read().decode(charset))


def load_json_file(path: Path) -> Any:
    if not path.exists():
        raise SystemExit(f"Schema file does not exist: {path}")
    if path.stat().st_size == 0:
        raise SystemExit(
            f"Schema file is empty: {path}. Save the Action response JSON to this file "
            "or upload the matching schema manually."
        )
    return json.loads(path.read_text(encoding="utf-8"))


def default_schema_url(target: str, root: str, version: str | None = None) -> str:
    if version:
        return f"{SCHEMA_BASE_URL}/v1/schema-versions/{version}/{target}/{root}.schema.json"
    return f"{SCHEMA_BASE_URL}/v1/latest/{target}/{root}.schema.json"


def resolve_schema(args: argparse.Namespace, metadata: dict[str, str]) -> tuple[Any, str]:
    if getattr(args, "schema_file", None):
        schema_path = Path(args.schema_file)
        return load_json_file(schema_path), str(schema_path)

    schema_root = getattr(args, "schema_root", None)
    target = getattr(args, "target", None) or metadata.get("edge16-target")
    root = getattr(args, "root", None) or metadata.get("edge16-yaml-root")
    version = getattr(args, "schema_version", None) or metadata.get("edge16-schema-version")

    if schema_root:
        if not target or not root:
            raise SystemExit("--schema-root requires --target and --root when YAML metadata is missing")
        schema_path = Path(schema_root) / target / f"{root}.schema.json"
        return load_json_file(schema_path), str(schema_path)

    schema_url = getattr(args, "schema_url", None) or metadata.get("schema_url")
    if not schema_url and target and root:
        schema_url = default_schema_url(target, root, version)
    if not schema_url:
        raise SystemExit(
            "No schema found. Provide --schema-file, --schema-url, or --target plus --root "
            "(model/radio), or upload YAML containing schema locator comments."
        )
    return fetch_json(schema_url), schema_url


def parse_yaml_for_schema(text: str) -> Any:
    yaml = require_yaml_module()
    without_checksum, _ = strip_checksum_line(text)
    data = yaml.safe_load(without_checksum)
    return {} if data is None else data


def validation_errors(schema: Any, data: Any) -> list[str]:
    jsonschema = require_jsonschema_module()
    validator_cls = jsonschema.validators.validator_for(schema)
    validator_cls.check_schema(schema)
    validator = validator_cls(schema)
    errors = sorted(validator.iter_errors(data), key=lambda err: list(err.path))
    return [format_validation_error(err) for err in errors]


def format_validation_error(err: Any) -> str:
    path = ".".join(str(part) for part in err.absolute_path)
    return f"{path or '<root>'}: {err.message}"


def inspect_text(text: str) -> dict[str, Any]:
    body, checksum = strip_checksum_line(text)
    metadata = parse_metadata(text)
    data = None
    parse_error = None
    try:
        parsed = parse_yaml_for_schema(text)
        if isinstance(parsed, dict):
            data = {
                "topLevelKeys": list(parsed.keys()),
                "topLevelKeyCount": len(parsed),
            }
        else:
            data = {"type": type(parsed).__name__}
    except Exception as exc:  # noqa: BLE001 - report parse error to user/GPT
        parse_error = str(exc)
    return {
        "hasChecksum": checksum is not None,
        "checksum": checksum,
        "bodyBytesAfterChecksum": len(body.encode("utf-8")),
        "metadata": metadata,
        "yaml": data,
        "parseError": parse_error,
    }


def make_diff(original_text: str, edited_text: str, original_name: str = "original.yml", edited_name: str = "edited.yml") -> str:
    return "".join(
        difflib.unified_diff(
            original_text.splitlines(keepends=True),
            edited_text.splitlines(keepends=True),
            fromfile=original_name,
            tofile=edited_name,
        )
    )


def validate_text(text: str, args: argparse.Namespace) -> dict[str, Any]:
    metadata = parse_metadata(text)
    schema, schema_source = resolve_schema(args, metadata)
    data = parse_yaml_for_schema(text)
    errors = validation_errors(schema, data)
    return {
        "valid": not errors,
        "schemaSource": schema_source,
        "metadata": metadata,
        "errors": errors,
    }


def finalize_text(original_text: str, edited_text: str, args: argparse.Namespace) -> tuple[str, dict[str, Any]]:
    validation = validate_text(edited_text, args)
    final_text = edited_text
    old_checksum = strip_checksum_line(original_text)[1]
    _, edited_checksum = strip_checksum_line(edited_text)
    new_checksum = None

    if old_checksum is not None or edited_checksum is not None or getattr(args, "radio", False):
        body, _ = strip_checksum_line(edited_text)
        body = normalize_radio_body_line_endings(body)
        new_checksum = compute_radio_checksum(body)
        final_text = f"checksum: {new_checksum}\r\n{body}"
        validation = validate_text(final_text, args)

    diff = make_diff(original_text, final_text)
    report = {
        "valid": validation["valid"],
        "schemaSource": validation["schemaSource"],
        "metadata": validation["metadata"],
        "errors": validation["errors"],
        "checksum": {
            "old": old_checksum,
            "edited": edited_checksum,
            "new": new_checksum,
        },
        "diff": diff,
    }
    return final_text, report


def print_json(value: Any) -> None:
    print(json.dumps(value, indent=2, sort_keys=True))


def command_inspect(args: argparse.Namespace) -> int:
    report = inspect_text(read_text(Path(args.input)))
    print_json(report) if args.json else print(report)
    return 0 if report["parseError"] is None else 1


def command_validate(args: argparse.Namespace) -> int:
    report = validate_text(read_text(Path(args.input)), args)
    print_json(report)
    return 0 if report["valid"] else 1


def command_diff(args: argparse.Namespace) -> int:
    original = read_text(Path(args.original))
    edited = read_text(Path(args.edited))
    print(make_diff(original, edited, args.original, args.edited), end="")
    return 0


def command_finalize(args: argparse.Namespace) -> int:
    original = read_text(Path(args.original))
    edited = read_text(Path(args.edited))
    final_text, report = finalize_text(original, edited, args)
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
    if report["valid"]:
        write_text(Path(args.out), final_text)
    else:
        report["outputSkipped"] = "validation failed; final YAML was not written"
    print_json(report)
    return 0 if report["valid"] else 1


def command_self_test() -> int:
    assert crc16_edge16(b"123456789", 0xFFFF) == 0x29B1
    body = "name: Radio\r\n"
    checksum = compute_radio_checksum(body)
    assert f"checksum: {checksum}\r\n" + body == f"checksum: {checksum}\r\nname: Radio\r\n"

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        schema = {
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "type": "object",
            "additionalProperties": False,
            "properties": {"name": {"type": "string"}},
            "required": ["name"],
        }
        schema_path = tmp / "schema.json"
        schema_path.write_text(json.dumps(schema), encoding="utf-8")
        original = tmp / "original.yml"
        edited = tmp / "edited.yml"
        final = tmp / "final.yml"
        original.write_text("checksum: 0\r\nname: Old\r\n", encoding="utf-8")
        edited.write_text("checksum: 0\nname: New\n", encoding="utf-8")
        args = argparse.Namespace(
            schema_file=str(schema_path), schema_root=None, schema_url=None,
            target=None, root=None, schema_version=None, radio=False,
        )
        final_text, report = finalize_text(original.read_text(encoding="utf-8"), edited.read_text(encoding="utf-8"), args)
        final.write_text(final_text, encoding="utf-8", newline="")
        assert report["valid"] is True
        assert report["checksum"]["new"] == compute_radio_checksum("name: New\r\n")
        assert final.read_bytes().startswith(f"checksum: {report['checksum']['new']}\r\n".encode("utf-8"))
    print("self-test ok")
    return 0


def add_schema_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--schema-file", help="Local JSON schema file")
    parser.add_argument("--schema-url", help="JSON schema URL")
    parser.add_argument("--schema-root", help="Directory containing <target>/<root>.schema.json")
    parser.add_argument("--schema-version", help="Edge16 schema release version, e.g. 1.0")
    parser.add_argument("--target", choices=["tx16s", "tx16smk3"], help="Radio target")
    parser.add_argument("--root", choices=["model", "radio"], help="YAML root kind")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="Run helper self-test")
    subparsers = parser.add_subparsers(dest="command")

    inspect_parser = subparsers.add_parser("inspect", help="Inspect YAML metadata and parse status")
    inspect_parser.add_argument("input")
    inspect_parser.add_argument("--json", action="store_true", help="Print JSON report")

    validate_parser = subparsers.add_parser("validate", help="Validate YAML against schema")
    validate_parser.add_argument("input")
    add_schema_args(validate_parser)

    diff_parser = subparsers.add_parser("diff", help="Print unified diff")
    diff_parser.add_argument("original")
    diff_parser.add_argument("edited")

    finalize_parser = subparsers.add_parser("finalize", help="Validate, recompute checksum if needed, and write final YAML")
    finalize_parser.add_argument("original")
    finalize_parser.add_argument("edited")
    finalize_parser.add_argument("--out", required=True)
    finalize_parser.add_argument("--report")
    finalize_parser.add_argument("--radio", action="store_true", help="Force radio checksum finalization")
    add_schema_args(finalize_parser)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.self_test:
        return command_self_test()
    if args.command == "inspect":
        return command_inspect(args)
    if args.command == "validate":
        return command_validate(args)
    if args.command == "diff":
        return command_diff(args)
    if args.command == "finalize":
        return command_finalize(args)
    parser.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
