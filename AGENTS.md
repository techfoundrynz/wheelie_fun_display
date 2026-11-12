# Instructions for coding and device assistants

Read `README.md` and `CONTRIBUTING.md` before changing the project. Use the relevant board guide and `docs/building.md` before building or flashing firmware. The help site's `docs/agents.md` contains task prompts for users.

## Repository and tools

- `ui/`: the device's shared Slint interface. `firmware/`: ESP-IDF firmware. `simulator/`: desktop and browser implementations of the device UI.
- `website/`: the React/TypeScript/TanStack Start help site, simulator controls and USB installer. This is separate from the Slint device UI.
- Rider and contributor content stays in repository Markdown files. `website/content.json` declares help-site routes, titles, navigation groups and descriptions.
- Use Node.js 24 or newer and the Corepack/pnpm version pinned in `package.json`. Use `corepack pnpm`, not npm. Preserve the workspace lockfile and minimum-release-age policy.
- Write new help-site code, scripts, plugins and tests in TypeScript. Use the existing Tailwind/CSS structure, Oxlint, Oxfmt and Vitest.
- Preserve local edits and generated tool assets. Do not edit `website/src/routeTree.gen.ts`, build output or downloaded dependencies as source.

## Help-site changes

The default design is Rail. Keep the shared header/footer mounted across docs, simulator and installer routes. Load shared styles once through `website/src/app.css`. Page transitions apply to the main content only; hash links and reduced-motion navigation must not start view-transition snapshots.

Use the repository-root commands:

```sh
corepack pnpm install --frozen-lockfile
corepack pnpm dev
corepack pnpm test:docs
corepack pnpm check:docs
corepack pnpm lint
corepack pnpm format:check
corepack pnpm build:docs
```

`corepack pnpm test:docs:watch` runs Vitest while editing. Keep its config separate from the application Vite config so unit tests do not start the app's build plugins. Add relevant cases under `website/src` or `website/plugins`. Use the existing Python/Playwright browser checks in `tests/` for actual navigation, responsive layout, WASM and installer behavior.

The static build needs generated screenshots and the simulator/installer need generated assets; follow `website/README.md` and `docs/site-maintenance.md`. Check root and GitHub Pages project-subpath hosting when changing routes or asset handling. Describe simulator checks separately from real-device checks.

## Downloading and installing firmware

- Identify the exact board model and hardware requirements first. `platformio.ini` and its `default_envs` are the source of truth. The current targets are `waveshare_esp32s3_206` and `guition_jc4880p443c`; chip family alone does not establish compatibility.
- Read `docs/hardware-waveshare-206.md` or `docs/hardware-guition-43.md`. GUITION requires production ESP32-P4 revision 3.1 or newer and compatible C6 companion firmware. Pre-v3 boards are unsupported. Display installation does not provision the C6.
- For a released version, use assets from the project's published GitHub release, not arbitrary binaries or another board's image. Read its `manifest.json`; verify the selected file's target, byte size and SHA256. Use the newest published stable release that includes the target unless the user specifies another version or channel.
- Complete USB images are `<environment>-factory.bin` and are listed under `installers` in the release manifest. Application-only images are for the documented update paths. Do not interchange them or guess partition offsets. Use the browser installer or the documented build/upload flow for the exact target.
- A request to download firmware alone does not authorize flashing. When installation is requested, identify the actual device and USB port; ask for identification if the target is ambiguous. Do not force a chip mismatch or select an unrelated device. Tell the user that a complete USB installation can reset saved settings.
- The browser's USB picker and physical BOOT/RESET steps may require the user. Explain the required action when handing over. Do not claim to have flashed or verified hardware when only a browser simulation or build ran.
- For local builds, discover ports with `pio device list`, build the matching environment and use `pio run -e <environment> -t upload --upload-port <detected-port>`. Follow `docs/building.md` for the pinned tools and required SDK audit. Do not erase all flash as a routine troubleshooting step.
- After installation, inspect boot output where available and verify dashboard startup, touch and orientation. Report the version or source revision, target, checks and any hardware verification still outstanding.

## Firmware changes

Preserve the separation between integrations, transports, access control and rendering described in `CONTRIBUTING.md`. Put board capabilities and GPIOs in the hardware profile. Keep Slint state owned by the UI task. Do not invent undocumented protocol behavior or treat draft settings as controller readbacks.

Build affected firmware targets and audit SDK configuration. Use the existing CTest, Cargo, Python and browser suites that exercise the changed behavior. UI changes need portrait and landscape review. A simulator result does not establish real-controller or radio interoperability. Keep required licenses and notices, and state the limits of validation in the final report.
