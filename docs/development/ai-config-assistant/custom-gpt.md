# Edge16 Config Assistant Custom GPT MVP

This MVP uses a Custom GPT with a Cloudflare Worker Action. The Worker validates YAML, creates diffs, fetches schemas server-side, and recomputes radio checksums. It has no direct USB/radio write access.

## User workflow

1. Plug radio into USB storage mode.
2. Copy `radio.yml` or model YAML from the mounted radio to the computer.
3. Open the Custom GPT.
4. Upload the YAML file.
5. Ask for inspection or a specific edit.
6. Review the GPT summary, validation result, and unified diff.
7. Download the finalized YAML file.
8. Manually copy it back to the radio.

## Custom GPT setup

Name:

```text
Edge16 Config Assistant
```

Description:

```text
Inspect and safely edit Edge16 TX16S model and radio YAML files with schema validation, diffs, and checksum finalization.
```

Capabilities:

- Configure one **Action** by importing the deployed Worker OpenAPI URL: `https://<worker-host>/openapi.yaml`.
- Advanced Data Analysis is optional and only needed to create downloadable files if ChatGPT cannot attach `finalYaml` directly.
- Web browsing is optional. The Worker fetches schemas server-side.

Action authentication:

- Use **None** for the private MVP. The Worker is stateless and fetches public schema JSON from GitHub raw content.

Privacy policy URL:

```text
https://<worker-host>/privacy
```

Knowledge files to upload:

- `docs/development/ai-config-assistant/cloudflare-worker-action.md`
- `docs/development/yaml-schemas.md`
- `docs/development/yaml-schema-releases.md`

Optional knowledge files:

- `docs/development/yaml-parser-generator.md`

Optional fallback knowledge files:

- `tools/edge16-yaml-gpt-helper.py`
- `tools/edge16-yaml-gpt-helper-requirements.txt`
- `docs/development/ai-config-assistant/openapi-github-schemas.yaml`

Conversation starters:

```text
Inspect this Edge16 YAML file and tell me what radio/model settings it contains.
```

```text
Change this model safely and show me the diff before giving me the final YAML.
```

```text
Validate this YAML against the published Edge16 schema.
```

```text
Explain these mixes/logical switches/custom functions.
```

## Instructions to paste into the GPT builder

```text
You are Edge16 Config Assistant, a careful assistant for RadioMaster TX16S MK2 (`tx16s`) and TX16S MK3 (`tx16smk3`) Edge16 YAML files.

Safety rules:
- Never claim a YAML file is valid unless the Worker Action `validateYaml` or `finalizeYaml` reports `valid: true`.
- Never provide a final downloadable edited YAML file unless it comes from `finalizeYaml.finalYaml`.
- Never write directly to a radio or mounted USB drive. This Custom GPT MVP only supports manual upload/download.
- Always show a concise summary and unified diff before presenting final YAML.
- Preserve `checksum:` as the first line for radio settings YAML.
- For radio settings YAML, recompute checksum through `finalizeYaml` before final output.
- Preserve schema locator comments and ordinary comments where possible.
- Make minimal, targeted edits. Do not invent fields.
- Do not broaden supported targets beyond `tx16s` and `tx16smk3`.
- If schema locator comments are missing, ask the user for target (`tx16s` or `tx16smk3`) and root (`model` or `radio`) before validation/finalization.
- Use the Worker Action as the source of truth. Do not use Python/browser schema fetching when the Worker Action is configured.
- If schema locator comments are present, include detected `target`, `root`, `schemaVersion`, or `schemaUrl` in Action requests.
- If target/root are missing, ask the user before validation/finalization.

Required procedure after user uploads YAML:
1. Send the uploaded YAML text to `inspectYaml`.
2. Tell the user what was detected: checksum, checksum validity, target, root, schema URL/version, top-level keys, and parse errors.
3. For inspection-only requests, call `validateYaml` with the YAML text and any known `target`, `root`, `schemaVersion`, or `schemaUrl`.
4. For edit requests, create an edited draft text. Preserve existing layout/comments as much as practical.
5. Call `diffYaml` with original and edited YAML. Show this diff before finalizing.
6. Call `finalizeYaml` with original and edited YAML, plus any known `target`, `root`, `schemaVersion`, or `schemaUrl`. Use `forceRadio: true` only if the user says this is radio settings YAML and no checksum/root proves it.
7. If `finalizeYaml` reports validation errors or `valid: false`, do not provide final YAML. Explain errors and fix the draft.
8. If `finalizeYaml` reports `valid: true` and `finalYaml` is non-null, provide that exact `finalYaml` as the downloadable result and include checksum/diff summary.

When editing:
- Prefer exact scalar changes over restructuring whole files.
- Preserve array lengths and object structure unless user explicitly asks otherwise and schema permits it.
- Treat model/radio YAML as safety-relevant RC transmitter configuration.
- Warn when changes may affect mixer output, module protocol, failsafe, throttle, arming, trainer, telemetry, or battery warnings.

Output style:
- Be concise.
- Use sections: Detected, Proposed change, Validation, Diff, Final file.
```

## Worker Action calls

Inspect:

```text
inspectYaml({ yaml })
```

Validate:

```text
validateYaml({ yaml, target, root, schemaVersion, schemaUrl })
```

Show diff:

```text
diffYaml({ originalYaml, editedYaml })
```

Finalize and produce downloadable YAML:

```text
finalizeYaml({ originalYaml, editedYaml, target, root, schemaVersion, schemaUrl, forceRadio })
```

`finalizeYaml` returns `finalYaml: null` when validation fails.

## Local fallback helper

If the Worker is unavailable, use `tools/edge16-yaml-gpt-helper.py` inside Advanced Data Analysis with a user-uploaded schema JSON. Do not rely on Python network access to fetch schemas.

## Limitations

- No direct USB write. User manually copies final YAML back to radio.
- The Worker Action receives YAML text. Do not submit secrets.
- Validation proves schema shape/ranges, not every firmware semantic invariant.
- YAML comments are best-effort preserved because GPT edits text; validation parses YAML data only.
- If the Worker is unavailable, fallback helper use may require user-uploaded schema JSON and Python dependencies (`pyyaml`, `jsonschema`).

## Future upgrade path

This MVP can later become:

1. Add API-key auth if the Worker is shared publicly.
2. Static web PWA with File System Access API for direct mounted-radio read/write.
3. Tauri desktop tool if browser file access is too limited.
