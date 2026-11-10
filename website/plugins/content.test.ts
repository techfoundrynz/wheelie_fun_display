import { assert, test } from "vitest";
import { load } from "cheerio";
import { compilePage, readCatalog, rewriteUrl } from "./content.ts";

test("existing Markdown sources keep their routes, anchors and downloadable files", () => {
  const catalog = readCatalog();
  for (const page of catalog) {
    const compiled = compilePage(page, catalog);
    assert.ok(compiled.body);
    assert.strictEqual(
      new Set(compiled.headings.map((heading) => heading.id)).size,
      compiled.headings.length,
    );
    const $ = load(compiled.body);
    $("h2").each((_, heading) => {
      const link = $(heading).children("a.heading-anchor");
      assert.strictEqual(link.length, 1);
      assert.strictEqual(link.attr("href"), "#" + $(heading).attr("id"));
      assert.strictEqual(link.attr("aria-label"), "Link to " + $(heading).text());
      assert.ok(
        compiled.headings.some(
          (item) => item.id === $(heading).attr("id") && item.title === $(heading).text(),
        ),
      );
    });
  }
  const page = catalog.find((page) => page.route === "hardware/guition-43/case-mount")!;
  assert.strictEqual(
    rewriteUrl("../../docs/user-guide.md#tune", page, catalog),
    "../../../guide/#tune",
  );
  assert.strictEqual(
    rewriteUrl("rear-casing.step", page, catalog),
    "../../../cad/guition-43/rear-casing.step",
  );
  assert.throws(() => rewriteUrl("../../../../private.txt", page, catalog), /escapes repository/);
});

import { isCatalog, localFirmware } from "../src/tools/flasher.ts";
test("React flasher accepts only same-site firmware paths", () => {
  const base = new URL("https://example.test/wheelie_fun_display/install/");
  assert.strictEqual(
    localFirmware("firmware/board.bin", base),
    "https://example.test/wheelie_fun_display/install/firmware/board.bin",
  );
  for (const url of [
    "../board.bin",
    "https://other.test/firmware/board.bin",
    "javascript:alert(1)",
  ])
    assert.throws(() => localFirmware(url, base));
  assert.strictEqual(
    isCatalog({
      boards: [{ environment: "board", label: "Display", available: false }],
      version: null,
      channel: "release",
      release_url: null,
    }),
    true,
  );
  assert.strictEqual(isCatalog({ boards: [{ environment: 1 }] }), false);
});
