# Contributing

Changes should preserve the separation between vehicle integrations, physical transports, local access control and UI rendering. A new board or protocol should not add board-name branches throughout shared code.

## Start with a reproducible change

For a bug, describe the concrete trigger, observed behavior and expected result. Include the board environment, firmware version, integration and transport. For BLE or proxy issues, include controller and phone/app versions. Redact PINs, passwords and other credentials from logs.

Discuss substantial new hardware/protocol support before assuming undocumented wire formats. The protocol references record compatibility details and uncertainties; keep those distinctions visible.

## Repository map

| Area | Responsibility |
| --- | --- |
| `ui/` | Shared Slint components, layout, gestures and dialogs. |
| `firmware/main/` | Board drivers, runtime ownership, transports, networking and application projection. |
| `firmware/components/bike/` | Portable framing, integrations, telemetry and state policies. |
| `simulator/` | Desktop, browser and headless previews of the shared UI. |
| `tests/` | Portable C++ checks for protocol, state and renderer-related policies. |
| `scripts/` | Reproducible configuration, packaging and documentation helpers. |
| `docs/` | Rider guides, architecture and maintenance documentation. |

## Keep changes reviewable

- Put target capabilities, resolution and GPIOs in the PlatformIO hardware profile. Share settings through `common` rather than inheriting one board from another.
- Put package-specific encoding and feature flags in its vehicle integration. Unsupported commands must be gated in both UI and command handling.
- Keep the phone proxy independent of the display's selected main-controller route.
- Invalidate old RX/TX work across connection or transport generations. Do not replay a tuning command after reconnect.
- Keep Slint ownership on the UI task. Communication callbacks should transfer bounded data, not edit UI properties.
- Preserve required third-party notices and source/binary line-ending rules.

## Validate the change

Use the checks that establish the behavior you changed. Firmware changes normally need a build and SDK audit:

```sh
pio run
python scripts/check_sdkconfig.py --all
```

Portable protocol/state changes need host tests; see [building](docs/building.md#host-checks). UI changes should be checked at 410 × 502 and 502 × 410 with the headless transition checks and actual rendered fixtures. Exercise touch/drag cancellation, safe corners and orientation as relevant.

Run `cargo fmt --check --manifest-path simulator/Cargo.toml` for Rust changes. C/C++ formatting uses the repository `.clang-format` plus declaration spacing; CI pins clang-format 23.1.0. Run `python scripts/format_cpp.py` to format, or add `--check` to verify without changes. Do not write a test that simply restates the implementation without exercising the failure or behavior that matters.

For docs, build the Pages artifact and inspect desktop and mobile layouts. The browser simulator is a demo: it must never imply that mocked controls operate a real vehicle.

## Describe evidence precisely

A useful change description states the problem, resulting behavior and relevant validation. Distinguish a compiler check, host/simulator test, startup log and a real-controller acceptance test. If radio, PIN changes or wired behavior could not be tested, say so and leave the acceptance item open.

Do not claim delivered FPS from a first-frame timing or successful boot. Add reproducible performance measurements to [the performance notes](docs/performance.md).

## License

Project contributions use GPL-3.0-only, subject to the existing third-party notices. See [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md). Do not copy images, fonts or code into the repository without retaining their applicable permissions and notices.

## Pull request checks and previews

PR CI checks C++/Python/Rust formatting, sanitizer-backed protocol/transport tests, all four UI viewport layouts and touch rotations, both firmware targets, browser interactions and installer integrity. A successful run publishes an isolated simulator preview and browser-test gallery, linked by one updated bot comment. New commits replace the preview; closed PRs have it removed. Downloadable artifacts remain available from the run.

Production deployment and PR preview publication use trusted default-branch code after the read-only build succeeds. The publisher verifies the run, head revision and current PR state before deploying. Version increases on `main` additionally create an immutable release after every validation stage passes; see [release maintenance](docs/site-maintenance.md#automatic-releases-and-pr-previews).
