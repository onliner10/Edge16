# Edge16 Developer Documentation

This folder contains source for Edge16 developer documentation, built with [MkDocs](https://www.mkdocs.org/) and [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/).

Edge16 docs are TX16S MK2/MK3-focused. Unsupported upstream EdgeTX radio pages may remain in the repository for history, but are excluded from published navigation unless Edge16 support changes.

## Prerequisites

Install documentation dependencies from repository root. Using [`uv`](https://docs.astral.sh/uv/getting-started/installation/) is recommended:

```bash
uv pip install -r docs-requirements.txt
```

Or with plain pip:

```bash
pip install -r docs-requirements.txt
```

[`mike`](https://github.com/jimporter/mike) is included for versioned site deployment.

## Local preview

```bash
mkdocs serve
```

Open [http://127.0.0.1:8000](http://127.0.0.1:8000).

## Building

```bash
mkdocs build --strict
```

CI runs strict docs builds on pull requests.

## Deployment

Docs deploy via `.github/workflows/docs.yml`:

- pushes to `main` deploy `latest`;
- stable release tags such as `v1.0.0` deploy minor version `v1.0` and alias `stable`; alpha/beta/rc tags are pre-release documentation points and should not be treated as stable pilot guidance.

## Project structure

```
docs/
├── assets/          # Images, stylesheets
├── building/        # Build guides
├── contributing/    # Git workflow and contribution guides
├── development/     # Developer reference
├── hardware/        # TX16S-focused hardware reference
├── release/         # Public release notes and feature summaries
└── troubleshooting/ # Recovery guides
```

Site navigation lives in [`mkdocs.yml`](../mkdocs.yml).
