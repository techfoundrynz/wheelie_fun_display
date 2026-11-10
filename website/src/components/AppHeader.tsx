import bike from "../../../docs/site/bike.svg?raw";
import { DocLink } from "./DocLink";
export function AppHeader({
  route,
  navOpen,
  onToggleNavigation,
  onSearch,
}: {
  route: string;
  navOpen: boolean;
  onToggleNavigation: () => void;
  onSearch: () => void;
}) {
  return (
    <header className="site-header sticky top-0 z-50 flex h-[var(--header)] items-center justify-between gap-6 border-b border-outline bg-page px-8 max-sm:px-4">
      <DocLink
        className="brand flex items-center gap-3 whitespace-nowrap text-lg font-bold text-ink"
        to="/"
        dangerouslySetInnerHTML={{ __html: bike + "<span>Wheelie Fun Display</span>" }}
      />
      <div className="header-actions">
        <nav
          className="app-primary-nav hidden items-center gap-[22px] text-[13px] min-[901px]:flex"
          aria-label="Site"
        >
          <DocLink
            to="/"
            aria-current={route !== "install" && route !== "simulator" ? "page" : undefined}
          >
            Documentation
          </DocLink>
          <DocLink to="/simulator/" aria-current={route === "simulator" ? "page" : undefined}>
            Simulator
          </DocLink>
          <DocLink to="/install/" aria-current={route === "install" ? "page" : undefined}>
            Install firmware
          </DocLink>
        </nav>
        <button
          className="search-trigger"
          type="button"
          aria-label="Search documentation"
          onClick={onSearch}
        >
          <span aria-hidden="true">⌕</span>
          <span>Search</span>
          <kbd>/</kbd>
        </button>
        <button
          className="menu-trigger"
          type="button"
          aria-label="Open navigation"
          aria-controls="site-navigation"
          aria-expanded={navOpen}
          onClick={onToggleNavigation}
        >
          ☰
        </button>
      </div>
    </header>
  );
}
