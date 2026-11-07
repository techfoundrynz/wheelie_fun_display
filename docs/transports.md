# Vehicle transports and controller discovery

Vehicle integrations exchange complete VESC application payloads through `transport::Interface`. BLE, CAN and UART implementations own their wire framing, input buffering and connection generations. Vehicle protocol selection is independent of the physical interface. The shared implementation lives under `firmware/components/bike/`; hardware drivers and transport ownership live under `firmware/main/`.

BLE is the default. **Settings → Vehicle connection** cycles through interfaces available in the target profile. The selection is saved in NVS. An unavailable or invalid saved interface falls back to BLE. A switch invalidates telemetry, controller discovery and queued writes; it does not replay a pending setting change. OTA pauses the selected interface. A phone proxy lease blocks changes to its active upstream.

## Configuring a target

Add definitions to that target's `build_flags` in `platformio.ini`. Both pins in a pair are required. Pins are deliberately absent from the supplied Waveshare 2.06 profile until the wiring is chosen; they are not inferred from the display size or assigned to arbitrary GPIOs.

| Flag | Meaning | Default |
| --- | --- | --- |
| `CAN_TX`, `CAN_RX` | ESP32 TWAI signal pins | Not configured; CAN unavailable |
| `CAN_BITRATE` | CAN bit rate | 500000; also supports 125000, 250000 and 1000000 |
| `CAN_LOCAL_ID` | This display/adapter's CAN address | 254; choose an unused ID from 0–254 |
| `UART_TX`, `UART_RX` | Vehicle serial pins | Not configured; UART unavailable |
| `BIKE_UART_PORT` | ESP32 UART peripheral | 1; supports 1 or 2, reserving UART0 for the console |
| `UART_BAUD` | Serial baud rate | 115200; allowed range 9600–1000000 |

Use GPIO numbers appropriate for the actual board and free of its display/touch/storage connections. CAN requires an external compatible transceiver, common reference and correctly terminated bus; ESP32 TWAI pins are not CAN-H/CAN-L. UART uses compatible logic-level TX/RX, crossed between endpoints, a common reference and matching VESC serial configuration. It is 8N1 without hardware flow control.

## Framing and ownership

BLE and UART use ordinary VESC length/CRC serial framing. Native CAN uses extended frames with `FILL_RX_BUFFER` (5), `FILL_RX_BUFFER_LONG` (6), `PROCESS_RX_BUFFER` (7) and `PROCESS_SHORT_BUFFER` (8). Application UART frames are not sent as CAN payloads. The native receive path checks destination, sender, reply mode, contiguous offsets, bounds, assembly timeout and payload CRC before delivering a packet.

The wired driver runs in a separate task with bounded queues and cancellable writes. CAN uses ESP-IDF 6.1's handle-based `esp_twai` node API. Eight persistent transmit slots retain each frame and its bytes until the completion callback returns ownership; a 64-frame internal queue copies received frames out of the ISR. Callbacks do not run vehicle protocols, allocate memory or block. This internal storage is allocated only while CAN is open. Transmit queue waits are bounded to 20 ms; a frame still pending after 250 ms triggers a session retry. Receive overflow, transmit failure and bus-off also restart the session rather than continuing with an incomplete packet stream. Packet storage and assembly buffers use the configured external-memory allocation policy; queue controls and task stacks remain internal. Driver failures, CAN bus-off, transport changes and reconnects invalidate the session. Late packets and writes from previous sessions are discarded. UART framing, native CAN framing and the phone's raw stream have separate parser state.

## Finding the main controller

The display performs read-only discovery before normal polling or configuration writes:

1. On BLE/UART, request `COMM_PING_CAN` (62), including the directly attached controller as a candidate. Native CAN sweeps controller IDs with CAN ping (17) and collects pong (18) responses addressed to this display.
2. Query `COMM_FW_VERSION` (0) for candidates. Serial forwarding uses `COMM_FORWARD_CAN` (34); native CAN addresses the candidate directly.
3. Exclude non-motor hardware such as BMS/custom adapters, then verify the selected vehicle integration with its telemetry request. Custom configuration metadata and a successful package reply identify installed package support.
4. Use the **first** compatible controller with a custom package/configuration. There is no multiple-controller choice prompt. Explicit generic VESC mode may fall back to a plain motor controller for read-only standard telemetry when no package controller is found.

