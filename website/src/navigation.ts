import { useRouter } from "@tanstack/react-router";
import type { MouseEvent } from "react";
import { pages } from "virtual:docs";
import { siteRoot } from "./paths";
const routes = new Set(["", "simulator", "install", ...pages.map((page) => page.route)]);
export function useContentNavigation(route: string) {
  const router = useRouter();
  return (event: MouseEvent<HTMLBodyElement>) => {
    if (
      event.defaultPrevented ||
      event.button !== 0 ||
      event.metaKey ||
      event.ctrlKey ||
      event.shiftKey ||
      event.altKey
    )
      return;
    const anchor = event.target instanceof Element ? event.target.closest("a") : null;
    if (!anchor || anchor.target || anchor.hasAttribute("download")) return;
    const url = new URL(anchor.href, location.href);
    if (url.origin !== location.origin || url.pathname === location.pathname) return;
    const base = new URL(siteRoot(route), location.href).pathname;
    if (!url.pathname.startsWith(base)) return;
    const slug = url.pathname.slice(base.length).replace(/\/$/, "");
    if (!routes.has(slug)) return;
    event.preventDefault();
    void router.navigate({
      to: slug ? "/" + slug + "/" : "/",
      hash: decodeURIComponent(url.hash.slice(1)),
      search: Object.fromEntries(url.searchParams),
    });
  };
}
