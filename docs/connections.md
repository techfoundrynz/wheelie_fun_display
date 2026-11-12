# Connections and the phone proxy

The display needs one connection to the bike. You can also connect a phone app through the display by enabling its **BLE proxy**: the display acts as a bridge between your phone and the bike.

## Choose a connection

| Connection | What you need |
| --- | --- |
| Bluetooth Low Energy (BLE) | A compatible bike Bluetooth interface. Available by default. |
| CAN | A configured firmware build, an external CAN transceiver and matching bus wiring. |
| UART | A configured firmware build and compatible serial wiring. |

Choose the interface under **Settings > Vehicle**. The vehicle integration (Auto, WFB, wheel-e or VESC) is a separate choice in that section. Leave integration at **Auto** for automatic detection.

## Bluetooth to the bike

1. Power the bike and make its Bluetooth interface available.
2. Open **Settings > Vehicle** and select the device. Recognizable bike names appear first; other compatible VESC Bluetooth devices are still listed.
3. Complete any pairing prompt with the PIN configured on the bike's Bluetooth device.
4. Wait for controller discovery and live readings before using bike controls.

![Bike Bluetooth pairing prompt](images/ble-pairing.png)

The display remembers the verified bike and integration. If the bike's Bluetooth PIN changes, pair again using its new PIN. Your display's screen-lock code is separate.

## Finding the main controller

The display searches the directly connected controller and controllers reachable over CAN. It selects the first compatible motor controller; a BMS is not selected as the motor controller.

With **Auto**, it checks for WFB and wheel-e packages before falling back to standard VESC readings. Selecting WFB or wheel-e explicitly requires that package to respond. A device name does not enable package-specific features.

After a successful connection, the display remembers the controller route, UUID and verified package for that transport and Bluetooth peer. Reconnecting checks that controller and package first, so a known wheel-e bike does not wait for a WFB probe or another CAN scan. The controller UUID and live package response must still match before the connection becomes ready. A missing or changed controller or package falls back to the full search. Generic VESC in Auto still uses the full package search so another controller's WFB or wheel-e package is not hidden.

Check discovery status under **Settings > Vehicle**. Use **Find main controller again** to repeat discovery when the link is available. If a phone connected before discovery finished, disconnect the phone first.

## Native CAN

CAN appears only when the firmware has both CAN pins configured. The default bus speed is **500 kbit/s** and the display's default node ID is **254**. The bus speed must match the bike, and the display ID must be unique on that bus.

Use a CAN transceiver and the wiring for your exact board. The motor-controller ID is discovered automatically. For the GUITION breakout, see [connector pinouts and setup](../pcb/guition-43/README.md). For custom wiring, see [wired-interface build configuration](building.md#configure-an-optional-wired-interface).

## UART

UART appears only when both UART pins are configured in the firmware. The default baud rate is **115200** and must match the bike.

Use compatible logic levels, connect TX to RX and RX to TX as required, and share a signal ground. Do not guess GPIOs from the screen model. Vehicle UART normally uses port 1; port 0 is reserved for the console. See [wired-interface build configuration](building.md#configure-an-optional-wired-interface).

## Connect a phone through the display

1. Unlock the display and enable **Settings > Phone connection > BLE proxy**. It is off by default.
2. In your phone app, find the display using the name below.
3. If a screen lock is configured, open **Settings > Phone BLE code** after unlocking and enter that code in the phone's pairing prompt. A pattern uses its six dot numbers. See [PINs, patterns and pairing](pins-and-locking.md).
4. Run the phone app's normal VESC discovery and select the controller you want to use.
5. Disconnect the phone before reading or saving tuning, requesting a bike lock, or updating the display.

| Active integration | Display name shown to the phone |
| --- | --- |
| WFB | `WFB`; the name editor is hidden. |
| wheel-e or VESC | Your saved BLE name, initially `WB Disp`. Edit it under **Settings > Phone connection > BLE name**. |

Leaving WFB restores your saved name. The display's name does not copy the bike's Bluetooth name.

## While the phone is connected

The green phone icon beside the cog indicates an authorized phone connection. Read-only ride data normally continues. Busy phone traffic or a phone firmware transfer can temporarily pause display readings. Tuning and display firmware updates wait until the phone disconnects.

The phone selects its own CAN controller; it does not automatically use the display's selected controller. Readings from another controller do not replace the display's bike readings. The bridge works with BLE, UART or CAN on the bike side.

The proxy pauses while the screen is locked. With a configured lock, unlock locally after each display restart before connecting the phone. Without a screen lock, the enabled proxy has no PIN requirement.

## Pairing and changed PINs

Changing or removing the screen-lock credential disconnects phone sessions and clears remembered phone pairings on the display. If the phone still remembers the old pairing, forget the display in the phone's Bluetooth settings and pair again.

Changing only the entry mode between PIN and pattern preserves pairings for the same credential. The bike's own Bluetooth pairing remains separate.

Phone pairing and wired operation still need validation with your exact hardware and app. [Troubleshooting](troubleshooting.md) covers common connection problems; [Bluetooth internals](bluetooth.md) and [transport internals](transports.md) explain implementation details.
