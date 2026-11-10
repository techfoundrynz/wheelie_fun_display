import "../app.css";
import { useContentNavigation } from "../navigation";
import { useCallback, useEffect, useRef, useState } from "react";
import { createRootRoute, HeadContent, Outlet, Scripts, useMatches } from "@tanstack/react-router";
import type { DocPage } from "virtual:docs";
import { AppHeader } from "../components/AppHeader";
import { AppFooter } from "../components/AppFooter";
import { Simulator } from "../components/Simulator";
import { Navigation } from "../components/Navigation";
import { Search } from "../components/Search";
import { TableOfContents } from "../components/TableOfContents";
import { NotFound } from "../components/NotFound";
import { repository, siteRoot } from "../paths";

export const Route = createRootRoute({
  head: () => ({
    meta: [
      { charSet: "utf-8" },
      { name: "viewport", content: "width=device-width,initial-scale=1" },
      { name: "theme-color", content: "#080b0e" },
      { title: "Wheelie Fun Display — Ride, connect, make it yours" },
      {
        name: "description",
        content: "Guides, simulator and development notes for the VESC bike display.",
      },
    ],
  }),
  component: RootDocument,
  notFoundComponent: NotFound,
});
function RootDocument() {
  const matches = useMatches();
  const match = matches.at(-1);
  const home = match?.routeId === "/";
  const isSimulator = match?.routeId === "/simulator";
  const isInstaller = match?.routeId === "/install";
  const tool = isSimulator || isInstaller;
  const [simulatorVisited, setSimulatorVisited] = useState(isSimulator);
  useEffect(() => {
    if (isSimulator) setSimulatorVisited(true);
  }, [isSimulator]);
  const page = (match?.loaderData || null) as DocPage | null;
  const route = page?.route || (isSimulator ? "simulator" : isInstaller ? "install" : "");
  const root = siteRoot(route);
  const navigateContent = useContentNavigation(route);
  const [navOpen, setNavOpen] = useState(false);
  const closeNavigation = useCallback(() => setNavOpen(false), []);
  const dialog = useRef<HTMLDialogElement>(null);
  return (
    <html lang="en">
      <head>
        <style>
          {
            "html{background:#080b0e;color-scheme:dark}body{margin:0;background:#080b0e;color:#edf3f5}"
          }
        </style>
        <meta name="docs-root" content={root} />
        <link rel="icon" href={root + "assets/favicon.svg"} type="image/svg+xml" />
        <HeadContent />
      </head>
      <body
        onClick={navigateContent}
        data-site-root={root}
        className={
          (isSimulator
            ? "simulator-page"
            : isInstaller
              ? "flasher-page"
              : home
                ? "home-page"
                : "docs-page") + (navOpen ? " nav-open" : "")
        }
      >
        <a className="skip-link" href="#content">
          Skip to content
        </a>
        <AppHeader
          route={route}
          navOpen={navOpen}
          onToggleNavigation={() => setNavOpen((open) => !open)}
          onSearch={() => {
            closeNavigation();
            dialog.current?.showModal();
            dialog.current?.querySelector("input")?.focus();
          }}
        />
        {tool && (
          <div className="tool-navigation">
            <Navigation route={route} onNavigate={closeNavigation} />
          </div>
        )}
        <button
          className="nav-overlay"
          type="button"
          aria-label="Close navigation"
          onClick={closeNavigation}
        />
        {(simulatorVisited || isSimulator) && (
          <div className="tool-page page-content-vt" hidden={!isSimulator}>
            <Simulator active={isSimulator} />
          </div>
        )}
        {isInstaller && <Outlet />}
        {!tool && (
          <div className="site-layout">
            <Navigation route={route} onNavigate={closeNavigation} />
            <main
              id="content"
              tabIndex={-1}
              className={(home ? "home-main" : "article-main") + " page-content-vt"}
            >
              {!home && (
                <header className="article-header">
                  <div className="eyebrow">{page?.group || "Documentation"}</div>
                  <h1>{page?.title || "Page not found"}</h1>
                  <p>{page?.description || "This link does not point to a documentation page."}</p>
                  {page && (
                    <a className="source-link" href={repository + "/blob/main/" + page.source}>
                      View source <span aria-hidden="true">↗</span>
                    </a>
                  )}
                </header>
              )}
              <Outlet />
            </main>
            <TableOfContents page={page} />
          </div>
        )}
        <AppFooter />
        <Search dialog={dialog} closeNavigation={closeNavigation} />
        <Scripts />
      </body>
    </html>
  );
}
