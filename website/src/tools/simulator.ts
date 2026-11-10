export interface Simulator {
  frame(milliseconds: number): void;
  pointer(kind: string, x: number, y: number, time: number): void;
  scroll(x: number, y: number, delta: number): void;
  set_control(name: string, value: number): void;
  set_connected(value: boolean): void;
  set_display_model(value: number): void;
  set_integration(value: number): void;
  set_rotation(value: number): void;
  navigate(page: number): void;
  use_documentation_fixture(riding: boolean): void;
  show_pairing_demo(): void;
  state(name: string): string | number | boolean;
  free(): void;
}
interface SimulatorModule {
  default(): Promise<unknown>;
  WebSimulator: new (id: string) => Simulator;
}

// React's development effects may overlap while the module is loading. Share
// initialization so wasm-bindgen never swaps the memory behind a live instance.
let moduleReady: Promise<SimulatorModule> | undefined;
function loadModule(url: string) {
  return (moduleReady ??= (async () => {
    const module = (await import(/* @vite-ignore */ url)) as SimulatorModule;
    await module.default();
    return module;
  })());
}
declare global {
  interface Window {
    wheelieFunDisplay?: Simulator;
  }
}

export interface SimulatorLayout {
  landscape: boolean;
  large: boolean;
  rotation: number;
}
export async function mountSimulator(
  canvas: HTMLCanvasElement,
  signal: AbortSignal,
  active: () => boolean,
  report: (status: string, error?: boolean) => void,
  layout: (state: SimulatorLayout) => void,
) {
  let simulator: Simulator | undefined;
  let animation = 0;
  signal.addEventListener(
    "abort",
    () => {
      cancelAnimationFrame(animation);
      simulator?.free();
      delete window.wheelieFunDisplay;
    },
    { once: true },
  );
  const moduleUrl = new URL("pkg/wheelie_fun_display.js", location.href).href;
  const wasm = await loadModule(moduleUrl);
  if (signal.aborted) return;
  simulator = new wasm.WebSimulator(canvas.id);
  const current = simulator;
  window.wheelieFunDisplay = current;
  const started = performance.now();
  const params = new URLSearchParams(location.search);
  if (params.get("display") === "guition") current.set_display_model(1);
  if (params.get("orientation") === "landscape") current.set_rotation(90);
  else if (params.has("orientation")) current.set_rotation(Number(params.get("orientation")));
  if (params.get("integration") === "wheel-e") current.set_integration(2);
  if (params.get("integration") === "vesc") current.set_integration(0);
  if (params.has("page")) current.navigate(Number(params.get("page")));
  if (params.get("preview") === "docs") current.use_documentation_fixture(!params.has("page"));
  canvas.addEventListener(
    "wheel",
    (event) => {
      event.preventDefault();
      const bounds = canvas.getBoundingClientRect();
      current.scroll(
        ((event.clientX - bounds.left) * canvas.width) / bounds.width,
        ((event.clientY - bounds.top) * canvas.height) / bounds.height,
        -event.deltaY,
      );
    },
    { passive: false, signal },
  );
  let announced = false;
  let previous = "";
  function frame(now: number) {
    if (signal.aborted) return;
    if (active()) {
      try {
        current.frame(now - started);
        if (!announced) {
          announced = true;
          report("Ready · real device UI, simulated hardware");
        }
        const state = {
          landscape: Boolean(current.state("landscape")),
          large: Boolean(current.state("display-model")),
          rotation: Number(current.state("rotation")),
        };
        const next = JSON.stringify(state);
        if (next !== previous) {
          previous = next;
          layout(state);
        }
      } catch (error) {
        report(`Simulator error: ${error instanceof Error ? error.message : String(error)}`, true);
        return;
      }
    }
    animation = requestAnimationFrame(frame);
  }
  animation = requestAnimationFrame(frame);
  return { simulator: current, started };
}
