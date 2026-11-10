import { defineConfig } from "vite";
import { tanstackStart } from "@tanstack/react-start/plugin/vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import { docsContent, readCatalog } from "./plugins/content.ts";
import { prepareAssets } from "./plugins/assets.ts";

export default defineConfig(({ command }) => ({
  base: command === "serve" ? "/" : "./",
  publicDir: prepareAssets(process.env.DOCS_IMAGES),
  plugins: [
    docsContent(),
    tailwindcss(),
    tanstackStart({
      router: { basepath: "/" },
      prerender: {
        enabled: true,
        crawlLinks: false,
        autoStaticPathsDiscovery: false,
        failOnError: true,
      },
      pages: [
        { path: "/" },
        { path: "/simulator/" },
        { path: "/install/" },
        ...readCatalog().map((page) => ({ path: "/" + page.route + "/" })),
        { path: "/404.html", prerender: { outputPath: "/404.html" } },
      ],
    }),
    react(),
  ],
}));
