# Using the display

The main screens keep the ride information close and the configuration controls deliberate. The same interface works at 0°, 90°, 180° and 270°. Tap **Settings > Orientation** to advance through those angles; the selection is saved across restarts.

![Landscape ride dashboard](images/ride-landscape.png)

## Ride

Speed uses your saved km/h or mph preference. Temperature units are configured separately. Pitch is shown with the degree symbol. In portrait, TRIP and ODOMETER occupy the upper-left and upper-right corners of the speedometer area. The gauge's layout box follows its actual painted and text bounds rather than a full-circle placeholder. The dashboard uses the selected integration's live VESC data; it does not calculate a substitute speed when messages are missing.

On larger displays such as the GUITION 4.3-inch, landscape Ride places a full-width battery gauge at the top, then a larger speed dial on the left and trip and odometer above the four telemetry cards on the right. Portrait uses larger telemetry cards and distance readings. The arrangement follows the available screen space and selected orientation.

Connection setup and data freshness are separate. A Bluetooth or wired link can be established before valid telemetry arrives. Readings become `--` when they expire, and configuration controls stay unavailable until their requirements are met.

## Stats

The bottom tabs are **Ride**, **Stats** and **Tune**. Stats is a read-only, vertically scrolling view. Wide screens use a three-column overview so more readings fit together; narrower screens keep the grouped two-column cards. It follows your independent speed and temperature units; missing or expired values show `--`.

| Group | Available controller values |
| --- | --- |
| Ride and distance | Speed, power, trip and lifetime distance. |
| Electrical and energy | Voltage, battery charge, input/motor current, duty cycle, eRPM, consumed/charged Ah and Wh. |
| Temperatures and attitude | Controller/motor temperatures, pitch and roll. |
| Controller | PID position, controller ID/count and reported uptime. |

When available, the BMS section adds pack values, cell voltages, balancing state, temperatures, state of charge (SOC) and state of health (SOH). These are read-only standard BMS command 96 replies. The display makes an initial probe when the vehicle becomes Ready, then polls at 1 Hz while Stats is open. While a phone is connected, BMS polling is enabled only after a command-96 reply has established support in the current vehicle session. They are values cached by the VESC, without a BMS-source timestamp: a newly received reply does not prove that the underlying BMS measurement is new. A real BMS has not yet been validated with this display.

![Detailed live stats](images/stats.png)

Scroll further to inspect optional BMS details and cell values:

![Optional BMS pack and temperature values](images/stats-bms.png)

![Optional BMS cell voltages and balancing states](images/stats-cells.png)

## Tune

WFB tuning begins by reading the bike's configuration schema and profile. The display verifies the schema signature and supported encodings before enabling changes. **Read from bike** values remain separate from your drafts; an initially selected preset is never treated as a configuration readback. An unsupported or unreadable schema leaves tuning disabled with a status message and a retry button.

The quick response choices are **Eco**, **Cruise** and **Sport**. Eco sets a 90 A motor-current limit; Cruise and Sport use 150 A, with different throttle ramps. Eco also has a different braking ramp. A read profile that does not match these exact values is labelled **Custom**. The speed-limit slider covers 5–35 mph and a final **No limit** position, restricted by the bike's reported schema. It shows your preferred units, while the bike stores whole mph; km/h steps can therefore vary by one or two.

For individual controls, choose a section:

- **Response:** ten-stop throttle/brake strength sliders cover 60–150 A, with braking stored as a negative current. Responsiveness has ten documented stops, ending with Instant.
- **Button:** choose Wheelie, Turbo, Horn or Disabled. Wheelie angle covers 5–55°; maintain-on-throttle and maintain-on-brake switches apply to the wheelie button.
- **Throttle:** throttle/brake tilt, signed tilt angles (−20 to +20°), anti-loop angle, linked button angle and wheel lift resistance. Linked or disabled controls remain visible with an explanation. Lift resistance is shown as 0–10 and stored as 0–3.
- **Horn:** when Horn is selected as the button action, set strength (1–10) and a musical note from G#3 to B5; the display also shows its frequency.

Tap **Adjust** to open a focused slider editor; quick −/+ controls also work on the Tune card. The editor keeps slider drags separate from page swipes and blocks taps on the page behind it. **Done** returns to Tune without sending changes. Sliders have tick marks and an amber reference-default marker. The readback label remains separate from the draft. A custom value outside the documented stops is shown unchanged; moving its slider explicitly selects a supported stop. The [WFB ride-controls reference](wheelie-ride-controls.md) records the mappings and limits. All controls also respect narrower limits reported by the bike.

