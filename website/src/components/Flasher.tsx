import { useEffect, useState } from "react";
import { isCatalog, localFirmware, type Catalog } from "../tools/flasher";
export function Flasher() {
  const [catalog, setCatalog] = useState<Catalog | null>(null);
  const [selected, setSelected] = useState("");
  const [base, setBase] = useState<URL | null>(null);
  const [ready, setReady] = useState(false);
  const [error, setError] = useState("");
  useEffect(() => {
    const controller = new AbortController();
    const installBase = new URL("./", location.href);
    setBase(installBase);
    void (async () => {
      try {
        const response = await fetch(new URL("catalog.json", installBase), {
          cache: "no-cache",
          signal: controller.signal,
        });
        if (!response.ok) throw new Error("Firmware catalog is unavailable.");
        const data: unknown = await response.json();
        if (!isCatalog(data)) throw new Error("Firmware catalog is invalid.");
        for (const board of data.boards)
          if (board.available) {
            localFirmware(board.manifest, installBase);
            localFirmware(board.download, installBase);
          }
        if (controller.signal.aborted) return;
        setCatalog(data);
        setSelected(data.boards[0]?.environment || "");
        const vendor = new URL("./vendor/esp-web-tools/install-button.js", installBase).href;
        await import(/* @vite-ignore */ vendor);
        await customElements.whenDefined("esp-web-install-button");
        if (!controller.signal.aborted) setReady(true);
      } catch (error) {
        if (!controller.signal.aborted)
          setError(
            `${error instanceof Error ? error.message : String(error)} See GitHub releases or the terminal installation guide.`,
          );
      }
    })();
    return () => controller.abort();
  }, []);
  const board = catalog?.boards.find((board) => board.environment === selected);
  const manifest = board?.available && base ? localFirmware(board.manifest, base) : undefined;
  const download = board?.available && base ? localFirmware(board.download, base) : undefined;
  const status =
    error ||
    (!catalog
      ? "Checking available installation images."
      : !board?.available
        ? catalog.version
          ? "This release does not include a browser installer for this display yet."
          : "No firmware installer release is available yet. Published releases will appear here automatically."
        : catalog.channel === "local"
          ? "Local build preview. This image has not been published as a release."
          : "Complete installation image, ready to install over USB.");
  return (
    <main
      id="content"
      className="flasher-main page-content-vt mx-auto w-full max-w-[1160px]"
      tabIndex={-1}
    >
      <section className="flasher-intro">
        <p className="eyebrow">USB INSTALLATION</p>
        <h1>Install firmware</h1>
        <p>
          Connect your display with a USB data cable, choose its model, and install Wheelie Fun
          Display from your browser.
        </p>
      </section>
      <div className="flasher-grid">
        <section className="install-card" aria-labelledby="install-title">
          <div className="install-card-heading">
            <span className="status-dot"></span>
            <span id="release-label">
              {catalog?.version
                ? `${catalog.channel === "local" ? "Local build" : "Firmware"} ${catalog.version}`
                : "Awaiting first release"}
            </span>
          </div>
          <h2 id="install-title">Choose your display</h2>
          <label htmlFor="board-select">Display model</label>
          <select
            id="board-select"
            disabled={!catalog?.boards.length}
            value={selected}
            onChange={(event) => setSelected(event.target.value)}
          >
            {catalog ? (
              catalog.boards.map((board) => (
                <option key={board.environment} value={board.environment}>
                  {board.label}
                </option>
              ))
            ) : (
              <option value="">Loading supported displays…</option>
            )}
          </select>
          <p id="firmware-status" className="firmware-status" role="status">
            {status}
          </p>
          <p id="compatibility" className="compatibility" hidden={!board?.compatibility}>
            {board?.compatibility}
          </p>
          <div id="install-available" hidden={!board?.available || Boolean(error)}>
            <dl className="firmware-summary">
              <div>
                <dt>Display</dt>
                <dd id="firmware-model">{board?.label}</dd>
              </div>
              <div>
                <dt>Firmware</dt>
                <dd id="firmware-version">{board?.version}</dd>
              </div>
              <div>
                <dt>Chip</dt>
                <dd id="firmware-chip">{board?.chip_family}</dd>
              </div>
            </dl>
            <esp-web-install-button id="installer" manifest={manifest}>
              <button
                id="install-action"
                className="button primary install-action"
                slot="activate"
                disabled={!ready || !board?.available || Boolean(error)}
              >
                Connect &amp; install
              </button>
              <span className="capability-message" slot="unsupported">
                This browser does not provide Web Serial. Open this page in a supported desktop
                browser, such as Chrome or Edge.
              </span>
              <span className="capability-message" slot="not-allowed">
                USB installation needs HTTPS or localhost. Open the published site using HTTPS.
              </span>
            </esp-web-install-button>
            <details className="download-details">
              <summary>Download and integrity</summary>
              <p>
                <a id="firmware-download" href={download} download>
                  Download the complete USB image
                </a>
              </p>
              <span className="hash-label">SHA256</span>
              <code id="firmware-sha">{board?.sha256}</code>
            </details>
          </div>
          <a
            id="release-link"
            className="release-link"
            href={
              catalog?.release_url ||
              "https://github.com/techfoundrynz/wheelie_fun_display/releases"
            }
          >
            View releases on GitHub <span aria-hidden="true">↗</span>
          </a>
        </section>
        <section className="install-guide" aria-labelledby="before-title">
          <p className="eyebrow">BEFORE YOU CONNECT</p>
          <h2 id="before-title">Installation steps</h2>
          <ol className="install-steps">
            <li>
              <span>1</span>
              <div>
                <h3>Choose the exact model</h3>
                <p>
                  Use the display name printed on your board. The installer checks the connected
                  chip family. For GUITION, confirm a P4 revision below 3.0 and compatible C6 radio
                  firmware before installing.
                </p>
              </div>
            </li>
            <li>
              <span>2</span>
              <div>
                <h3>Connect over USB</h3>
                <p>
                  Use a data cable and close any serial monitor. Select the display’s USB port when
                  your browser asks.
                </p>
              </div>
            </li>
            <li>
              <span>3</span>
              <div>
                <h3>Install, then restart</h3>
                <p>
                  Keep the display powered until installation finishes. Use its touchscreen to
                  connect Wi-Fi and your bike afterward.
                </p>
              </div>
            </li>
          </ol>
          <aside className="install-note">
            <strong>Already running Wheelie Fun Display?</strong>
            <p>
              Use <b>Settings → Firmware updates</b> for normal updates. A complete USB installation
              replaces the flash layout and can reset saved settings.
            </p>
          </aside>
          <p className="setup-help">
            Port missing? Check your USB data cable and follow your board’s download-mode
            instructions. For Waveshare, hold <b>BOOT</b>, tap <b>RESET</b>, then release{" "}
            <b>BOOT</b>. See{" "}
            <a href="../getting-started/#1-install-and-power-up">USB installation instructions</a>{" "}
            for board setup and first connection.
          </p>
        </section>
      </div>
    </main>
  );
}
