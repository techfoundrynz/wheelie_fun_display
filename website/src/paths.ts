export const repository = "https://github.com/techfoundrynz/wheelie_fun_display";
export function siteRoot(route = "") {
  return "../".repeat(route.split("/").filter(Boolean).length) || "./";
}
export function relative(target: string, route: string) {
  const from = route.split("/").filter(Boolean);
  const to = target.split("/").filter(Boolean);
  while (from.length && to.length && from[0] === to[0]) {
    from.shift();
    to.shift();
  }
  return "../".repeat(from.length) + (to.length ? to.join("/") + "/" : "") || "./";
}
