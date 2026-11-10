import { createFileRoute } from "@tanstack/react-router";
import { Flasher } from "../components/Flasher";
export const Route = createFileRoute("/install")({
  head: () => ({ meta: [{ title: "Install firmware · Wheelie Fun Display" }] }),
  component: Flasher,
});
