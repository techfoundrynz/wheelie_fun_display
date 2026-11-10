import { DocLink } from "./DocLink";
import { repository } from "../paths";
export function AppFooter() {
  return (
    <footer
      className="app-footer mx-auto flex w-full max-w-[1500px] flex-wrap justify-between gap-4 border-t border-outline px-8 py-6 text-left text-xs max-sm:flex-col max-sm:px-5"
      id="app-footer"
    >
      <span>Wheelie Fun Display · Open-source VESC bike display</span>
      <nav aria-label="Footer">
        <a href={repository + "/blob/main/LICENSE"}>GPL-3.0-only</a>
        <DocLink to="/reference/notices/">Notices</DocLink>
        <a href={repository}>GitHub</a>
      </nav>
    </footer>
  );
}
