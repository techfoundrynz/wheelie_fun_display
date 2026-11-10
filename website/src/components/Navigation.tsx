import { DocLink as Link } from "./DocLink";
import { pages } from "virtual:docs";
import { repository } from "../paths";

export function Navigation({ route, onNavigate }: { route: string; onNavigate: () => void }) {
  const groups = Map.groupBy(pages, (page) => page.group);
  const routes = new Set(pages.map((page) => page.route));
  return (
    <aside className="sidebar" id="site-navigation" aria-label="Documentation navigation">
      <div className="nav-group app-mobile-nav">
        <span>Explore</span>
        <Link to="/" onClick={onNavigate}>
          Documentation
        </Link>
        <Link to="/simulator/" onClick={onNavigate}>
          Simulator
        </Link>
        <Link to="/install/" onClick={onNavigate}>
          Install firmware
        </Link>
      </div>
      <Link className="overview-link" to="/" onClick={onNavigate}>
        Overview
      </Link>
      {[...groups].map(([name, group]) => (
        <div className="nav-group" key={name}>
          <span>{name}</span>
          {group.map((page) => (
            <Link
              key={page.route}
              to={"/" + page.route + "/"}
              className={
                routes.has(page.route.split("/").slice(0, -1).join("/")) ? "nav-child" : undefined
              }
              aria-current={page.route === route ? "page" : undefined}
              onClick={onNavigate}
            >
              {page.source.startsWith("cad/")
                ? "Case & mount (CAD)"
                : page.source.startsWith("pcb/")
                  ? "CAN breakout (PCB)"
                  : page.title}
            </Link>
          ))}
          {name === "Start here" && (
            <Link to="/install/" onClick={onNavigate}>
              Install firmware
            </Link>
          )}
        </div>
      ))}
      <a className="github-link" href={repository}>
        Source on GitHub <span aria-hidden="true">↗</span>
      </a>
    </aside>
  );
}
