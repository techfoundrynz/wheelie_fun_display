# AI assistants

You can ask an AI assistant to help install firmware, investigate a problem or change the display software. Start with one of the prompts below and replace the bracketed details. Give the assistant this page or the [source repository](https://github.com/techfoundrynz/wheelie_fun_display) so it can check the project instructions.

An assistant with local terminal access can download files, inspect a connected USB device, build firmware and run checks. A chat-only assistant can explain the steps. Browser USB installation still needs you to select the display in the browser's device picker.

## Download firmware

Use this when you want an existing release rather than a custom build. Include the complete display model printed on the board.

```text
Download Wheelie Fun Display firmware for [exact display model].
Use https://github.com/techfoundrynz/wheelie_fun_display.

Read the board guide and find the newest published stable release that
includes this exact target. I need [a complete USB installation image /
an application image for an update]. Download the matching asset and
release manifest, verify its size and SHA256, and tell me the version,
target and local file path. Do not flash anything yet.
```

The release manifest lists complete USB images under `installers`. A `-factory.bin` contains the full flash layout; an application `.bin` is for an application update. They are not interchangeable. If the release has no image for your board, the assistant should report that rather than substitute another target.

See [updates and maintenance](maintenance.md) for the available update methods.

## Install a published release over USB

Connect the display with a USB data cable. Close any serial monitor that is using its port. A complete USB installation can reset saved settings; routine updates can use the display's Wi-Fi update menu instead.

```text
Install the newest published stable Wheelie Fun Display release on my
[exact display model] connected to this computer over USB.
Use https://github.com/techfoundrynz/wheelie_fun_display and read AGENTS.md
and the board's setup guide first.

Identify the target and USB port, check the board requirements, and verify
the matching release image against its manifest. Use the browser installer
or the documented terminal workflow available on this computer. If the
board or port is ambiguous, ask me which device it is before writing.

Tell me when I need to select a browser USB device or press BOOT/RESET.
After installation, check the boot output where available and help me
verify the dashboard, touch and orientation. Report anything you could
not verify on the hardware.
```

The [USB installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) handles complete release images. The [Waveshare guide](hardware-waveshare-206.md) and [GUITION guide](hardware-guition-43.md) cover board-specific requirements and recovery. GUITION requires a P4 below revision 3.0 and compatible C6 radio firmware; installing the display application does not provision the C6 companion.

## Make a change and test it

Describe the behavior you want and where you expect to see it. Start with a preview when changing the interface.

```text
In https://github.com/techfoundrynz/wheelie_fun_display, change
[screen, setting or behavior] so that [specific expected result].

Read AGENTS.md and CONTRIBUTING.md. Inspect the existing implementation,
make the change using the project's existing patterns, and run the checks
that exercise it. For display UI changes, check portrait and landscape
in the simulator. For help-site changes, check desktop and mobile.

Show me the result and summarize the files changed, checks run and any
hardware behavior that still needs testing. Do not flash my display as
part of this task.
```

The display interface lives in `ui/` and uses Slint. The help site lives in `website/` and uses React, TypeScript, Tailwind and TanStack Start. Changing one does not automatically change the other. See [contributing](../CONTRIBUTING.md) and [building and installing](building.md) for the repository map and validation steps.

## Build and install a custom version

Use this after reviewing a change, or when you specifically want to test local firmware on the display.

```text
Build the current Wheelie Fun Display checkout for [exact display model]
and install it on that display over USB.

Read AGENTS.md and the board guide. Preserve my local changes, use the
matching PlatformIO environment and discover the actual USB port. Run
the relevant checks and build before uploading. If more than one device
could be the display, ask me to identify it.

After uploading, inspect the boot log where available and help me check
touch and orientation. Record the environment, source revision, local
changes and validation results. Treat this as a local build, not an
official release.
```

PlatformIO commands and port discovery are documented under [first USB installation](building.md#first-usb-installation). Use the exact target environment; sharing an ESP32 chip family does not make two display boards compatible.

## Investigate a problem

```text
Help diagnose this Wheelie Fun Display problem:
- Display model: [exact model]
- Firmware version: [version shown in About]
- Bike integration and connection: [WFB / wheel-e / VESC; BLE / CAN / UART]
- Steps: [what I did]
- Expected result: [what should happen]
- Actual result: [what happens, including any message]

Use the project's troubleshooting and board guides. Start with checks
that preserve my settings. Explain what each result establishes, and
separate simulator results from anything verified on the real display.
```

Attach a screenshot or serial log if it helps describe the problem. See [troubleshooting](troubleshooting.md) for connection, touch and update checks.

## Instructions for assistants

The repository's [AGENTS.md](../AGENTS.md) describes the tools, supported targets and checks an assistant should use. It complements [CONTRIBUTING.md](../CONTRIBUTING.md), the board guides and the release manifest. Follow those sources rather than assuming generic ESP32 flashing instructions apply to this display.
