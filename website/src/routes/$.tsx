import { createFileRoute, notFound } from "@tanstack/react-router";
import { loadDoc } from "virtual:docs";
import { NotFound } from "../components/NotFound";
export const Route = createFileRoute("/$")({
  loader: async ({ params }) => {
    const route = (params._splat || "").replace(/\/$/, "");
    if (route === "404.html") return null;
    const page = await loadDoc(route);
    if (!page) throw notFound();
    return page;
  },
  head: ({ loaderData }) => ({
    meta: [
      {
        title: loaderData
          ? loaderData.title + " · Wheelie Fun Display"
          : "Page not found · Wheelie Fun Display",
      },
      {
        name: "description",
        content: loaderData?.description || "This link does not point to a documentation page.",
      },
    ],
  }),
  component: Article,
});
function Article() {
  const page = Route.useLoaderData();
  return page ? (
    <article className="md-content" dangerouslySetInnerHTML={{ __html: page.body }} />
  ) : (
    <NotFound />
  );
}
