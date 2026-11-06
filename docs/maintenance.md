# Updates and maintenance

Keep firmware artifacts tied to the hardware profile and record the checks performed when reviewing a release.

## Update from the display

1. Open **Settings > Wi-Fi** and connect to a network. The keyboard supports hidden SSIDs and password entry.
2. Open **Settings > Firmware updates** and check for a release.
3. Review the available version, confirm installation and keep the display powered.
4. Restart when the verified image is ready.

The public release feed is `techfoundrynz/wheelie_fun_display`. A release needs the matching environment-named image and GitHub SHA256 digest. Missing releases or incompatible images are reported without replacing the installed firmware.

Installation pauses vehicle communications. A phone owning the proxy conversation blocks maintenance until it disconnects. Certificate-verified HTTPS and image validation protect the download; this is not a separate signed-firmware/secure-boot scheme.

## Release artifacts

Build the supported environments before packaging:

```sh
pio run
python scripts/check_sdkconfig.py --all
python -m pip install -r requirements-release.txt
python scripts/package_release.py
```

Packaging writes OTA applications `.pio/releases/<environment>.bin`, complete USB images `<environment>-factory.bin`, `manifest.json`, `licenses.zip` and `source-references.json`. The notices archive preserves the project, font, icon and bundled component licenses. Source references record the application repository/commit and pinned Slint tag/commit; local uncommitted changes are marked explicitly. The supported list comes from `default_envs`; removed board images are not carried forward. The release workflow injects the tag version using `WFD_FIRMWARE_VERSION` and builds that same supported list.

The complete USB image is merged from the build's actual bootloader, partition-table, OTA initialization and application offsets; it is not interchangeable with an OTA application image. USB installation can reset NVS settings.

Review the version, board identity, sizes and hashes before publishing. App slots are 7 MiB each. A newly booted image is accepted only after initialization and ten seconds of a functioning UI loop; an early failure leaves rollback available.

## Diagnose before resetting

**About > Diagnostics** provides version/build identity, uptime, memory, display work timings and connection information. Save the serial log and describe the action that caused an issue. Panel work timings are not the same as measured delivered FPS.

NVS contains preferences, saved associations, bonds, Wi-Fi details and PIN state. Do not erase it as routine troubleshooting; that changes the user's setup and makes a problem harder to reproduce.

## Hardware acceptance

A successful build and simulator check do not establish radio or wired interoperability. For a release, record the exact board, controller firmware and phone/app used, then exercise:

- Cold boot, all four orientations, corner-safe layout and touch alignment.
- Settings scrolling, PIN entry, locked timeout and wake-contact consumption.
- Disconnect/reconnect, upstream encrypted pairing and changed/removed local PINs.
- CAN/UART operation with the actual transceiver/wiring and controller discovery.
- Phone proxy behavior, CAN forwarding, MTU differences, sustained traffic and lease release.
- Wi-Fi/OTA success, failure, cancellation and first-boot acceptance.

Track remaining hardware work in [TODO](../TODO.md). Record repeatable performance measurements in [performance notes](performance.md); a startup sample does not establish a sustained frame rate.

## Documentation and Pages

CI builds the browser simulator, renders documentation pictures from that current shared Slint UI, then assembles and tests the site. Screenshots are generated artifacts under `.cache/pages/docs-images`; do not check them in or regenerate them before committing.

To reproduce the complete Pages build locally:

```sh
python -m pip install -r requirements-docs.txt -r simulator/requirements-browser-test.txt
python -m playwright install chromium
python scripts/build_web_simulator.py --output .cache/pages/site/simulator
python scripts/generate_docs_screenshots.py --site .cache/pages/site
python scripts/build_docs.py --output .cache/pages/site
python scripts/build_flash_site.py --site .cache/pages/site --github-release
python tests/flasher_browser_test.py --site .cache/pages/site
python tests/simulator_browser_test.py --site .cache/pages/site
python scripts/package_browser_results.py --site .cache/pages/site
python tests/docs_browser_test.py --site .cache/pages/site
```

The [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) uses pinned ESP Web Tools and same-origin, SHA256-verified release images. A completed release triggers a new Pages build so the installer picks up the newly published images. Before the first release, the installer lists supported boards and reports that firmware is unavailable. For local packaging checks, use `--release-dir .pio/releases` instead of `--github-release`; that preview is clearly labelled unpublished.

The [interactive simulator](../simulator/) is the clean end-user preview. The separate [CI browser evidence gallery](../ci/) contains touchscreen checks and their captured images. Browser checks cover root and project-subpath hosting, navigation, search, touch, rotation, PIN entry, telemetry and responsive layout. Screenshots support visual review; they are not pixel-baseline comparisons or real-device acceptance.

Preview the generated site through a local HTTP server. Both the downloadable site artifact and GitHub Pages include the simulator and separate CI gallery. Generated HTML and screenshots are build outputs, not a second hand-edited copy of the source.
