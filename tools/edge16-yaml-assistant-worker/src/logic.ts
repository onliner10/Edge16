import { Validator, type Schema } from "@cfworker/json-schema";
import { parse as parseYaml } from "yaml";

const SCHEMA_BASE_URL = "https://raw.githubusercontent.com/onliner10/Edge16/yaml-schemas";
const ALLOWED_SCHEMA_PREFIX = `${SCHEMA_BASE_URL}/`;
const CHECKSUM_RE = /^checksum:\s*([0-9]+)\r?\n/;
const SCHEMA_URL_RE = /^#\s*yaml-language-server:\s*\$schema=(\S+)\s*$/m;
const META_RE = /^#\s*(edge16-[A-Za-z0-9_-]+):\s*(\S+)\s*$/gm;
const MAX_YAML_BYTES = 512 * 1024;

type Target = "tx16s" | "tx16smk3";
type Root = "model" | "radio";

export interface AssistantRequest {
  yaml?: string;
  originalYaml?: string;
  editedYaml?: string;
  target?: Target;
  root?: Root;
  schemaVersion?: string;
  schemaUrl?: string;
  forceRadio?: boolean;
}

export interface DetectedYaml {
  hasChecksum: boolean;
  checksum: number | null;
  checksumValid: boolean | null;
  computedChecksum: number | null;
  metadata: Record<string, string>;
  target: string | null;
  root: string | null;
  schemaVersion: string | null;
  schemaUrl: string | null;
  topLevelKeys: string[];
  parseError: string | null;
}

interface SchemaResolution {
  schema: unknown;
  source: string;
}

export function inspectYamlText(text: string): DetectedYaml {
  enforceSize(text);
  const { body, checksum } = stripChecksumLine(text);
  const metadata = parseMetadata(text);
  const computedChecksum = checksum === null ? null : computeRadioChecksum(body);
  const parsed = parseYamlData(text);
  return {
    hasChecksum: checksum !== null,
    checksum,
    checksumValid: checksum === null ? null : checksum === computedChecksum,
    computedChecksum,
    metadata,
    target: metadata["edge16-target"] ?? null,
    root: metadata["edge16-yaml-root"] ?? null,
    schemaVersion: metadata["edge16-schema-version"] ?? null,
    schemaUrl: metadata.schema_url ?? null,
    topLevelKeys: parsed.ok && isRecord(parsed.value) ? Object.keys(parsed.value) : [],
    parseError: parsed.ok ? null : parsed.error,
  };
}

export async function validateYamlText(text: string, request: AssistantRequest = {}): Promise<Record<string, unknown>> {
  enforceSize(text);
  const detected = inspectYamlText(text);
  const parsed = parseYamlData(text);
  if (!parsed.ok) {
    return {
      ok: true,
      valid: false,
      errors: [`YAML parse error: ${parsed.error}`],
      detected,
      schemaSource: null,
    };
  }

  const schemaResult = await resolveSchema(text, request, detected);
  const errors = validateData(schemaResult.schema, parsed.value);
  return {
    ok: true,
    valid: errors.length === 0,
    errors,
    detected,
    schemaSource: schemaResult.source,
  };
}

export async function finalizeYamlText(request: AssistantRequest): Promise<Record<string, unknown>> {
  if (request.originalYaml === undefined || request.editedYaml === undefined) {
    throw new Error("originalYaml and editedYaml are required");
  }
  enforceSize(request.originalYaml);
  enforceSize(request.editedYaml);

  const originalInfo = inspectYamlText(request.originalYaml);
  const editedInfo = inspectYamlText(request.editedYaml);
  let finalYaml = request.editedYaml;
  let newChecksum: number | null = null;

  const shouldFinalizeRadio =
    request.forceRadio === true ||
    request.root === "radio" ||
    editedInfo.root === "radio" ||
    originalInfo.hasChecksum ||
    editedInfo.hasChecksum;

  if (shouldFinalizeRadio) {
    const { body } = stripChecksumLine(request.editedYaml);
    const normalizedBody = normalizeCrLf(body);
    newChecksum = computeRadioChecksum(normalizedBody);
    finalYaml = `checksum: ${newChecksum}\r\n${normalizedBody}`;
  }

  const validation = await validateYamlText(finalYaml, request);
  const valid = validation.valid === true;
  const diff = makeUnifiedDiff("original.yml", "final.yml", request.originalYaml, finalYaml);

  return {
    ok: true,
    valid,
    errors: validation.errors,
    schemaSource: validation.schemaSource,
    detected: validation.detected,
    checksum: {
      old: originalInfo.checksum,
      edited: editedInfo.checksum,
      new: newChecksum,
    },
    diff,
    finalYaml: valid ? finalYaml : null,
  };
}

export function makeDiff(originalYaml: string, editedYaml: string): Record<string, unknown> {
  enforceSize(originalYaml);
  enforceSize(editedYaml);
  return {
    ok: true,
    diff: makeUnifiedDiff("original.yml", "edited.yml", originalYaml, editedYaml),
  };
}

