export interface Board {
  environment: string;
  label: string;
  available: boolean;
  compatibility?: string;
  manifest?: string;
  download?: string;
  version?: string;
  chip_family?: string;
  sha256?: string;
}
export interface Catalog {
  version: string | null;
  channel: string;
  release_url: string | null;
  boards: Board[];
}
export function isCatalog(value: unknown): value is Catalog {
  if (!value || typeof value !== "object" || !("boards" in value) || !Array.isArray(value.boards))
    return false;
  return value.boards.every((board: unknown) =>
    Boolean(
      board &&
      typeof board === "object" &&
      "environment" in board &&
      typeof board.environment === "string" &&
      "label" in board &&
      typeof board.label === "string" &&
      "available" in board &&
      typeof board.available === "boolean",
    ),
  );
}

export function localFirmware(path: string | undefined, base: URL) {
  if (!path) throw new Error("Missing installation file.");
  const url = new URL(path, base);
  const directory = new URL("./firmware/", base);
  if (url.origin !== directory.origin || !url.pathname.startsWith(directory.pathname))
    throw new Error("Installation files must come from this site.");
  return url.href;
}
