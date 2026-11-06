(() => {
  const menu = document.querySelector(".menu-trigger");
  const closeNav = () => {
    document.body.classList.remove("nav-open");
    menu?.setAttribute("aria-expanded", "false");
  };
  menu?.addEventListener("click", () => {
    const open = document.body.classList.toggle("nav-open");
    menu.setAttribute("aria-expanded", String(open));
  });
  document.querySelector(".nav-overlay")?.addEventListener("click", closeNav);
  const dialog = document.querySelector(".search-dialog");
  const input = document.querySelector("#docs-search");
  const results = document.querySelector(".search-results");
  const root = new URL(document.body.dataset.siteRoot, location.href);
  const render = () => {
    const terms = input.value
      .toLocaleLowerCase()
      .trim()
      .split(/\s+/)
      .filter(Boolean);
    results.replaceChildren();
    if (!terms.length) {
      results.textContent = "Try “pairing”, “CAN” or “build”.";
      return;
    }
    const matches = (window.WFD_DOCS_SEARCH || []).filter((page) =>
      terms.every((term) =>
        (page.title + " " + page.description + " " + page.text)
          .toLocaleLowerCase()
          .includes(term),
      ),
    );
    matches.sort(
      (a, b) =>
        Number(
          terms.some((term) => b.title.toLocaleLowerCase().includes(term)),
        ) -
        Number(
          terms.some((term) => a.title.toLocaleLowerCase().includes(term)),
        ),
    );
    for (const page of matches.slice(0, 8)) {
      const link = document.createElement("a");
      link.href = new URL(page.url, root).href;
      const title = document.createElement("strong");
      title.textContent = page.title;
      const description = document.createElement("span");
      description.textContent = page.description;
      link.append(title, description);
      results.append(link);
    }
    if (!matches.length)
      results.textContent = "No matching guide. Try a shorter phrase.";
  };
  const openSearch = () => {
    closeNav();
    dialog.showModal();
    render();
    input.focus();
  };
  document
    .querySelector(".search-trigger")
    ?.addEventListener("click", openSearch);
  input?.addEventListener("input", render);
  dialog?.addEventListener("click", (event) => {
    if (event.target === dialog) dialog.close();
  });
  document.addEventListener("keydown", (event) => {
    const typing =
      /INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName || "") ||
      document.activeElement?.isContentEditable;
    if (
      (event.key === "/" && !typing) ||
      ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k")
    ) {
      event.preventDefault();
      openSearch();
    }
    if (event.key === "Escape") {
      if (dialog.open) {
        event.preventDefault();
        dialog.close();
      }
      closeNav();
    }
  });
  const observer = new IntersectionObserver(
    (entries) => {
      for (const entry of entries)
        if (entry.isIntersecting)
          document
            .querySelectorAll(".page-toc a")
            .forEach((link) =>
              link.classList.toggle(
                "current-heading",
                link.hash === "#" + entry.target.id,
              ),
            );
    },
    { rootMargin: "-80px 0px -65% 0px" },
  );
  document
    .querySelectorAll(".md-content h2[id], .md-content h3[id]")
    .forEach((heading) => observer.observe(heading));
})();
