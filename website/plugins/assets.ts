import fs from "node:fs";
import path from "node:path";
import { root } from "./content.ts";

export function prepareAssets(images = path.join(root, ".cache/pages/docs-images")) {
  const output = path.join(root, ".cache/docs-public");
  fs.mkdirSync(path.join(output, "assets/fonts"), { recursive: true });
  fs.copyFileSync(path.join(root, "docs/site/style.css"), path.join(output, "assets/style.css"));
  for (const name of ["Roboto.ttf", "Roboto-OFL.txt"])
    fs.copyFileSync(path.join(root, "ui/fonts", name), path.join(output, "assets/fonts", name));
  fs.cpSync(path.join(root, "docs/images"), path.join(output, "assets/images"), {
    recursive: true,
  });
  const manifest = path.join(images, "manifest.json");
  if (fs.existsSync(manifest)) {
    const { images: names } = JSON.parse(fs.readFileSync(manifest, "utf8"));
    for (const name of names) {
      if (path.basename(name) !== name) throw new Error(`Invalid screenshot: ${name}`);
      fs.copyFileSync(path.join(images, name), path.join(output, "assets/images", name));
    }
    fs.copyFileSync(manifest, path.join(output, "assets/images/manifest.json"));
  } else if (process.env.DOCS_BUILD === "1")
    throw new Error("Generate documentation screenshots before building the production site.");
  const downloads: [string, string[]][] = [
    ["cad", [".step", ".f3d", ".f3z"]],
    ["pcb", [".epro2"]],
  ];
  for (const [directory, extensions] of downloads) {
    for (const entry of fs.readdirSync(path.join(root, directory), {
      recursive: true,
      withFileTypes: true,
    })) {
      if (!entry.isFile() || !extensions.includes(path.extname(entry.name))) continue;
      const source = path.join(entry.parentPath, entry.name);
      const destination = path.join(output, path.relative(root, source));
      fs.mkdirSync(path.dirname(destination), { recursive: true });
      fs.copyFileSync(source, destination);
    }
  }
  const icon = fs.readFileSync(path.join(root, "docs/site/bike.svg"), "utf8").trim();
  const [x, y, width, height] = /viewBox="([^"]+)"/.exec(icon)![1].split(" ").map(Number);
  const fit = `translate(16 16) scale(${(28.8 / Math.max(width, height)).toFixed(9)}) translate(${-(x + width / 2)} ${-(y + height / 2)})`;
  const shape = icon.slice(icon.indexOf(">") + 1, icon.lastIndexOf("</svg>")).trim();
  const favicon = fs
    .readFileSync(path.join(root, "docs/site/favicon.svg.in"), "utf8")
    .replace("<!-- WFD_BIKE_FIT -->", fit)
    .replace("<!-- WFD_BIKE_SHAPE -->", shape);
  fs.writeFileSync(path.join(output, "assets/favicon.svg"), favicon);
  // Preserve upstream license text for the web dependencies shipped by this build.
  const notices = new Map<string, string>();
  const store = path.join(root, "node_modules/.pnpm");
  for (const bucket of fs.readdirSync(store, { withFileTypes: true })) {
    if (!bucket.isDirectory()) continue;
    const modules = path.join(store, bucket.name, "node_modules");
    if (!fs.existsSync(modules)) continue;
    const candidates: string[] = [];
    for (const entry of fs.readdirSync(modules, { withFileTypes: true })) {
      if (!entry.isDirectory()) continue;
      const directory = path.join(modules, entry.name);
      if (entry.name.startsWith("@")) {
        for (const child of fs.readdirSync(directory, { withFileTypes: true }))
          if (child.isDirectory()) candidates.push(path.join(directory, child.name));
      } else candidates.push(directory);
    }
    for (const directory of candidates) {
      if (!fs.existsSync(path.join(directory, "package.json"))) continue;
      const metadata = JSON.parse(
        fs.readFileSync(path.join(directory, "package.json"), "utf8"),
      ) as { name: string; version: string };
      const texts = fs
        .readdirSync(directory, { withFileTypes: true })
        .filter(
          (file) => file.isFile() && /^(licen[cs]e|copying|notice)(?:[._-]|$)/i.test(file.name),
        )
        .map((file) => file.name + "\n" + fs.readFileSync(path.join(directory, file.name), "utf8"));
      if (texts.length) notices.set(metadata.name + "@" + metadata.version, texts.join("\n\n"));
    }
  }
  fs.writeFileSync(
    path.join(output, "assets/web-dependency-notices.txt"),
    "Documentation runtime and build dependency license notices\n\n" +
      [...notices]
        .sort(([a], [b]) => a.localeCompare(b))
        .map(([name, text]) => name + "\n" + text)
        .join("\n\n---\n\n"),
  );
  fs.writeFileSync(path.join(output, ".nojekyll"), "");
  return output;
}
