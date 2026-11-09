# Updates and maintenance

Use the display's Wi-Fi update menu for routine updates. Use the browser installer for a first installation or complete USB recovery.

## Choose an update method

| Method | Use it for | What to expect |
| --- | --- | --- |
| Display Wi-Fi update | Routine updates | Downloads an image for your board; restart when offered. |
| Browser USB installer | First installation or recovery | Installs the full flash layout and can reset saved settings. |
| VESC Tool | Application updates over BLE or CAN | Requires the exact board's application image; restarts automatically after verification. |

## Update from the display

1. Stop the bike, unlock the display and disconnect any phone using its proxy.
2. Open **Settings > Wi-Fi** and connect to a network. You can enter a hidden network name if needed.
3. Open **Settings > Firmware updates** and check for a release.
4. Review the version, confirm installation and keep power connected.
5. Wait for verification, then select **Restart into update** when offered.

Vehicle communications pause during installation. If no public release is available, an image is incompatible or validation fails, the display reports the problem and keeps the installed firmware selected.

Updates come from the project's [GitHub releases](https://github.com/techfoundrynz/wheelie_fun_display/releases). The display checks the download and board identity before activating it. An update that fails early during startup can roll back to the previous image.

## Update progress on the display

Wi-Fi downloads and VESC Tool uploads open a progress panel showing the source, percentage and status. Navigation is blocked during an update.

- **Cancel** is available during transfer, then disabled while verification and activation finish.
- A stopped or rejected update leaves its status visible until you close it.
- A verified Wi-Fi update offers **Restart into update**.
- A verified VESC Tool upload restarts automatically. Cancellation after activation cannot undo the restart.

Screen-lock and Bluetooth pairing prompts take priority over the progress panel.

## USB installation or recovery

1. Read the setup guide for your exact [Waveshare](hardware-waveshare-206.md) or [GUITION](hardware-guition-43.md) board.
2. Open the [browser installer](https://techfoundrynz.github.io/wheelie_fun_display/install/) using a supported desktop browser.
3. Choose the exact model, connect a USB data cable and follow the prompts.
4. After startup, check touch, orientation and saved settings before reconnecting the bike.

The complete USB image includes the bootloader and partition layout. It is different from an application-only update image. GUITION USB installation does not update its C6 radio companion.

## VESC Tool updates

Stop the bike and unlock the display. Download the **application** `.bin` for your exact display target from the release. Do not select a `-factory.bin`, bootloader, partition table or GUITION C6 image.

Choose one connection route:

| Route | Steps |
| --- | --- |
| Display BLE with CAN selected | Enable Phone connection, connect VESC Tool to the display, and use its local Custom Module identity. Selecting another CAN controller targets that controller instead. |
| Through another VESC on CAN | Connect VESC Tool to that VESC, scan the bus and select the display's CAN ID (default 254). The display's CAN transport must be active. |
| Update-only BLE, without a bike or Wi-Fi | Disconnect an existing phone session. Open **Settings > Firmware updates > Start BLE update mode**, then connect VESC Tool to **Wheelie Fun Display OTA**. |

In VESC Tool, open **Firmware > Custom File**, select the matching application image and start the upload. Keep power and the connection available until verification finishes and the display restarts.

Update-only BLE uses the usual phone pairing and screen-lock code. Exiting that mode or restarting restores normal operation without changing the saved phone-connection preference or advertised name.

During upload, bike polling and tuning pause. A BLE disconnect, CAN session change, screen lock, cancellation or 60 seconds of inactivity aborts the transfer. A remote CAN sender disappearing is detected by the inactivity timeout. CAN has no Bluetooth authentication; physical bus access must be trusted.

The updater checks size, checksum, image validity and board identity before selecting the new image. Failed transfers keep the current firmware. Compressed uploads, bootloader writes and broadcast/all-controller flashing are unavailable. Physical BLE and CAN uploads still require hardware acceptance testing.

## Diagnose before resetting

Open **Settings > About** and inspect Diagnostics. Record the firmware version, board identity, uptime and connection information. For a repeatable problem, note the exact steps and save a serial log if available. Never include PINs or Wi-Fi passwords in a report.

Avoid erasing saved settings as a routine fix: this removes preferences, saved bikes, Bluetooth pairings, Wi-Fi details and screen-lock state. Start with [troubleshooting](troubleshooting.md).

## Contributor information

Release publishing, hardware acceptance checks and help-site build instructions are in [releases, validation and Pages](site-maintenance.md). Download validation uses HTTPS and hashes; separate signed firmware and secure boot are not implemented.
