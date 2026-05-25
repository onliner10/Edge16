#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

COMMIT_SHA="${COMMIT_SHA:-$(git -C "$REPO_ROOT" rev-parse HEAD)}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/build/yaml-schema-publish}"
RAW_BASE_URL="${RAW_BASE_URL:-https://raw.githubusercontent.com/onliner10/Edge16/yaml-schemas}"
RELEASE_NOTES="$REPO_ROOT/docs/development/yaml-schema-releases.md"

cd "$REPO_ROOT"

if [[ "${SKIP_YAML_DATA:-0}" != "1" ]]; then
  SRCDIR="$REPO_ROOT" FLAVOR="${FLAVOR:-tx16s;tx16smk3}" ./tools/generate-yaml.sh
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/v1/schema-versions"

SCHEMA_FORMAT_VERSION=1
SCHEMA_RELEASE_VERSION="$(python3 ./tools/check-yaml-schema-version.py \
  --release-notes "$RELEASE_NOTES" \
  --print-latest-release-version)"
SCHEMA_VERSION_DIR="$OUT_DIR/v${SCHEMA_FORMAT_VERSION}/schema-versions/$SCHEMA_RELEASE_VERSION"

python3 ./tools/generate-yaml-schemas.py \
  --out "$SCHEMA_VERSION_DIR" \
  --commit "$COMMIT_SHA" \
  --schema-release-version "$SCHEMA_RELEASE_VERSION" \
  --base-url "$RAW_BASE_URL/v${SCHEMA_FORMAT_VERSION}/schema-versions/$SCHEMA_RELEASE_VERSION"

cp -a "$SCHEMA_VERSION_DIR" "$OUT_DIR/v${SCHEMA_FORMAT_VERSION}/latest"

find "$OUT_DIR" -name '*.json' -print0 | while IFS= read -r -d '' file; do
  python3 -m json.tool "$file" >/dev/null
done

printf 'Generated YAML schema release %s in %s\n' "$SCHEMA_RELEASE_VERSION" "$OUT_DIR"
