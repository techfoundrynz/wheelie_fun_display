import init, { WebSimulator } from "./pkg/wheelie_fun_display.js";

const status = document.querySelector("#status");
const canvas = document.querySelector("#display");
const controls = ["speed", "battery", "temperature", "pitch"];
const unit = { speed: " km/h", battery: "%", temperature: "°C", pitch: "°" };
let simulator;
let started;
let pointerDown = false;

function position(event) {
  const bounds = canvas.getBoundingClientRect();
  return [
    ((event.clientX - bounds.left) * canvas.width) / bounds.width,
    ((event.clientY - bounds.top) * canvas.height) / bounds.height,
  ];
}
function pointer(kind, event) {
  if (!simulator) return;
  simulator.pointer(kind, ...position(event), performance.now() - started);
}
function control(name, value) {
  const input = document.getElementById(name);
  input.value = value;
  document.getElementById(`${name}-value`).textContent =
    `${value}${unit[name]}`;
  simulator?.set_control(name, Number(value));
}
canvas.addEventListener("pointerdown", (event) => {
  event.preventDefault();
  canvas.focus();
  pointerDown = true;
  canvas.setPointerCapture(event.pointerId);
  pointer("down", event);
});
canvas.addEventListener("pointermove", (event) => {
  if (pointerDown) pointer("move", event);
});
canvas.addEventListener("pointerup", (event) => {
  if (pointerDown) pointer("up", event);
  pointerDown = false;
  if (canvas.hasPointerCapture(event.pointerId))
    canvas.releasePointerCapture(event.pointerId);
});
canvas.addEventListener("pointercancel", (event) => {
  pointer("cancel", event);
  pointerDown = false;
});
canvas.addEventListener(
  "wheel",
  (event) => {
    if (!simulator) return;
    event.preventDefault();
    simulator.scroll(...position(event), -event.deltaY);
  },
  { passive: false },
);
for (const name of controls)
  document
    .getElementById(name)
    .addEventListener("input", (event) => control(name, event.target.value));
document
  .getElementById("connected")
  .addEventListener("change", (event) =>
    simulator?.set_connected(event.target.checked),
  );
document
  .getElementById("display-model")
  .addEventListener("change", (event) =>
    simulator?.set_display_model(Number(event.target.value)),
  );

document
  .getElementById("integration")
  .addEventListener("change", (event) =>
    simulator?.set_integration(Number(event.target.value)),
  );
document
  .getElementById("orientation")
  .addEventListener("click", () =>
    simulator?.set_rotation((simulator.state("rotation") + 90) % 360),
  );
document
  .getElementById("stop")
  .addEventListener("click", () => control("speed", 0));
document.getElementById("ride").addEventListener("click", () => {
  control("speed", 24);
  control("pitch", 12);
});
document
  .getElementById("reset")
  .addEventListener("click", () => location.reload());
document
  .getElementById("pairing-demo")
  .addEventListener("click", () => simulator?.show_pairing_demo());
for (const button of document.querySelectorAll("[data-page]"))
  button.addEventListener("click", () =>
    simulator?.navigate(Number(button.dataset.page)),
  );

try {
  await init();
  simulator = new WebSimulator("display");
  window.wheelieFunDisplay = simulator;
  started = performance.now();
  const params = new URLSearchParams(location.search);
  if (params.get("display") === "guition") {
    simulator.set_display_model(1);
    document.getElementById("display-model").value = "1";
  }
  if (params.get("orientation") === "landscape") simulator.set_rotation(90);
  else if (params.has("orientation"))
    simulator.set_rotation(Number(params.get("orientation")));
  if (params.get("integration") === "wheel-e") simulator.set_integration(2);
  if (params.get("integration") === "vesc") simulator.set_integration(0);
  if (params.has("page")) simulator.navigate(Number(params.get("page")));
  // Screenshot exports use live-looking illustrative values. The surrounding
  // browser page retains its simulation disclosure in every mode.
  if (params.get("preview") === "docs")
    simulator.use_documentation_fixture(!params.has("page"));
  status.textContent = "Ready · real device UI, simulated hardware";
  document.getElementById("workspace").hidden = false;
  function frame(now) {
    try {
      simulator.frame(now - started);
      const landscape = simulator.state("landscape");
      document
        .querySelector(".display-frame")
        .classList.toggle("landscape", landscape);
      document
        .querySelector(".display-frame")
        .classList.toggle("large-display", simulator.state("display-model"));
      document.getElementById("orientation").textContent =
        `Rotate: ${simulator.state("rotation")}°`;
      requestAnimationFrame(frame);
    } catch (error) {
      status.textContent = `Simulator error: ${error.message || error}`;
      status.classList.add("error");
    }
  }
  requestAnimationFrame(frame);
} catch (error) {
  status.classList.add("error");
  status.textContent = `The simulator could not start. Reload this page in a current browser with WebAssembly enabled.\n${error.message || error}`;
}
