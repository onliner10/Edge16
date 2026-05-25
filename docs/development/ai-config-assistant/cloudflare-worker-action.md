# Cloudflare Worker Action for Edge16 Config Assistant

Use this when the GitHub raw schema Action is not enough. The Worker performs validation and finalization server-side, so ChatGPT does not need Python network access or a local `schema.json` file.

## What the Worker does

- Receives YAML text from the Custom GPT.
- Fetches the matching published Edge16 JSON Schema from GitHub.
- Parses YAML and validates it with AJV.
- Generates unified diffs.
- Recomputes radio settings `checksum:` when finalizing radio YAML.
- Returns `finalYaml` only if validation passes.

## Deploy

Worker source lives in:

```text
tools/edge16-yaml-assistant-worker/
```

Manual local deploy:

```sh
cd tools/edge16-yaml-assistant-worker
npm ci
npm test
npm run check
npm run deploy
```

GitHub Actions deploy:

1. Add repository secrets:
   - `CLOUDFLARE_API_TOKEN`
   - `CLOUDFLARE_ACCOUNT_ID`
2. Run `.github/workflows/deploy_yaml_assistant_worker.yml` manually.
3. If the Cloudflare account has no workers.dev subdomain yet, set the workflow input `workers_dev_subdomain`, for example `onliner10-edge16`.

## Custom GPT Action setup

After deploy, import this OpenAPI URL in the GPT builder:

```text
https://<worker-host>/openapi.yaml
```

Authentication for the private MVP:

```text
None
```

Privacy policy URL:

```text
https://<worker-host>/privacy
```

## GPT rules

The GPT should call:

1. `inspectYaml` after upload.
2. `validateYaml` for inspection-only validation.
3. `diffYaml` before presenting proposed changes.
4. `finalizeYaml` for final output.

The GPT must never provide final edited YAML unless `finalizeYaml` returns `valid: true` and non-null `finalYaml`.

## Fallback

If the Worker is unavailable, use the local Python helper documented in `custom-gpt.md`.
