# Madnight Engine Docs

Docusaurus site for [psyqo-madnight-engine](https://github.com/Forceh91/psyqo-madnight-engine).

## Local development

```bash
npm install
npm start
```

Opens at http://localhost:3000/psyqo-madnight-engine/ with hot reload.

## Build

```bash
npm run build
```

Outputs static files to `build/`. Serve locally with `npm run serve` to sanity-check before deploying.

## Deploying to GitHub Pages

`docusaurus.config.js` is already set up for `Forceh91/psyqo-madnight-engine` (org, project name, and `baseUrl` all point at it). Easiest path:

1. Push this site as a folder (e.g. `docs-site/`) in the engine repo, or as its own repo.
2. `GIT_USER=Forceh91 npm run deploy` (uses the `docusaurus deploy` script already in `package.json`) — this builds and pushes `build/` to the `gh-pages` branch.
3. In the repo's GitHub Pages settings, serve from the `gh-pages` branch.

If you'd rather deploy via GitHub Actions instead of the CLI, Docusaurus's own guide covers that: https://docusaurus.io/docs/deployment#deploying-to-github-pages

## Structure

- `docs/getting-started/` — ported from the engine's own `README.md` / `getting-started/README.md`
- `docs/guides/` — ported from `tools/*.md` (MESHBIN/COLBIN/ANIMBIN/SCENEBIN format specs)
- `docs/api/` — API reference written from the engine source headers (`src/`), organized by subsystem
