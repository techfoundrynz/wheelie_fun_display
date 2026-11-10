import { useMatches, useRouter } from "@tanstack/react-router";
import type { AnchorHTMLAttributes } from "react";
import type { DocPage } from "virtual:docs";
import { relative } from "../paths";

export function DocLink({
  to,
  onClick,
  ...props
}: { to: string } & Omit<AnchorHTMLAttributes<HTMLAnchorElement>, "href">) {
  const router = useRouter();
  const match = useMatches().at(-1);
  const page = match?.loaderData as DocPage | null;
  const route =
    page?.route ||
    (match?.routeId === "/simulator"
      ? "simulator"
      : match?.routeId === "/install"
        ? "install"
        : "");
  const href = relative(to, route);
  return (
    <a
      {...props}
      href={href}
      onClick={(event) => {
        onClick?.(event);
        if (
          event.defaultPrevented ||
          event.button !== 0 ||
          event.metaKey ||
          event.ctrlKey ||
          event.shiftKey ||
          event.altKey ||
          props.target === "_blank" ||
          props.download
        )
          return;
        event.preventDefault();
        void router.navigate({ to });
      }}
    />
  );
}
