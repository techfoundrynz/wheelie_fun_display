import { DocLink as Link } from "./DocLink";
export function Home() {
  return (
    <>
      <section className="hero">
        <div className="hero-copy">
          <div className="eyebrow">
            <span className="status-dot"></span> Touch display · VESC integrations
          </div>
          <h1>
            Your ride.
            <br />
            <span>A clearer view.</span>
          </h1>
          <p>
            A responsive bike display for Wheelie Fun Bike, wheel-e and VESC. Set up your display,
            connect your bike and learn the everyday controls.
          </p>
          <div className="hero-actions">
            <Link className="button primary" to="/install/">
              Install firmware <span aria-hidden="true">→</span>
            </Link>
            <Link className="button secondary" to="/simulator/">
              Open simulator
            </Link>
          </div>
          <div className="hero-tags">
            <span>ESP32-S3 / P4</span>
            <span>Slint UI</span>
            <span>GPL-3.0</span>
          </div>
        </div>
        <figure className="hero-preview">
          <div className="preview-glow"></div>
          <picture>
            <source
              media="(prefers-reduced-motion: reduce)"
              srcSet="assets/images/ride-portrait.png"
            />
            <img
              className="portrait-preview"
              src="assets/images/ride-portrait.webp"
              width="410"
              height="502"
              alt="Portrait ride dashboard with animated simulated speed and telemetry"
            />
          </picture>
          <figcaption>Shared UI preview · simulated values</figcaption>
        </figure>
      </section>
      <section className="intro-cards" aria-label="Choose a guide">
        <Link className="intro-card" to="/getting-started/">
          <span className="card-number">01 / SET UP</span>
          <h2>Set up your display</h2>
          <p>
            Follow the first-installation steps, choose your bike type and make your first
            connection.
          </p>
          <span className="card-link">Get started →</span>
        </Link>
        <Link className="intro-card" to="/guide/">
          <span className="card-number">02 / RIDE</span>
          <h2>Know your display</h2>
          <p>Learn Ride, Stats and Tune, change display settings and use the touchscreen.</p>
          <span className="card-link">Explore the interface →</span>
        </Link>
        <Link className="intro-card" to="/connections/">
          <span className="card-number">03 / CONNECT</span>
          <h2>Choose your connection</h2>
          <p>
            Connect over Bluetooth, set up a wired connection or use a phone app through the
            display.
          </p>
          <span className="card-link">Connect a bike →</span>
        </Link>
      </section>
      <section className="landscape-section">
        <div>
          <div className="eyebrow">Made for the screen you have</div>
          <h2>
            Portrait or landscape.
            <br />
            One familiar interface.
          </h2>
          <p>
            Choose portrait or landscape in Settings to match your mount. The screen and touch
            controls rotate together.
          </p>
          <Link className="text-link" to="/guide/">
            See everyday controls →
          </Link>
        </div>
        <figure>
          <img
            src="assets/images/ride-landscape.png"
            width="502"
            height="410"
            alt="Landscape dashboard with simulated ride values"
          />
          <figcaption>Landscape software preview</figcaption>
        </figure>
      </section>
      <section className="supported-section">
        <div className="eyebrow">Start with supported hardware</div>
        <h2>Two displays. One shared interface.</h2>
        <p>
          Waveshare ESP32-S3 AMOLED 2.06 pairs a 410 × 502 CO5300 panel with FT3168 touch. GUITION
          JC4880P443C uses a 480 × 800 ST7701S panel, GT911 touch and ESP32-P4 with a C6 radio
          companion. Choose your exact model when installing, then follow its board guide for wiring
          and setup. The GUITION profile requires a P4 below revision 3.0 and compatible C6 radio
          firmware.
        </p>
        <div className="supported-links">
          <Link to="/getting-started/">First connection →</Link>
          <Link to="/install/">Install firmware →</Link>
          <Link to="/hardware/waveshare-206/">Waveshare board setup →</Link>
          <Link to="/hardware/guition-43/">GUITION board setup →</Link>
          <Link to="/maintenance/">Maintaining the display →</Link>
        </div>
      </section>
    </>
  );
}