Firmware identity parsing is bounded and handles the hardware name, UUID, version, hardware type and custom-configuration count. Queries are serialized, with quiet/settling periods because serial CAN-forwarded replies do not carry a request/source identifier. Native CAN replies retain their source ID. Bounded discovery retries handle missing devices; reconnects discard the previous discovery result. **Settings → Vehicle → Find main controller again** restarts discovery. The Settings vehicle section reports the selected controller and firmware.

## Transparent phone BLE proxy

The optional proxy is a separate VESC client path. It does not use the display's main-controller route or vehicle integration to rewrite the phone's commands. Read-only display polling continues through `transport::poll` while the phone is connected. Phone traffic has priority at complete-packet boundaries, with at most one broker-owned display poll in flight. Broker-owned replies update display telemetry and are not forwarded to the phone. Phone command-47 replies may also supply a passive read-only view when their controller ID matches the selected bike; native CAN additionally requires the matching source controller. Other phone replies are excluded. This keeps the phone's independently selected CAN controller separate from the display's route. Initial controller discovery still waits for the phone to disconnect if it was incomplete at connection time. Configuration writes and display OTA remain paused. On normal phone departure, admitted valid frames finish, a never-forwarded partial tail is discarded, and backend drain plus quiet receive allow normal display operation to resume without resetting the bike BLE session or main-controller route. Source loss or corrupt traffic can still require recovery.

The relay uses three 16 KiB PSRAM byte rings and validates each complete phone frame before forwarding any of its bytes. The 16 KiB limit includes framing; ordinary VESC payloads are 512 bytes. Up to 512-byte batches coalesce valid frames while preserving boundaries and reply-key/firmware conflict guards. Invalid or oversized input ends the phone session before that frame reaches the bike. Phone notification backpressure is serviced nonblockingly. Idle, pairing or unsubscribed phones do not suppress local read-only polls; unauthenticated phones receive no data.

| Vehicle connection | Phone proxy behavior |
| --- | --- |
| BLE | Raw ordered byte stream to the connected BLE UART peer |
| UART | Raw ordered byte stream to the configured serial port |
| CAN | Generic adapter endpoint: firmware identity, CAN discovery and explicit CAN forwarding |

On CAN, a direct firmware query identifies a custom adapter, not a motor controller. `COMM_PING_CAN` lists responding nodes. `COMM_FORWARD_CAN` sends the phone's payload to its explicitly requested CAN ID and returns the unmodified VESC reply inside serial framing. A direct motor command without a CAN destination is not implicitly routed to the display's selected bike; the adapter tells the app to select a CAN controller. Native CAN packet size limits and CRC framing apply. The phone app performs its own controller discovery/selection, as with a CAN-capable VESC adapter.

Only the active WFB integration forces the advertised/GAP name to **WFB** and hides its editor. Wheel-e and generic VESC use the editable saved name (default **WB Disp**). Leaving WFB restores that preference without mutating NVS. The name never mirrors an upstream BLE module. The bike lock PIN or pattern controls the proxy's pairing requirement; the upstream BLE PIN is independent. See [Bluetooth](bluetooth.md) and [PINs and locking](pins-and-locking.md).

## References and verification

Native packet formats and firmware discovery were checked against [VESC firmware 21e66bb](https://github.com/vedderb/bldc/tree/21e66bb9efb41322c84f34c1db03a71ffeb7d8a1), particularly [CAN transport](https://github.com/vedderb/bldc/blob/21e66bb9efb41322c84f34c1db03a71ffeb7d8a1/comm/comm_can.c) and [command responses](https://github.com/vedderb/bldc/blob/21e66bb9efb41322c84f34c1db03a71ffeb7d8a1/comm/commands.c). Hardware uses the [ESP-IDF TWAI driver](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/peripherals/twai.html).

Host tests exercise CAN boundaries and CRCs, missing/reordered/expired fragments, wrong destinations/senders, first-package discovery, BMS exclusion, native bridge commands and PIN-policy changes. Driver ownership tests cover pending transmit storage, completion reuse, receive overflow, stalled transfers, startup failures and cancellation under sanitizers. Software-renderer UI tests cover interface selection/availability and all four rotations. Real CAN/UART wiring, bus load, phone-app interoperability and encrypted pairing remain hardware acceptance tests; build success does not establish those results.
