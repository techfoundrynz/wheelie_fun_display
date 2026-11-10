import { createFileRoute } from "@tanstack/react-router";
export const Route = createFileRoute("/simulator")({
  head: () => ({ meta: [{ title: "Wheelie Fun Display · Interactive simulator" }] }),
  component: () => null,
});
