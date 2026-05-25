import assert from "node:assert/strict";
import { computeRadioChecksum, crc16Edge16, finalizeYamlText, inspectYamlText, validateYamlText } from "./logic";

const schema = {
  $schema: "https://json-schema.org/draft/2020-12/schema",
  type: "object",
  additionalProperties: false,
  properties: {
    name: { type: "string" },
  },
  required: ["name"],
};

const originalFetch = globalThis.fetch;
globalThis.fetch = async (input: string | URL | Request) => {
  const url = typeof input === "string" ? input : input instanceof URL ? input.toString() : input.url;
  assert.equal(
    url,
    "https://raw.githubusercontent.com/onliner10/Edge16/yaml-schemas/v1/latest/tx16s/radio.schema.json",
  );
  return new Response(JSON.stringify(schema), { status: 200, headers: { "content-type": "application/json" } });
};

assert.equal(crc16Edge16(new TextEncoder().encode("123456789")), 0x29b1);
assert.equal(computeRadioChecksum("name: Radio\r\n"), 36181);

const original = "checksum: 0\r\nname: Old\r\n";
const edited = "checksum: 0\nname: New\n";

const inspect = inspectYamlText(original);
assert.equal(inspect.hasChecksum, true);
assert.deepEqual(inspect.topLevelKeys, ["name"]);

const validation = await validateYamlText(edited, { target: "tx16s", root: "radio" });
assert.equal(validation.valid, true);

const finalized = await finalizeYamlText({ originalYaml: original, editedYaml: edited, target: "tx16s", root: "radio" });
assert.equal(finalized.valid, true);
assert.equal(typeof finalized.finalYaml, "string");
assert.match(finalized.finalYaml as string, /^checksum: \d+\r\nname: New\r\n$/);
assert.equal((finalized.checksum as { new: number }).new, computeRadioChecksum("name: New\r\n"));

const invalid = await finalizeYamlText({ originalYaml: original, editedYaml: "name: 123\n", target: "tx16s", root: "radio" });
assert.equal(invalid.valid, false);
assert.equal(invalid.finalYaml, null);

globalThis.fetch = originalFetch;
console.log("self-test ok");
