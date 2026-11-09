# Troubleshooting

Find the symptom below and work through its checks in order. Keep the bike stopped while investigating settings or locking. A reading of `--` means unavailable or out-of-date data, not zero.

## The screen is black

1. Tap once, lift your finger and wait for the unlock screen. A locked display intentionally turns the panel black after its timeout.
2. Check power and the USB cable.
3. After a first installation, confirm you selected the exact board model and completed the full USB installation.
4. If the correct image still does not start, save the boot log if available and report the problem.

Only the listed Waveshare 2.06-inch and GUITION 4.3-inch targets are supported. See [Waveshare recovery](hardware-waveshare-206.md#usb-recovery-and-checks) or [GUITION requirements](hardware-guition-43.md).

## The USB port is missing

1. Use a known USB **data** cable and try a direct computer port.
2. Close any serial monitor or app already using the port.
3. Follow your board's BOOT/RESET download-mode procedure, then reconnect in the installer. For Waveshare, hold BOOT, tap RESET and release BOOT.
4. Select the newly detected port; its name may have changed.

If using PlatformIO, `pio device list` lists available ports. See the [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) for USB installation.

## No bike appears over BLE

1. Power the bike and make its Bluetooth interface available.
2. Disconnect another phone/app if the bike's module allows only one connection.
3. Open **Settings > Vehicle** and look through the device list. Recognizable names sort first, but other compatible devices remain selectable.
4. Leave integration at **Auto**, or confirm the explicitly selected WFB/wheel-e package is installed on the intended controller.

## The link connects but values stay unavailable

1. Check controller discovery status under **Settings > Vehicle**. A connected link can still be waiting for valid readings.
2. Disconnect the phone proxy to let discovery and display polling proceed.
3. Use **Find main controller again** when the link is available.
4. Check the integration. Explicit WFB or wheel-e selection requires that package; generic VESC provides standard readings only.

Busy phone traffic or firmware transfers can temporarily pause readings. BMS data also depends on controller support and may be cached. See [Stats](user-guide.md#stats).

## The BLE PIN is rejected

Check which devices you are pairing:

| Connection | Code to enter |
| --- | --- |
| Display to the bike's Bluetooth device | The PIN configured on that bike device. |
| Phone to the display | The code shown under **Settings > Phone BLE code** after unlocking. A pattern uses its six dot numbers. |

If the bike's PIN changed, retry with its new PIN. If the display's screen lock changed or was removed, forget the display's old pairing on the phone and pair again. After restarting a secured display, unlock it locally before connecting the phone.

See [PINs, patterns and pairing](pins-and-locking.md) for entry rules and cooldowns.

## CAN or UART is absent from Settings

These choices require firmware with both pins configured for that interface. Stock profiles leave optional wired pins unset.

Check your board's wiring and firmware configuration. CAN also needs a transceiver, matching bus speed and a unique display ID; UART needs matching baud and compatible signal levels. Changing GPIO configuration requires rebuilding firmware. See [wired-interface setup](building.md#configure-an-optional-wired-interface).

## A tuning or lock action is unavailable

1. Confirm the integration supports that feature. Tune currently requires a supported WFB configuration.
2. Unlock the display and stop the bike.
3. Disconnect the phone proxy.
4. Wait for fresh readings, a ready connection and no controller fault.
5. For tuning, read the bike's configuration successfully before editing.

A draft is not a saved setting. After an interrupted tuning save, read the bike again before retrying. An **unconfirmed** lock means the required bike response has not arrived; do not assume propulsion is disabled. See [tuning](user-guide.md#tune) or [bike locking](pins-and-locking.md#lock-the-bike-and-screen).

## No update is offered

1. Confirm the display is connected to Wi-Fi.
2. Disconnect the phone proxy and check again under **Settings > Firmware updates**.
3. Check whether a public [release](https://github.com/techfoundrynz/wheelie_fun_display/releases) exists for your exact board.
4. Record any message about a missing or invalid image.

Unavailable releases or validation failures keep the current firmware selected. See [updates and maintenance](maintenance.md).

## Report a reproducible problem

Include:

- Display model and firmware version from **Settings > About**.
- Integration and connection type (BLE, CAN or UART).
- Controller firmware and, if relevant, phone/app versions.
- Steps to reproduce, expected behavior, actual behavior and the last version that worked.
- A serial log if available, with passwords and PINs removed.

Avoid erasing settings before collecting this information. See [contributing](../CONTRIBUTING.md) for reporting details.
