# Edge16 YAML Assistant Cloudflare Worker

Cloudflare Worker backend for the Edge16 Config Assistant Custom GPT Action.

The Worker is stateless. It receives YAML text from the GPT, fetches published Edge16 JSON Schemas from GitHub, validates YAML, computes diffs, and returns finalized YAML only when validation succeeds.

## Endpoints

- `GET /health`
- `GET /privacy`
- `GET /openapi.yaml`
- `POST /inspect`
- `POST /validate`
- `POST /diff`
- `POST /finalize`

`/finalize` returns `finalYaml: null` when validation fails.

## Local development

```sh
cd tools/edge16-yaml-assistant-worker
npm ci
npm test
npm run check
npm run dev
```

## Deploy manually

Requires Cloudflare Wrangler authentication.

```sh
cd tools/edge16-yaml-assistant-worker
npm ci
npm test
npm run check
npm run deploy
```

After deploy, configure the Custom GPT Action by importing:

```text
https://<worker-host>/openapi.yaml
```

Set authentication to **None** for private MVP use.

## GitHub Actions deploy

The workflow `.github/workflows/deploy_yaml_assistant_worker.yml` is manual-only (`workflow_dispatch`). Configure repository secrets:

- `CLOUDFLARE_API_TOKEN`
- `CLOUDFLARE_ACCOUNT_ID`

Then run the workflow from GitHub Actions.

## Privacy

The Worker does not intentionally persist YAML content. Cloudflare may process transient request metadata to operate the service. Do not submit secrets.

## Limits

- Max YAML payload size: 512 KiB per YAML string.
- Supports only Edge16 `tx16s` and `tx16smk3` schemas from `https://raw.githubusercontent.com/onliner10/Edge16/yaml-schemas/`.
- No direct USB/radio write. User manually copies finalized YAML back to radio.
- MVP diff output is conservative and may show broad whole-file changes.
