import { useEffect, useState } from "react";
import type { DocPage } from "virtual:docs";
export function TableOfContents({ page }: { page: DocPage | null }) {
  const [current, setCurrent] = useState("");
  useEffect(() => {
    setCurrent("");
    const observer = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) if (entry.isIntersecting) setCurrent(entry.target.id);
      },
      { rootMargin: "-80px 0px -65% 0px" },
    );
    document
      .querySelectorAll(".md-content h2[id], .md-content h3[id]")
      .forEach((heading) => observer.observe(heading));
    return () => observer.disconnect();
  }, [page?.route]);
  return (
    <aside className="page-toc" aria-label="On this page">
      {page && page.headings.length > 0 && (
        <>
          <span>On this page</span>
          <ul>
            {page.headings.map((heading) => (
              <li key={heading.id} className={heading.level === 3 ? "toc-child" : undefined}>
                <a
                  href={"#" + heading.id}
                  className={current === heading.id ? "current-heading" : undefined}
                >
                  {heading.title}
                </a>
              </li>
            ))}
          </ul>
        </>
      )}
    </aside>
  );
}
