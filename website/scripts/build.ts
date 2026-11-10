import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { parseArgs } from "node:util";
import { readCatalog, root, siteRoot } from "../plugins/content.ts";

const { values } = parseArgs({
  options: {
    output: { type: "string", default: path.join(root, ".cache/pages/site") },
    images: { type: "string", default: path.join(root, ".cache/pages/docs-images") },
  },
  strict: true,
});
const output = path.resolve(values.output!);
const relative = path.relative(root, output);
if (!relative || relative.startsWith("..") || path.isAbsolute(relative))
  throw new Error("Documentation output must be a separate directory inside the repository");
execFileSync(
  process.execPath,
  [path.join(root, "website/node_modules/vite/bin/vite.js"), "build"],
  {
    cwd: path.join(root, "website"),
    stdio: "inherit",
    env: { ...process.env, DOCS_IMAGES: path.resolve(values.images!), DOCS_BUILD: "1" },
  },
);
const client = path.join(root, "website/dist/client");
const catalog = readCatalog();
const pages = ["index.html", "404.html", ...catalog.map((page) => page.route + "/index.html")];
const ownedFiles: string[] = [];
const manifestPath = path.join(output, "docs-manifest.json");
const previous = fs.existsSync(manifestPath)
  ? (JSON.parse(fs.readFileSync(manifestPath, "utf8")) as { files?: string[]; pages?: string[] })
  : {};
for (const entry of fs.readdirSync(client, { recursive: true, withFileTypes: true })) {
  if (!entry.isFile()) continue;
  const source = path.join(entry.parentPath, entry.name);
  const name = path.relative(client, source).replaceAll("\\", "/");
  const destination = path.join(output, name);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  if (name.endsWith(".html")) {
    const route =
      name === "404.html" ? "" : path.posix.dirname(name) === "." ? "" : path.posix.dirname(name);
    // Start normalizes Vite's relative base to /./ in its SSR manifest.
    // Relativize both its script tags and serialized manifest for portable Pages artifacts.
    fs.writeFileSync(
      destination,
      fs.readFileSync(source, "utf8").replaceAll("/./assets/", siteRoot(route) + "assets/"),
    );
  } else fs.copyFileSync(source, destination);
  ownedFiles.push(name);
}
for (const name of previous.files || previous.pages || []) {
  if (ownedFiles.includes(name)) continue;
  const stale = path.resolve(output, name);
  const inside = path.relative(output, stale);
  if (!inside || inside.startsWith("..") || path.isAbsolute(inside))
    throw new Error(`Unsafe previous documentation file: ${name}`);
  fs.rmSync(stale, { force: true });
}
// Tool engines and release catalogs remain generated assets; their pages are React routes.
for (const [directory, source] of [
  ["simulator", "simulator/web/static/style.css"],
  ["install", "docs/flasher/style.css"],
]) {
  fs.mkdirSync(path.join(output, directory), { recursive: true });
  fs.copyFileSync(path.join(root, source), path.join(output, directory, "style.css"));
}
for (const name of ["site.js", "search-index.js"])
  fs.rmSync(path.join(output, "assets", name), { force: true });
fs.writeFileSync(
  manifestPath,
  JSON.stringify(
    {
      framework: "tanstack-start",
      pages,
      files: ownedFiles,
      simulator: "simulator/",
      installer: "install/",
      sources: catalog.length,
    },
    null,
    2,
  ) + "\n",
);
console.log(
  `Built ${catalog.length} content pages, overview, simulator and installer with TanStack Start into ${output}`,
);
