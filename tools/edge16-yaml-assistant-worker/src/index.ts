import { finalizeYamlText, inspectYamlText, makeDiff, validateYamlText } from "./logic";

const JSON_HEADERS = {
  "content-type": "application/json; charset=utf-8",
  "access-control-allow-origin": "*",
  "access-control-allow-methods": "GET,POST,OPTIONS",
  "access-control-allow-headers": "content-type,authorization",
};

export default {
  async fetch(request: Request): Promise<Response> {
    if (request.method === "OPTIONS") return new Response(null, { headers: JSON_HEADERS });

    const url = new URL(request.url);
    try {
      if (request.method === "GET" && url.pathname === "/health") {
        return json({ ok: true, service: "edge16-yaml-assistant" });
      }
      if (request.method === "GET" && url.pathname === "/privacy") {
        return new Response(privacyText(url.origin), {
          headers: { "content-type": "text/plain; charset=utf-8" },
        });
      }
      if (request.method === "GET" && url.pathname === "/openapi.yaml") {
        return new Response(openApiYaml(url.origin), {
          headers: { "content-type": "application/yaml; charset=utf-8" },
        });
      }
      if (request.method === "POST" && url.pathname === "/inspect") {
        const body = await readJson(request);
        requireString(body.yaml, "yaml");
        return json({ ok: true, detected: inspectYamlText(body.yaml) });
      }
      if (request.method === "POST" && url.pathname === "/validate") {
        const body = await readJson(request);
        requireString(body.yaml, "yaml");
        return json(await validateYamlText(body.yaml, body));
      }
      if (request.method === "POST" && url.pathname === "/diff") {
        const body = await readJson(request);
        requireString(body.originalYaml, "originalYaml");
        requireString(body.editedYaml, "editedYaml");
        return json(makeDiff(body.originalYaml, body.editedYaml));
      }
      if (request.method === "POST" && url.pathname === "/finalize") {
        const body = await readJson(request);
        requireString(body.originalYaml, "originalYaml");
        requireString(body.editedYaml, "editedYaml");
        return json(await finalizeYamlText(body));
      }
      return json({ ok: false, error: "not found" }, 404);
    } catch (error) {
      return json({ ok: false, error: error instanceof Error ? error.message : String(error) }, 400);
    }
  },
};

async function readJson(request: Request): Promise<Record<string, unknown>> {
  const value = await request.json();
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error("request body must be a JSON object");
  }
  return value as Record<string, unknown>;
}

function requireString(value: unknown, name: string): asserts value is string {
  if (typeof value !== "string") throw new Error(`${name} must be a string`);
}

function json(value: unknown, status = 200): Response {
  return new Response(JSON.stringify(value, null, 2), { status, headers: JSON_HEADERS });
}

function privacyText(origin: string): string {
  return `Edge16 YAML Assistant Action Privacy Policy\n\nThis service processes YAML content sent by a Custom GPT only to inspect, validate, diff, and finalize Edge16 configuration files. It fetches public schemas from GitHub raw content. It does not require user accounts and does not intentionally store YAML files or API request bodies. Cloudflare may process transient request metadata to operate the Worker. Do not submit secrets.\n\nService: ${origin}\n`;
}

function openApiYaml(origin: string): string {
  return `openapi: 3.0.3
info:
  title: Edge16 YAML Assistant Action
  version: 1.0.0
  description: Validate, diff, and finalize Edge16 TX16S YAML settings using published Edge16 schemas.
servers:
  - url: ${origin}
paths:
  /health:
    get:
      operationId: health
      summary: Check service health
      responses:
        "200":
          description: Service health.
  /inspect:
    post:
      operationId: inspectYaml
      summary: Inspect Edge16 YAML metadata and parse status
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: "#/components/schemas/InspectRequest"
      responses:
        "200":
          description: Inspection result.
  /validate:
    post:
      operationId: validateYaml
      summary: Validate Edge16 YAML against the matching published schema
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: "#/components/schemas/ValidateRequest"
      responses:
        "200":
          description: Validation result.
  /diff:
    post:
      operationId: diffYaml
      summary: Create a unified diff between original and edited YAML
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: "#/components/schemas/DiffRequest"
      responses:
        "200":
          description: Unified diff result.
  /finalize:
    post:
      operationId: finalizeYaml
      summary: Validate edited YAML, recompute radio checksum if needed, and return final YAML only when valid
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: "#/components/schemas/FinalizeRequest"
      responses:
        "200":
          description: Finalization result. finalYaml is null when validation fails.
components:
  schemas:
    InspectRequest:
      type: object
      properties:
        yaml:
          type: string
      required: [yaml]
    ValidateRequest:
      type: object
      properties:
        yaml:
          type: string
        target:
          type: string
          enum: [tx16s, tx16smk3]
        root:
          type: string
          enum: [model, radio]
        schemaVersion:
          type: string
        schemaUrl:
          type: string
      required: [yaml]
    DiffRequest:
      type: object
      properties:
        originalYaml:
          type: string
        editedYaml:
          type: string
      required: [originalYaml, editedYaml]
    FinalizeRequest:
      type: object
      properties:
        originalYaml:
          type: string
        editedYaml:
          type: string
        target:
          type: string
          enum: [tx16s, tx16smk3]
        root:
          type: string
          enum: [model, radio]
        schemaVersion:
          type: string
        schemaUrl:
          type: string
        forceRadio:
          type: boolean
      required: [originalYaml, editedYaml]
`;
}
