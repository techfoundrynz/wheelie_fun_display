# Help site

The help site uses TanStack Start, React and Vite to prerender static HTML. GitHub Pages serves only `dist/client` output; no Node server is deployed. The documentation, USB installer and simulator are routes in the same React app.

## Local development

Use Node.js 24 and Corepack. The root pnpm workspace pins pnpm 12.18.0 and retains a 24-hour minimum dependency release age. Its single lockfile is `../pnpm-lock.yaml`. Root commands include `pnpm dev`, `pnpm build:docs`, `pnpm lint` and `pnpm format`.

```sh
cd website
corepack enable
pnpm install --frozen-lockfile
pnpm dev
```

The React app imports one Tailwind stylesheet from `src/app.css`; shell, documentation and tool styles live under `src/styles`. Route navigation does not replace stylesheets. The old `docs/site/style.css` remains for standalone bootstrap artifacts.

Vite serves the app at `http://127.0.0.1:5173/` and reloads Markdown, layout and stylesheet changes. Content stays in the existing repository Markdown files. `content.json` defines their validated titles, routes, groups and order; `plugins/content.ts` compiles their links, anchors and search text. Page content is split into separate lazy-loaded chunks.

The simulator needs generated WASM files under `.cache/pages/site/simulator/pkg`. The installer needs its generated catalog, firmware and ESP Web Tools under `.cache/pages/site/install`. Generate those with the existing repository build scripts before exercising the tools. Documentation editing does not rebuild firmware, WASM or screenshots.

## Build and check

From the repository root, the existing entry point remains supported:

```sh
python scripts/build_docs.py --output .cache/pages/site
```

Or from this directory:

```sh
pnpm build --output ../.cache/pages/site --images ../.cache/pages/docs-images
pnpm check
pnpm lint
pnpm format:check
pnpm test
pnpm test:watch
```

The build requires the generated screenshot manifest and preserves the simulator engine, installer catalog/firmware and CI gallery. It generates the route tree before TypeScript checking. `pnpm format` applies Oxfmt; `pnpm lint` runs Oxlint. The CI workflow uses these same checks.

TypeScript tests use Vitest with a separate `vitest.config.ts`; app build plugins are not loaded for unit tests. `pnpm test` runs once and `pnpm test:watch` watches for changes. Add tests under `src` or `plugins`. Browser acceptance checks continue to use the existing Playwright scripts.

The generated artifact works at both `/` and `/wheelie_fun_display/`. Keep route metadata and relative asset handling intact when adding pages. Browser acceptance checks cover both paths, static HTML links, responsive navigation, device-tool behavior and the isolated PR preview.

## Interactive pages

`src/components/Flasher.tsx` owns catalog loading and board selection in React. `src/tools/flasher.ts` validates firmware URLs; ESP Web Tools handles the explicit USB installation action.

`src/components/Simulator.tsx` owns the scene controls in React. `src/tools/simulator.ts` adapts the generated Slint WASM renderer to its canvas. A persistent simulator mount preserves its single Slint platform instance during navigation and pauses rendering while hidden. The standalone simulator files remain available to bootstrap screenshot generation and isolated engine checks.

Page transitions follow the admin app's 150 ms directional content slides. Navigation stays fixed, open dialogs suppress content snapshots, and reduced-motion preferences disable the animation.
