# PINs, patterns and BLE pairing

Use the bike’s Bluetooth PIN to pair the display with the bike. Use your own screen-lock PIN or pattern to unlock the display and pair a phone with it. These are separate credentials.

| PIN | Where it comes from | What it controls |
| --- | --- | --- |
| Upstream BLE pairing PIN | The VESC Express or other secured bike BLE device | This display's encrypted connection to that device. |
| Local screen lock | Six-digit code entered as a PIN or six-dot pattern | Local lock screen, supported bike lock requests and the display's phone BLE proxy. |

The upstream pairing PIN never becomes the proxy PIN.

## Set or change the screen lock

1. Open **Settings > Screen lock > Set screen lock**.
2. Choose **Use PIN** or **Use pattern** on the entry screen.
3. Enter your new six-digit PIN or six-dot pattern twice.

To change or remove it, choose **Change screen lock** or **Remove screen lock** and enter the current credential first. There is no default code or recovery-code screen.

### Enter a PIN or pattern

Both entry modes use the same six-digit credential. A pattern can be entered numerically using its dot numbers. Codes containing zeroes or repeated digits require PIN entry. Switching entry mode clears only the partial entry; it does not change the credential, reset the retry counter or revoke phone bonds.

A PIN always has six digits and submits automatically on the sixth digit. The backspace icon removes one digit; X clears the current entry. The entered digits are visible in large type.

A pattern always uses six different dots on a 3 × 3 grid. Draw in one continuous gesture, then lift your finger to submit. Crossing directly over an unvisited middle dot includes it automatically, as on Android. Dots cannot be reused; short gestures clear without counting as an incorrect credential. Wrong six-dot patterns and wrong PINs clear for another attempt, with a 30-second cooldown after five failures.

![Six-dot pattern entry with numbered dots](images/pattern-entry.png)

The dots are numbered left to right, top to bottom:

```text
1 2 3
4 5 6
7 8 9
```

For example, drawing 1 → 3 → 9 → 8 includes 2 and 6 automatically, producing the six-dot sequence **123698**. This is also the phone proxy's numeric BLE pairing code.

The display remembers your lock and entry mode across restarts. After restarting a locked display, unlock it locally before connecting a phone. Plaintext credentials are not stored or logged; see [locking internals](locking.md) for storage details.

## Lock only the screen

**Lock screen** protects the local controls without sending a bike lock command. The locked-screen timeout can be 15, 30, 60 or 120 seconds, with 30 seconds as the default.

When it times out, the panel becomes black. Tap once to restore your brightness setting, lift your finger, and enter the PIN or draw the pattern. That first tap cannot activate a hidden control. Partial PIN or pattern input is cleared across sleep and wake.

## Lock the bike and screen

When supported, **Lock bike and screen** asks the bike to lock, then locks the display. Stop the bike, disconnect the phone proxy and wait for fresh readings with no controller fault before using it. The screen stays locked while the display waits for the bike to confirm its state.

A command being queued is not proof that propulsion is disabled. If the controller does not confirm the state, the interface reports it as unconfirmed. Validate lock behavior on the exact bike firmware before relying on it. The supplied WFB protocol notes do not establish vendor firmware compatibility by themselves.

## Pair the phone to the display

1. Unlock the display and enable **Settings > Phone connection > BLE proxy**.
2. Find the display in the phone app: **WFB** for an active WFB integration, or your saved **BLE name** for wheel-e/VESC (initially **WB Disp**).
3. Open **Settings > Phone BLE code** and enter the shown code in the phone’s numeric pairing prompt.

For a PIN, this is the same six digits. For a pattern, it is the six dot numbers in order, including automatically inserted middle dots.

To rename the display for wheel-e/VESC, use **Settings > Phone connection > BLE name**. WFB hides that editor; leaving WFB restores your saved name.

![Phone BLE pairing code shown in Settings](images/proxy-pairing-code.png)

The proxy stays paused while the display is locked. This policy applies whether the display connects to the bike over BLE, CAN or UART. With no screen-lock credential, the enabled proxy has no PIN requirement.

### After changing your lock

Changing or removing the credential disconnects phones and clears their remembered pairings on the display. Forget the old pairing in the phone’s Bluetooth settings, then pair again. Switching only the PIN/pattern entry mode preserves pairings for the same credential.

Restarting preserves valid pairings, but you must still unlock locally before a secured phone connection becomes available.

## Pair the display to VESC Express

The VESC Express pairing screen always uses numeric entry and hides the mode switch, regardless of the remembered local entry mode. Enter the passkey configured on that device, then press **Submit**. Entry accepts 1–6 digits representing the BLE passkey range 000000–999999; leading zeroes can be entered or omitted. It never submits automatically on the sixth digit.

![Upstream BLE passkey screen with explicit submission](images/ble-pairing.png)

This upstream passkey never replaces the display's own proxy code. The bike pairing is remembered independently; the entered passkey is cleared after submission. Changing your local PIN or pattern does not change the PIN configured on VESC Express or remove its independent upstream bond.

## Keep the security boundaries clear

The local code protects this display's controls and its proxy. It does not prevent another client from talking directly to the bike's separate interface, and it is not a hardware immobilizer. Physical flash access is outside this local access-control mechanism.

See [locking internals](locking.md) for implementation details and [open work](../TODO.md) for remaining hardware acceptance checks.
