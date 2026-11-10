import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import MarkdownIt from "markdown-it";
import { load } from "cheerio";
import { z } from "zod";
import type { Plugin } from "vite";
import { headingAnchorIcon } from "../src/heading-anchor.ts";

export const root = path.resolve(fileURLToPath(new URL("../..", import.meta.url)));
import { repository, relative, siteRoot } from "../src/paths.ts";
export { repository, relative, siteRoot };
const schema = z.array(
  z.object({
    source: z.string().min(1),
    route: z.string().regex(/^[a-z0-9]+(?:[/-][a-z0-9]+)*$/),
    title: z.string().min(1),
    group: z.string().min(1),
    description: z.string().min(1),
  }),
);
export type PageMeta = z.infer<typeof schema>[number];
export function readCatalog() {
  const pages = schema.parse(
    JSON.parse(fs.readFileSync(path.join(root, "website/content.json"), "utf8")),
  );
  for (const key of ["route", "source"] as const) {
    if (new Set(pages.map((page) => page[key])).size !== pages.length)
      throw new Error(`Duplicate content ${key}`);
  }
  for (const page of pages) {
    if (!inside(path.resolve(root, page.source), root))
      throw new Error(`Content source escapes repository: ${page.source}`);
    if (!fs.statSync(path.resolve(root, page.source)).isFile())
      throw new Error(`Missing content: ${page.source}`);
  }
  return pages;
}
function inside(target: string, parent: string) {
  const relative = path.relative(parent, target);
  return relative === "" || (!relative.startsWith("..") && !path.isAbsolute(relative));
}
export function rewriteUrl(url: string, page: PageMeta, catalog: PageMeta[]) {
  if (url.startsWith("https://techfoundrynz.github.io/wheelie_fun_display/install/"))
    return relative("install", page.route) + url.split("/install/")[1];
  if (/^(?:[a-z][a-z0-9+.-]*:|\/\/|#)/i.test(url)) return url;
  const match = /^([^?#]*)(.*)$/.exec(url)!;
  if (!match[1]) return url;
  const target = path.resolve(root, path.dirname(page.source), decodeURIComponent(match[1]));
  if (!inside(target, root)) throw new Error(`Documentation link escapes repository: ${url}`);
  const source = path.relative(root, target).replaceAll("\\", "/");
  const known = catalog.find((item) => item.source === source);
  let result;
  if (known || source === "README.md") result = relative(known?.route || "", page.route);
  else if (["simulator", "ci", "install"].includes(source)) result = relative(source, page.route);
  else if (source === "assets/web-dependency-notices.txt") result = siteRoot(page.route) + source;
  else if (source.startsWith("docs/images/"))
    result = siteRoot(page.route) + "assets/images/" + path.basename(source);
  else if (
    (source.startsWith("cad/") && /\.(step|f3d|f3z)$/.test(source)) ||
    (source.startsWith("pcb/") && source.endsWith(".epro2"))
  )
    result = siteRoot(page.route) + source;
  else result = repository + "/blob/main/" + source;
  return result + match[2];
}
export function compilePage(page: PageMeta, catalog: PageMeta[]) {
  const text = fs.readFileSync(path.resolve(root, page.source), "utf8").replace(/^# [^\n]+\n+/, "");
  const $ = load(new MarkdownIt({ html: true }).render(text), null, false);
  const used = new Set();
  const headings: { id: string; title: string; level: number }[] = [];
  $("h1,h2,h3,h4,h5,h6").each((_, element) => {
    const heading = $(element);
    // Preserve Python-Markdown's published anchors, including duplicate suffixes.
    const slug = heading
      .text()
      .normalize("NFKD")
      .replace(/[^\u0020-\u007e]/g, "")
      .toLowerCase()
      .replace(/[^\w\s-]/g, "")
      .trim()
      .replace(/[\s-]+/g, "-");
    let id = slug;
    let index = 0;
    while (used.has(id)) id = `${slug}_${++index}`;
    used.add(id);
    heading.attr("id", id);
    if (["h2", "h3"].includes(element.tagName))
      headings.push({ id, title: heading.text(), level: Number(element.tagName[1]) });
    if (element.tagName === "h2") {
      heading.append(
        $('<a class="heading-anchor"></a>')
          .attr("href", "#" + id)
          .attr("aria-label", "Link to " + heading.text())
          .attr("title", "Link to " + heading.text())
          .append(headingAnchorIcon),
      );
    }
  });
  $("[href], [src]").each((_, element) => {
    for (const attribute of ["href", "src"]) {
      const value = $(element).attr(attribute);
      if (value) $(element).attr(attribute, rewriteUrl(value, page, catalog));
    }
  });
  $("table").wrap('<div class="table-scroll" tabindex="0"></div>');
  const search = $.text().replace(/\s+/g, " ").trim().slice(0, 7000);
  return { ...page, body: $.html(), headings, search };
}
export function docsContent(): Plugin {
  let catalog = readCatalog();
  const main = "\0virtual:docs";
  const prefix = "\0virtual:doc/";
  return {
    name: "wheelie-docs-content",
    resolveId(id) {
      if (id === "virtual:docs") return main;
      if (id.startsWith("virtual:doc/")) return "\0" + id;
    },
    load(id) {
      if (id === main) {
        const metadata = catalog.map((page) => ({
          ...page,
          search: compilePage(page, catalog).search,
        }));
        const loaders = catalog
          .map(
            (page) =>
              `${JSON.stringify(page.route)}:()=>import(${JSON.stringify("virtual:doc/" + page.route)})`,
          )
          .join(",");
        return `export const pages=${JSON.stringify(metadata)};const loaders={${loaders}};export async function loadDoc(route){return loaders[route] ? (await loaders[route]()).default : null}`;
      }
      if (id.startsWith(prefix)) {
        const page = catalog.find((page) => page.route === id.slice(prefix.length));
        if (!page) throw new Error(`Unknown documentation route: ${id}`);
        this.addWatchFile(path.resolve(root, page.source));
        return `export default ${JSON.stringify(compilePage(page, catalog))}`;
      }
    },
    configureServer(server) {
      server.middlewares.use((request, response, next) => {
        const pathname = decodeURIComponent(
          new URL(request.url || "/", "http://localhost").pathname,
        );
        let source: string | undefined;
        if (pathname === "/assets/style.css") source = path.join(root, "docs/site/style.css");
        else if (pathname === "/simulator/style.css")
          source = path.join(root, "simulator/web/static/style.css");
        else if (pathname === "/install/style.css")
          source = path.join(root, "docs/flasher/style.css");
        else if (
          /^\/(simulator\/(pkg\/|build\.json)|install\/(catalog\.json|firmware\/|vendor\/)|ci\/)/.test(
            pathname,
          )
        ) {
          const candidate = path.resolve(root, ".cache/pages/site", "." + pathname);
          if (inside(candidate, path.join(root, ".cache/pages/site"))) source = candidate;
        }
        if (!source || !fs.existsSync(source) || !fs.statSync(source).isFile()) {
          next();
          return;
        }
        const types: Record<string, string> = {
          ".js": "text/javascript",
          ".json": "application/json",
          ".wasm": "application/wasm",
          ".css": "text/css",
        };
        response.setHeader(
          "Content-Type",
          types[path.extname(source)] || "application/octet-stream",
        );
        fs.createReadStream(source).pipe(response);
      });
      server.watcher.add([
        path.join(root, "website/content.json"),
        ...catalog.map((page) => path.resolve(root, page.source)),
      ]);
    },
    handleHotUpdate({ file, server }) {
      const changedFile = path.resolve(file);
      if (
        changedFile === path.join(root, "website/content.json") ||
        catalog.some((page) => path.resolve(root, page.source) === changedFile)
      ) {
        catalog = readCatalog();
        server.moduleGraph.invalidateAll();
        server.ws.send({ type: "full-reload" });
        return [];
      }
    },
  };
}
