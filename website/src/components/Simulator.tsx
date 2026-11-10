import { useEffect, useRef, useState, type PointerEvent } from "react";
import { mountSimulator, type Simulator, type SimulatorLayout } from "../tools/simulator";
const units = { speed: " km/h", battery: "%", temperature: "°C", pitch: "°" };
export function Simulator({ active }: { active: boolean }) {
  const [initialViewport, setInitialViewport] = useState({
    large: false,
    landscape: false,
    rotation: 0,
    width: 410,
    height: 502,
  });
  const canvas = useRef<HTMLCanvasElement>(null);
  const isActive = useRef(active);
  isActive.current = active;
  const api = useRef<{ simulator: Simulator; started: number } | undefined>(undefined);
  const pointerDown = useRef(false);
  const [status, setStatus] = useState("Loading the interactive display…");
  const [error, setError] = useState(false);
  const [ready, setReady] = useState(false);
  const [scene, setScene] = useState({ speed: 0, battery: 82, temperature: 36, pitch: 0 });
  const [connected, setConnected] = useState(true);
  const [integration, setIntegration] = useState(1);
  const [layout, setLayout] = useState<SimulatorLayout>(initialViewport);
  useEffect(() => {
    const params = new URLSearchParams(location.search);
    const large = params.get("display") === "guition";
    const rotation =
      params.get("orientation") === "landscape" ? 90 : Number(params.get("orientation") || 0);
    const landscape = rotation === 90 || rotation === 270;
    const [short, long] = large ? [480, 800] : [410, 502];
    setInitialViewport({
      large,
      landscape,
      rotation,
      width: landscape ? long : short,
      height: landscape ? short : long,
    });
    setLayout({ large, landscape, rotation });
    const controller = new AbortController();
    void mountSimulator(
      canvas.current!,
      controller.signal,
      () => isActive.current,
      (message, isError = false) => {
        setStatus(message);
        setError(isError);
        setReady(!isError);
      },
      setLayout,
    )
      .then((value) => {
        api.current = value;
      })
      .catch((error) => {
        if (!controller.signal.aborted) {
          setStatus(
            `The simulator could not start. ${error instanceof Error ? error.message : String(error)}`,
          );
          setError(true);
        }
      });
    return () => controller.abort();
  }, []);
  function control(name: keyof typeof scene, value: number) {
    setScene((scene) => ({ ...scene, [name]: value }));
    api.current?.simulator.set_control(name, value);
  }
  function pointer(kind: string, event: PointerEvent<HTMLCanvasElement>) {
    if (!api.current) return;
    const target = event.currentTarget;
    const bounds = target.getBoundingClientRect();
    api.current.simulator.pointer(
      kind,
      ((event.clientX - bounds.left) * target.width) / bounds.width,
      ((event.clientY - bounds.top) * target.height) / bounds.height,
      performance.now() - api.current.started,
    );
  }
  return (
    <>
      <main
        id={active ? "content" : "simulator-content"}
        className="simulator-main mx-auto w-full max-w-[1214px]"
        tabIndex={-1}
      >
        <section className="intro">
          <p className="eyebrow">TRY THE DISPLAY</p>
          <h1>Your ride, on screen.</h1>
          <p>
            The same Slint interface and software renderer as the device, running in your browser.
            Touch, drag and swipe to explore.
          </p>
          <div className="notice">
            <span className="dot"></span>
            <span>
              Demo only. All telemetry, connections and updates are simulated. Nothing connects to a
              bike or downloads firmware.
            </span>
          </div>
        </section>
        <p id="status" role="status" className={error ? "error" : undefined}>
          {status}
        </p>
        <div className="workspace" id="workspace" aria-busy={!ready}>
          <section className="display-section" aria-label="Interactive display">
            <div
              className={
                "display-frame" +
                (layout.landscape ? " landscape" : "") +
                (layout.large ? " large-display" : "")
              }
            >
              <canvas
                ref={canvas}
                onPointerDown={(event) => {
                  event.preventDefault();
                  event.currentTarget.focus();
                  pointerDown.current = true;
                  event.currentTarget.setPointerCapture(event.pointerId);
                  pointer("down", event);
                }}
                onPointerMove={(event) => {
                  if (pointerDown.current) pointer("move", event);
                }}
                onPointerUp={(event) => {
                  if (pointerDown.current) pointer("up", event);
                  pointerDown.current = false;
                  if (event.currentTarget.hasPointerCapture(event.pointerId))
                    event.currentTarget.releasePointerCapture(event.pointerId);
                }}
                onPointerCancel={(event) => {
                  pointer("cancel", event);
                  pointerDown.current = false;
                }}
                id="display"
                width={initialViewport.width}
                height={initialViewport.height}
                tabIndex={ready ? 0 : -1}
                aria-label="Wheelie Fun Display touchscreen. Use pointer or touch to activate controls, drag to scroll, and swipe between Ride, Stats and Tune."
              ></canvas>
            </div>
            <p className="hint">Swipe between Ride, Stats and Tune. Use the cog for Settings.</p>
            <div className="view-options" inert={!ready}>
              <button
                id="orientation"
                type="button"
                onClick={() => api.current?.simulator.set_rotation((layout.rotation + 90) % 360)}
              >
                Rotate: {layout.rotation}°
              </button>
              <button
                id="reset"
                onClick={() => location.reload()}
                type="button"
                className="secondary"
              >
                Reset demo
              </button>
            </div>
          </section>
          <aside inert={!ready} className="controls" aria-labelledby="controls-title">
            <p className="eyebrow">SIMULATED HARDWARE</p>
            <h2 id="controls-title">Set the scene</h2>
            <label className="switch">
              <span>Bike connected</span>
              <input
                id="connected"
                type="checkbox"
                checked={connected}
                onChange={(event) => {
                  setConnected(event.target.checked);
                  api.current?.simulator.set_connected(event.target.checked);
                }}
              />
            </label>
            <label className="select-label" htmlFor="display-model">
              Display model
            </label>
            <select
              id="display-model"
              value={layout.large ? 1 : 0}
              onChange={(event) =>
                api.current?.simulator.set_display_model(Number(event.target.value))
              }
            >
              <option value="0">Waveshare 2.06-inch</option>
              <option value="1">GUITION 4.3-inch</option>
            </select>
            <label className="select-label" htmlFor="integration">
              Vehicle integration
            </label>
            <select
              id="integration"
              value={integration}
              onChange={(event) => {
                setIntegration(Number(event.target.value));
                api.current?.simulator.set_integration(Number(event.target.value));
              }}
            >
              <option value="1">Wheelie Fun Bike</option>
              <option value="2">wheel-e</option>
              <option value="0">Generic VESC</option>
            </select>
            <label className="range-label" htmlFor="speed">
              <span>Speed</span>
              <output id="speed-value">
                {scene.speed}
                {units.speed}
              </output>
            </label>
            <input
              id="speed"
              type="range"
              min="0"
              max="65"
              step="0.5"
              value={scene.speed}
              onChange={(event) => control("speed", Number(event.target.value))}
            />
            <label className="range-label" htmlFor="battery">
              <span>Battery</span>
              <output id="battery-value">
                {scene.battery}
                {units.battery}
              </output>
            </label>
            <input
              id="battery"
              type="range"
              min="0"
              max="100"
              value={scene.battery}
              onChange={(event) => control("battery", Number(event.target.value))}
            />
            <label className="range-label" htmlFor="temperature">
              <span>Controller temperature</span>
              <output id="temperature-value">
                {scene.temperature}
                {units.temperature}
              </output>
            </label>
            <input
              id="temperature"
              type="range"
              min="20"
              max="100"
              value={scene.temperature}
              onChange={(event) => control("temperature", Number(event.target.value))}
            />
            <label className="range-label" htmlFor="pitch">
              <span>Pitch</span>
              <output id="pitch-value">
                {scene.pitch}
                {units.pitch}
              </output>
            </label>
            <input
              id="pitch"
              type="range"
              min="-30"
              max="60"
              step="0.5"
              value={scene.pitch}
              onChange={(event) => control("pitch", Number(event.target.value))}
            />
            <div className="presets">
              <button id="stop" type="button" onClick={() => control("speed", 0)}>
                Stop bike
              </button>
              <button
                id="ride"
                onClick={() => {
                  control("speed", 24);
                  control("pitch", 12);
                }}
                type="button"
                className="secondary"
              >
                Ride example
              </button>
            </div>
            <p className="small">
              Stop the simulated bike before applying tuning or exploring update installation. Demo
              PINs last only until you reload; use a made-up code.
            </p>
            <button
              onClick={() => api.current?.simulator.show_pairing_demo()}
              id="pairing-demo"
              className="secondary"
              type="button"
            >
              Try Bluetooth pairing
            </button>
            <p className="small">
              The simulated upstream Bluetooth PIN is <strong>123456</strong>. It is separate from
              the demo bike lock PIN.
            </p>
            <details>
              <summary>Jump to a screen</summary>
              <div id="screen-links" className="screen-links">
                <button onClick={() => api.current?.simulator.navigate(0)} data-page="0">
                  Ride
                </button>
                <button onClick={() => api.current?.simulator.navigate(1)} data-page="1">
                  Stats
                </button>
                <button data-page="2">Bike</button>
                <button onClick={() => api.current?.simulator.navigate(3)} data-page="3">
                  Settings
                </button>
                <button data-page="4">Wi-Fi</button>
                <button onClick={() => api.current?.simulator.navigate(5)} data-page="5">
                  Updates
                </button>
                <button onClick={() => api.current?.simulator.navigate(6)} data-page="6">
                  About
                </button>
              </div>
            </details>
          </aside>
        </div>
      </main>
    </>
  );
}
