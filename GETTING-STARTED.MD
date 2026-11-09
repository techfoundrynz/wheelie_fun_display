# Getting started

Install the firmware for your display, connect your bike, then choose your everyday settings. You can try the [browser simulator](simulator/) first to learn the screens without hardware.

## What you need

- A supported **Waveshare ESP32-S3 Touch AMOLED 2.06-inch** or **GUITION JC4880P443C 4.3-inch** display.
- A USB data cable for the first installation.
- A VESC-based bike with Wheelie Fun Bike, wheel-e or a compatible generic VESC setup.

Check your board before installing: [Waveshare setup](docs/hardware-waveshare-206.md) or [GUITION setup](docs/hardware-guition-43.md). The GUITION profile requires an ESP32-P4 below revision 3.0 and compatible C6 radio firmware. Images for other boards or revisions are not interchangeable.

## 1. Install and power up

1. Open the [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) and choose your exact display model.
2. Connect the display with a USB data cable and follow the installation prompts. Keep it powered until installation finishes.
3. Let the display start. The Ride dashboard should appear. Unlock it if a screen lock was already configured.

A complete USB installation can reset saved settings. For later updates, use [the display's update menu](docs/maintenance.md).

**Disconnected** is normal before you select a bike. **Reconnecting** means the display is trying a saved connection.

![Ride dashboard in portrait](docs/images/ride-portrait.png)

Screenshots throughout these guides use simulated values. The simulator does not connect to or control a real bike.

## 2. Choose your bike type

Tap the cog in the upper-right corner, then open **Settings > Vehicle > Vehicle integration** before connecting. Leave it at **Auto** unless you need a specific integration.

| Integration | Available features |
| --- | --- |
| Auto | Detects WFB or wheel-e; falls back to standard VESC telemetry if neither package is found. |
| Wheelie Fun Bike | Ride data, supported tuning controls and bike locking. |
| wheel-e | Ride data and supported bike locking. Use VESC Tool for wheel-e tuning. |
| VESC | Standard ride data and local display settings. |

The display checks the controller's replies before enabling package features. A Bluetooth device name alone does not identify its installed package.

## 3. Connect your bike

1. Power the bike and make its Bluetooth interface available. Disconnect another phone or app if it occupies the bike's only Bluetooth connection.
2. Open **Settings > Vehicle** and select the bike. Bluetooth Low Energy (BLE) is the default connection.
3. If prompted, enter the pairing PIN configured on the bike's Bluetooth device.
4. Wait for controller discovery to finish and live readings to appear.

The display finds a compatible motor controller automatically, including one reached through the bike's CAN bus. **Auto** works with renamed devices too.

A reading of `--` means the value is missing or out of date; it does not mean zero. If readings do not appear, follow [troubleshooting](docs/troubleshooting.md#the-link-connects-but-values-stay-unavailable). For wired connections or a phone app, see [connections](docs/connections.md).

## 4. Set up the screen

Open Settings with the cog. Choose brightness, speed units and temperature units. Speed and temperature units are independent.

Tap **Settings > Orientation** to cycle through 0°, 90°, 180° and 270°. Touch controls rotate with the screen. These preferences are saved across restarts.

Use the bottom tabs or swipe horizontally:

- **Ride:** speed, battery, power, pitch and connection status.
- **Stats:** detailed readings; scroll vertically for more.
- **Tune:** supported bike settings. Changes remain drafts until you save and confirm them.

See [using the display](docs/user-guide.md) for tuning steps and screen details.

## 5. Add a screen lock (optional)

1. Open **Settings > Screen lock > Set screen lock**.
2. Choose **Use PIN** or **Use pattern**.
3. Enter the six-digit PIN or six-dot pattern twice.

There is no default credential or recovery-code screen. Keep a record of your chosen credential.

**Lock screen** protects the display controls. **Lock bike and screen**, when supported, also asks the bike to lock and waits for confirmation. An unconfirmed request does not establish that the bike is locked.

When the locked screen goes black, tap once to wake it, lift your finger, then unlock. See [PINs, patterns and pairing](docs/pins-and-locking.md) for phone pairing and credential changes.

## 6. Check for updates

Connect under **Settings > Wi-Fi**, then open **Settings > Firmware updates**. Review the version and confirm installation. Keep power connected and restart when offered.

See [updates and maintenance](docs/maintenance.md) for update methods and recovery checks.

## Where next

[Using the display](docs/user-guide.md) · [Connect a phone](docs/connections.md#connect-a-phone-through-the-display) · [Troubleshooting](docs/troubleshooting.md)
