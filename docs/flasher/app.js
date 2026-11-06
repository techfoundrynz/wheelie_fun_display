const select = document.getElementById("board-select");
const status = document.getElementById("firmware-status");
const available = document.getElementById("install-available");
const button = document.getElementById("install-action");
const installer = document.getElementById("installer");
let catalog;
let ready = false;

function localFirmware(path) {
  const url = new URL(path, location.href);
  const directory = new URL("./firmware/", location.href);
  if (
    url.origin !== directory.origin ||
    !url.pathname.startsWith(directory.pathname)
  ) {
    throw new Error("Installation files must come from this site.");
  }
  return url.href;
}

function update() {
  const board = catalog.boards.find(
    (item) => item.environment === select.value,
  );
  button.disabled = true;
  available.hidden = !board?.available;
  const compatibility = document.getElementById("compatibility");
  compatibility.textContent = board?.compatibility || "";
  compatibility.hidden = !board?.compatibility;
  if (!board?.available) {
    status.textContent = catalog.version
      ? "This release does not include a browser installer for this display yet."
      : "No firmware installer release is available yet. Published releases will appear here automatically.";
    return;
  }
  installer.setAttribute("manifest", localFirmware(board.manifest));
  document.getElementById("firmware-model").textContent = board.label;
  document.getElementById("firmware-version").textContent = board.version;
  document.getElementById("firmware-chip").textContent = board.chip_family;
  document.getElementById("firmware-sha").textContent = board.sha256;
  document.getElementById("firmware-download").href = localFirmware(
    board.download,
  );
  status.textContent =
    catalog.channel === "local"
      ? "Local build preview. This image has not been published as a release."
      : "Complete installation image, ready to install over USB.";
  button.disabled = !ready;
}

select.addEventListener("change", update);

try {
  const response = await fetch("catalog.json", { cache: "no-cache" });
  if (!response.ok) throw new Error("Firmware catalog is unavailable.");
  catalog = await response.json();
  select.replaceChildren(
    ...catalog.boards.map((board) => {
      const option = document.createElement("option");
      option.value = board.environment;
      option.textContent = board.label;
      return option;
    }),
  );
  select.disabled = !catalog.boards.length;
  document.getElementById("release-label").textContent = catalog.version
    ? `${catalog.channel === "local" ? "Local build" : "Firmware"} ${catalog.version}`
    : "Awaiting first release";
  if (catalog.release_url)
    document.getElementById("release-link").href = catalog.release_url;
  update();
  await import("./vendor/esp-web-tools/install-button.js");
  await customElements.whenDefined("esp-web-install-button");
  ready = true;
  update();
} catch (error) {
  available.hidden = true;
  status.textContent = `${error.message} See GitHub releases or the terminal installation guide.`;
}
