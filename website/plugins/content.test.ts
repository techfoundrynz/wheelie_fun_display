import { test } from "node:test";
import assert from "node:assert/strict";
import { compilePage, readCatalog, rewriteUrl } from "./content.ts";

test("existing Markdown sources keep their routes, anchors and downloadable files", () => {
  const catalog = readCatalog();
  for (const page of catalog) {
    const compiled = compilePage(page, catalog);
    assert.ok(compiled.body);
    assert.equal(
      new Set(compiled.headings.map((heading) => heading.id)).size,
      compiled.headings.length,
    );
  }
  const page = catalog.find((page) => page.route === "hardware/guition-43/case-mount")!;
  assert.equal(rewriteUrl("../../docs/user-guide.md#tune", page, catalog), "../../../guide/#tune");
  assert.equal(
    rewriteUrl("rear-casing.step", page, catalog),
    "../../../cad/guition-43/rear-casing.step",
  );
  assert.throws(() => rewriteUrl("../../../../private.txt", page, catalog), /escapes repository/);
});

import { isCatalog, localFirmware } from "../src/tools/flasher.ts";
test("React flasher accepts only same-site firmware paths", () => {
  const base = new URL("https://example.test/wheelie_fun_display/install/");
  assert.equal(
    localFirmware("firmware/board.bin", base),
    "https://example.test/wheelie_fun_display/install/firmware/board.bin",
  );
  for (const url of [
    "../board.bin",
    "https://other.test/firmware/board.bin",
    "javascript:alert(1)",
  ])
    assert.throws(() => localFirmware(url, base));
  assert.equal(
    isCatalog({
      boards: [{ environment: "board", label: "Display", available: false }],
      version: null,
      channel: "release",
      release_url: null,
    }),
    true,
  );
  assert.equal(isCatalog({ boards: [{ environment: 1 }] }), false);
});