![Wheelie tuning controls](images/tune-wheelie.png)

![Focused wheelie angle slider editor](images/tune-slider.png)

Edits stay in the display's draft until you tap Apply or **Save tuning changes** and confirm. **Discard tuning draft** restores the last read profile. Re-reading the bike replaces unsaved drafts with its reported configuration. The save prompt lists the changed values and scrolls when needed; its buttons stay visible inside a solid rounded panel over a dimmed backdrop. Tuning editors and update confirmations use the same panel styling.

![Bike tuning save confirmation](images/tune-confirmation.png)

Before sending, the firmware reads the profile again. If another client changed it, the save is cancelled and the new values are shown for review. Saving requires the same verified vehicle session, an unlocked display, fresh fault-free telemetry and a stopped bike. The display then compares the entire returned profile with the requested settings. **Saved profile verified by readback** confirms those reported values; it is not a power-cycle retention test. A queued write, acknowledgment, timeout or mismatching response is never presented as a verified save.

The phone must be disconnected while reading or saving tuning; ordinary display telemetry continues while the phone proxy is connected. Connecting a phone or changing vehicle session invalidates pending tuning work. An interrupted save that was already transmitted may still have reached the bike; re-read its profile before trying again.

Wheel-e and generic VESC currently have no tuning backend, so these controls remain unavailable for them. The shared tuning UI and transport can support another integration once its schema and write semantics have been implemented and verified.

## Vehicle settings

Leave the integration at **Auto** (the default) to detect a supported package through controller probes. Choose a specific integration to require that package. Disconnect or forget the bike to change the selection. The main-controller discovery status explains which controller the display is using; **Find main controller again** reruns the read-only discovery process when the link is available.

These controls now live under **Settings > Vehicle**. Lock controls and the locked-screen timeout are grouped under **Settings > Screen lock**. Unsupported package features are not enabled by selecting a different page.

![PIN entry screen](images/pin-entry.png)

## Settings

The cog is always in the upper-right header. The vertically scrolling list groups **Display**, **Units**, **Vehicle**, **Screen lock**, **Phone connection** and **System** controls.

![Vehicle controls in Settings](images/vehicle-settings.png)

| Setting | Behavior |
| --- | --- |
| Orientation | Rotates the interface and touch mapping together. |
| Speed units | Switches km/h and mph. |
| Temperature units | Switches °C and °F independently of speed units. |
| Brightness | Sets the brightness restored after waking the locked display. |
| Vehicle connection | Selects BLE, CAN or UART when wired interfaces are configured for the target. |
| BLE name | Active WFB forces `WFB` and hides this control. Wheel-e and generic VESC use the editable saved name (default `WB Disp`). Leaving WFB restores that preference without writing a replacement to NVS. |
| BLE proxy | Enables the phone connection; off by default. |
| Wi-Fi | Scans, connects, forgets a network or enters a hidden SSID. |
| Firmware updates | Checks and installs a matching GitHub release image. |
| About | Shows version, board identity and live diagnostics. |

## Touch and gestures

Horizontal swipes change the three primary pages and follow your finger. Short or reversed drags snap back. Vertical motion scrolls Stats and Tune content rather than changing pages. Moving pages do not accept accidental Apply or device-selection taps.

The layout keeps content inside the panel's rounded corners. Changing orientation cancels a held gesture; lift your finger before starting another touch. The keyboard and PIN entry also respect the safe area.

## Bike BLE signal

The bars beside the phone indicator show the display's BLE signal from the bike, sampled once per second. Fewer bars mean a weaker signal; one active bar is yellow. They are hidden for CAN/UART or while no valid BLE reading is available. Signal strength is not a distance or throughput measurement.

## When a phone is connected

A green phone icon immediately left of the cog indicates an authorized phone connection. The main header text remains the bike status, showing **Connected** for a healthy ready link. Read-only display telemetry continues alongside the phone connection. Phone traffic takes priority between complete packets; an unfinished phone packet or phone firmware transfer can temporarily pause display polls. The display uses its own tracked replies and may also reuse a phone setup reply when its controller identity proves it belongs to the selected bike. Replies from a different CAN controller cannot replace the display's vehicle readings. Configuration writes and display firmware updates remain paused until the phone disconnects. If main-controller discovery had not finished when the phone connected, discovery waits until it disconnects.

The phone chooses its own CAN controller. The proxy never substitutes the display's selected main controller. See [connections](connections.md).
