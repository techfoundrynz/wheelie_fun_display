import { useEffect, useRef, useState, type RefObject } from "react";
import { DocLink as Link } from "./DocLink";
import { pages } from "virtual:docs";

export function Search({
  dialog,
  closeNavigation,
}: {
  dialog: RefObject<HTMLDialogElement | null>;
  closeNavigation: () => void;
}) {
  const input = useRef<HTMLInputElement>(null);
  const [query, setQuery] = useState("");
  useEffect(() => {
    function onKey(event: KeyboardEvent) {
      const target = document.activeElement;
      const typing =
        target instanceof HTMLElement &&
        (/INPUT|TEXTAREA|SELECT/.test(target.tagName) || target.isContentEditable);
      if (
        (event.key === "/" && !typing) ||
        ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k")
      ) {
        event.preventDefault();
        closeNavigation();
        dialog.current?.showModal();
        input.current?.focus();
      }
      if (event.key === "Escape") {
        dialog.current?.close();
        closeNavigation();
      }
    }
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [dialog, closeNavigation]);
  const terms = query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
  const matches = terms.length
    ? pages
        .filter((page) =>
          terms.every((term) =>
            (page.title + " " + page.description + " " + page.search)
              .toLocaleLowerCase()
              .includes(term),
          ),
        )
        .sort(
          (a, b) =>
            Number(terms.some((term) => b.title.toLocaleLowerCase().includes(term))) -
            Number(terms.some((term) => a.title.toLocaleLowerCase().includes(term))),
        )
        .slice(0, 8)
    : [];
  return (
    <dialog
      ref={dialog}
      className="search-dialog"
      aria-label="Search documentation"
      onClick={(event) => {
        if (event.target === dialog.current) dialog.current?.close();
      }}
    >
      <form method="dialog">
        <button className="close-search" aria-label="Close search">
          ×
        </button>
      </form>
      <label htmlFor="docs-search">Search Wheelie Fun Display</label>
      <input
        ref={input}
        id="docs-search"
        type="search"
        placeholder="Connection, PIN, build…"
        autoComplete="off"
        value={query}
        onChange={(event) => setQuery(event.target.value)}
      />
      <div className="search-results" aria-live="polite">
        {!terms.length
          ? "Try “pairing”, “CAN” or “build”."
          : matches.length
            ? matches.map((page) => (
                <Link
                  key={page.route}
                  to={"/" + page.route + "/"}
                  onClick={() => dialog.current?.close()}
                >
                  <strong>{page.title}</strong>
                  <span>{page.description}</span>
                </Link>
              ))
            : "No matching guide. Try a shorter phrase."}
      </div>
      <p className="search-hint">Search the guides and implementation notes. Press Esc to close.</p>
    </dialog>
  );
}
