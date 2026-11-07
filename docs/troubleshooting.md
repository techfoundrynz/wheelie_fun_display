# Troubleshooting

Start with what the display reports, then check the part of the connection it describes. Keep the bike stopped while investigating configuration or locking.

## The screen is black

If the display was locked, tap once, lift your finger and wait for the PIN screen. The timeout intentionally sets brightness to zero; waking restores your saved brightness.

For a first installation, confirm the exact board target and that the full USB upload completed. A 1.8-inch or 2.41-inch image is not supported by this repository. Capture the boot log if the correct 2.06-inch build never shows the interface.

## The USB port is missing

Use a USB data cable and try another direct port. Check `pio device list`. Follow the manufacturer's BOOT/RESET download-mode procedure if the board is not enumerating. Do not assume the port number is the same as a previous session.

## No bike appears over BLE

Power the bike and open its BLE interface. Make sure another client has not occupied a single-connection module. Recognizable bike names sort first, but other VESC UART devices can still appear. Leave the integration at **Auto** to probe supported packages regardless of the BLE name. If selecting a package explicitly, check that it is installed on the intended motor controller.

## The link connects but values stay unavailable

Transport connection, main-controller discovery and telemetry freshness are separate. Check the main-controller status on the Settings vehicle section. The package must match WFB or wheel-e when that integration is selected; generic VESC does not expose package-specific controls.

Read-only display polls normally continue while a phone uses the proxy, but unfinished phone packets and phone firmware transfers temporarily suppress them. Initial discovery also waits if the phone connected before it finished. Disconnect the phone to let discovery complete, then use **Find main controller again** if needed. BMS polls during a phone session require a command-96 reply earlier in the current vehicle session. `--` is an unavailable reading, not zero.

## The BLE PIN is rejected

Use the PIN configured on the upstream BLE device when pairing the display to it. Use the local bike lock PIN when pairing the phone to this display. They are separate codes.

If credentials changed, retry pairing with the new upstream PIN. Changing or removing the bike PIN invalidates the phone's old security sessions/bonds. The phone may also need to forget its cached pairing before pairing again.

## CAN or UART is absent from Settings

The target must define both pins for that interface. The stock profile leaves optional wired pins unset. Check the build flags, actual board wiring, CAN transceiver, bus bit rate, unique display CAN ID or UART baud as applicable. Rebuild after changing target configuration.

## A tuning or lock action is unavailable

The selected integration may not support it. Otherwise, wait for fresh stationary telemetry, a ready connection and no controller fault. Configuration writes remain blocked while a phone is connected to the proxy, even though read-only telemetry can continue. Draft values are not readbacks.

For a lock request, an unconfirmed state means the display has not received the required matching telemetry. Do not assume the motor is disabled merely because the request was queued.

## No update is offered

Confirm Wi-Fi and the release feed, and look for a public release containing the exact board asset plus its digest. No release, a missing asset or a validation failure leaves the installed firmware in place. See [maintenance](maintenance.md).

## Report a reproducible problem

Include the board target, firmware version, integration, transport, controller/phone/app versions, the last working behavior, exact steps and a serial log. Never include Wi-Fi passwords or PINs. See [contributing](../CONTRIBUTING.md).
