# PINs and locking

There are two separate PIN purposes: connecting this display to a secured BLE device, and protecting this display plus its phone proxy.

| PIN | Where it comes from | What it controls |
| --- | --- | --- |
| Upstream BLE pairing PIN | The VESC Express or other secured bike BLE device | This display's encrypted connection to that device. |
| Bike lock PIN | Six digits set in the Bike menu | Local lock screen, supported bike lock requests and the display's phone BLE proxy. |

The upstream pairing PIN never becomes the proxy PIN.

## Set or change the bike PIN

Open **Bike > Set lock PIN**, enter six digits and confirm them. If a PIN already exists, Change PIN and Remove PIN require the current code first. No default PIN or recovery-code screen is provided.

The persistent record uses a salted PIN digest. The numeric proxy passkey is held in RAM after PIN setup or successful local unlock. Boot with a configured PIN requires local unlock before the secured proxy becomes available; the upstream pairing PIN itself is not saved.

## Lock only the screen

**Lock screen** protects the local controls without sending a bike lock command. The locked-screen timeout can be 15, 30, 60 or 120 seconds, with 30 seconds as the default.

When it times out, the panel becomes black. Tap once to restore your brightness setting, lift your finger, and enter the PIN. That first tap cannot activate a hidden control. Partial PIN input is cleared across sleep and wake.

## Lock the bike and screen

When supported, **Lock bike and screen** requests the package's explicit LOCK command. The bike must have fresh stationary telemetry, no controller fault and a ready exclusive connection. The screen stays locked while the display waits for matching telemetry from the same connection.

A command being queued is not proof that propulsion is disabled. If the controller does not confirm the state, the interface reports it as unconfirmed. Validate lock behavior on the exact bike firmware before relying on it. The supplied WFB protocol notes do not establish vendor firmware compatibility by themselves.

## Pair the phone

With no bike PIN, the enabled proxy has no PIN requirement. With a bike PIN, the phone must complete authenticated BLE pairing using that code. Changing or removing the code disconnects old security sessions and removes old phone bonds; pair again when prompted.

Upstream bike bonds are independent. Changing the local PIN does not change the PIN configured on VESC Express.

## Keep the security boundaries clear

The local code protects this display's controls and its proxy. It does not prevent another client from talking directly to the bike's separate interface, and it is not a hardware immobilizer. Physical flash access is outside this local access-control mechanism.

See [locking internals](locking.md) for implementation details and [open work](../TODO.md) for remaining hardware acceptance checks.
