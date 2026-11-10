import { createRouter } from "@tanstack/react-router";
import { routeTree } from "./routeTree.gen";

export function getRouter() {
  const basepath =
    typeof document === "undefined"
      ? "/"
      : new URL(
          document.querySelector('meta[name="docs-root"]')?.getAttribute("content") || "./",
          location.href,
        ).pathname;
  return createRouter({
    routeTree,
    basepath: "/",
    rewrite:
      basepath === "/"
        ? undefined
        : {
            input: ({ url }) => {
              const prefix = basepath.replace(/\/$/, "");
              if (url.pathname === prefix || url.pathname.startsWith(prefix + "/"))
                url.pathname = url.pathname.slice(prefix.length) || "/";
              return url;
            },
            output: ({ url }) => {
              url.pathname = basepath.replace(/\/$/, "") + url.pathname;
              return url;
            },
          },
    scrollRestoration: true,
    trailingSlash: "always",
    defaultViewTransition: {
      types: ({ fromLocation, toLocation }) => {
        if (
          typeof window !== "undefined" &&
          window.matchMedia("(prefers-reduced-motion: reduce)").matches
        )
          return false;
        // An empty type list still creates snapshots and the browser's default fade.
        if (fromLocation?.pathname === toLocation.pathname) return false;
        const from = fromLocation?.pathname.split("/").filter(Boolean).length || 0;
        const to = toLocation.pathname.split("/").filter(Boolean).length;
        return [to < from ? "backward" : "forward"];
      },
    },
  });
}
declare module "@tanstack/react-router" {
  interface Register {
    router: ReturnType<typeof getRouter>;
  }
}
