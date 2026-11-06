# Open work

## Controller and phone acceptance

- [ ] Test WFB and wheel-e discovery and telemetry against the intended controller firmware; compare speed, voltage and angle with the phone app.
- [ ] Confirm the inferred WFB telemetry tail and live command encodings, including LOCK (12), against hardware. Check lock/unlock acknowledgement and persistence across power cycles.
- [ ] Exercise encrypted VESC Express pairing, failed PINs, retry and re-pairing after an upstream PIN change.
- [ ] Check phone proxy interoperability, MTU differences, sustained traffic, lease release and upstream loss over BLE, CAN and UART.
- [ ] Confirm proxy bond revocation after bike PIN changes/removal, PIN persistence, boot unlock and locked-screen timeout/wake behavior.
- [ ] Commission CAN/UART wiring with the required transceiver, controller discovery and multiple-controller routing.

## Device and release acceptance

- [ ] Soak touch, scrolling and all four orientations with concurrent radio traffic; investigate intermittent idle touch I2C NACKs.
- [ ] On the GUITION P4 board, verify DSI scanout, PPA rotation, PSRAM cache synchronization and framebuffer handoff under sustained load; check all four GT911 touch mappings and inspect logs for underruns or tearing.
- [ ] Provision compatible C6 ESP-Hosted firmware, then test Wi-Fi, encrypted upstream BLE, phone proxy and OTA together on the P4/C6 hardware.
- [ ] Measure sustained display responsiveness, internal heap minima and task-stack headroom under realistic load.
- [ ] Exercise USB and battery cold boots, reconnect/stale-data behavior and preference persistence.
- [ ] Test Wi-Fi/OTA success, interruption, cancellation, rollback and first-boot acceptance on the device.
- [ ] Install each supported target through the browser USB installer on its actual board; check chip/revision selection, complete factory layout, boot, touch and documented settings-reset behavior. Keep the P4 C6 provisioning check separate from the P4 application install.
- [ ] Confirm the GitHub Pages deployment and generated screenshots on the default branch.

## Protocol coverage

- [ ] Resolve signature, argument order and scaling uncertainties in the supplied protocol notes before enabling persisted tuning or additional feature/horn commands.