export function makeUnifiedDiff(originalName: string, editedName: string, originalText: string, editedText: string): string {
  if (originalText === editedText) return "";
  const originalLines = originalText.split(/(?<=\n)/);
  const editedLines = editedText.split(/(?<=\n)/);
  const oldCount = originalLines.length;
  const newCount = editedLines.length;
  const lines = [
    `--- ${originalName}\n`,
    `+++ ${editedName}\n`,
    `@@ -1,${oldCount} +1,${newCount} @@\n`,
    ...originalLines.map((line) => `-${line}`),
    ...editedLines.map((line) => `+${line}`),
  ];
  return lines.join("");
}

export function crc16Edge16(data: Uint8Array, start = 0xffff): number {
  let crc = start & 0xffff;
  for (const byte of data) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc & 0x8000) !== 0 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
    }
  }
  return crc & 0xffff;
}

export function computeRadioChecksum(bodyAfterChecksumLine: string): number {
  return crc16Edge16(new TextEncoder().encode(bodyAfterChecksumLine), 0xffff);
}

export function stripChecksumLine(text: string): { body: string; checksum: number | null } {
  const match = CHECKSUM_RE.exec(text);
  if (!match) return { body: text, checksum: null };
  const parsed = Number.parseInt(match[1] ?? "", 10);
  return { body: text.slice(match[0].length), checksum: Number.isFinite(parsed) ? parsed : null };
}

export function normalizeCrLf(text: string): string {
  return text.replace(/\r\n/g, "\n").replace(/\r/g, "\n").replace(/\n/g, "\r\n");
}

function parseMetadata(text: string): Record<string, string> {
  const metadata: Record<string, string> = {};
  const schemaMatch = SCHEMA_URL_RE.exec(text);
  if (schemaMatch?.[1]) metadata.schema_url = schemaMatch[1];
  for (const match of text.matchAll(META_RE)) {
    if (match[1] && match[2]) metadata[match[1]] = match[2];
  }
  return metadata;
}

function parseYamlData(text: string): { ok: true; value: unknown } | { ok: false; error: string } {
  try {
    const { body } = stripChecksumLine(text);
    return { ok: true, value: parseYaml(body) ?? {} };
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : String(error) };
  }
}

async function resolveSchema(text: string, request: AssistantRequest, detected: DetectedYaml): Promise<SchemaResolution> {
  const metadata = detected.metadata;
  const requestedUrl = request.schemaUrl ?? metadata.schema_url;
  if (requestedUrl) {
    assertAllowedSchemaUrl(requestedUrl);
    return { schema: await fetchJson(requestedUrl), source: requestedUrl };
  }

  const target = request.target ?? asTarget(metadata["edge16-target"]);
  const root = request.root ?? asRoot(metadata["edge16-yaml-root"]);
  const schemaVersion = request.schemaVersion ?? metadata["edge16-schema-version"];
  if (!target || !root) {
    throw new Error("target and root are required when YAML has no schema locator comments");
  }

  const url = schemaVersion
    ? `${SCHEMA_BASE_URL}/v1/schema-versions/${encodeURIComponent(schemaVersion)}/${target}/${root}.schema.json`
    : `${SCHEMA_BASE_URL}/v1/latest/${target}/${root}.schema.json`;
  assertAllowedSchemaUrl(url);
  return { schema: await fetchJson(url), source: url };
}

function validateData(schema: unknown, data: unknown): string[] {
  const validator = new Validator(schema as Schema, "2020-12", false);
  const result = validator.validate(data);
  if (result.valid) return [];
  return result.errors.map((error) => {
    const path = error.instanceLocation || "#";
    return `${path}: ${error.error}`;
  });
}

async function fetchJson(url: string): Promise<unknown> {
  const response = await fetch(url, {
    cf: { cacheTtl: 300, cacheEverything: true },
    headers: { accept: "application/json" },
  });
  if (!response.ok) {
    throw new Error(`schema fetch failed: ${response.status} ${response.statusText} (${url})`);
  }
  return response.json();
}

function assertAllowedSchemaUrl(url: string): void {
  if (!url.startsWith(ALLOWED_SCHEMA_PREFIX)) {
    throw new Error(`schema URL is not allowed: ${url}`);
  }
}

function asTarget(value: string | undefined): Target | undefined {
  return value === "tx16s" || value === "tx16smk3" ? value : undefined;
}

function asRoot(value: string | undefined): Root | undefined {
  return value === "model" || value === "radio" ? value : undefined;
}

function enforceSize(text: string): void {
  const bytes = new TextEncoder().encode(text).length;
  if (bytes > MAX_YAML_BYTES) {
    throw new Error(`YAML payload too large: ${bytes} bytes; maximum is ${MAX_YAML_BYTES}`);
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}
