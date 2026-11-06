#include "bike/can_bridge.hpp"
#include "bike/can_packet.hpp"
#include "bike/controller_discovery.hpp"
#include "bike/security_policy.hpp"
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>
#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)
using Bytes = std::vector<uint8_t>;

Bytes firmware(uint8_t uuid, uint8_t type = 0, uint8_t configs = 1) {
  Bytes bytes{0, 6, 6, 'V', 'E', 'S', 'C', 0};
  for (int i = 0; i < 12; ++i)
    bytes.push_back(uuid + i);
  bytes.insert(bytes.end(), {0, 0, type, configs});
  return bytes;
}

Bytes wfb_sample() {
  Bytes bytes(48);
  bytes[0] = 36;
  bytes[1] = 102;
  bytes[2] = 10;
  bytes[3] = 1;
  return bytes;
}

Bytes wheel_e_info(std::string_view name = "Wheel-E") {
  Bytes bytes(61);
  bytes[0] = 36;
  bytes[1] = 101;
  bytes[2] = 0;
  bytes[3] = 2;
  std::copy_n(name.begin(), std::min<size_t>(name.size(), 20), bytes.begin() + 5);
  bytes[25] = 1;
  bytes[26] = 3;
  bytes[54] = 0x27;
  bytes[55] = 0x10;
  return bytes;
}

Bytes wheel_e_sample() {
  Bytes bytes(28);
  bytes[0] = 36;
  bytes[1] = 101;
  bytes[2] = 33;
  bytes[3] = 1;
  bytes[5] = 0x0a;
  bytes[7] = 2;
  return bytes;
}

Bytes setup_sample() {
  Bytes bytes(56);
  bytes[0] = 47;
  return bytes;
}

void codec_tests() {
  for (size_t size :
       {size_t(1), size_t(6), size_t(7), size_t(255), size_t(256), size_t(259), size_t(260), bike::max_payload}) {
    Bytes payload(size);
    std::iota(payload.begin(), payload.end(), uint8_t(0));
    std::vector<bike::CanFrame> frames;
    CHECK(bike::send_can_packet(payload, 12, 254, true, [&](auto frame) {
      frames.push_back(frame);
      return true;
    }));
    bike::CanReassembler decoder;
    std::span<const uint8_t> result;
    uint64_t now = 1;
    for (auto frame : frames)
      result = decoder.feed(frame, 254, 12, now++);
    CHECK(Bytes(result.begin(), result.end()) == payload);
    CHECK(frames.back().id == ((size <= 6 ? 8u : 7u) << 8 | 254));
    if (size > 6) {
      CHECK(frames.front().data[0] == 0 && frames.front().size == 8);
      auto bad = frames.back();
      bad.data[5] ^= 1;
      for (size_t i = 0; i + 1 < frames.size(); ++i)
        decoder.feed(frames[i], 254, 12, now++);
      CHECK(decoder.feed(bad, 254, 12, now++).empty());
      // Missing/reordered fragments and timeout cannot deliver stale payloads.
      for (size_t i = 1; i < frames.size(); ++i)
        CHECK(decoder.feed(frames[i], 254, 12, now++).empty());
      decoder.feed(frames.front(), 254, 12, 1000);
      for (size_t i = 1; i < frames.size(); ++i)
        CHECK(decoder.feed(frames[i], 254, 12, 1600 + i).empty());
    }
    for (auto frame : frames)
      CHECK(decoder.feed(frame, 253, 12, now++).empty());
    for (auto frame : frames)
      CHECK(decoder.feed(frame, 254, 11, now++).empty());
  }
  const Bytes command{36, 102, 12, 1};
  std::vector<bike::CanFrame> frames;
  CHECK(bike::send_can_packet(command, 254, 5, false, [&](auto frame) {
    frames.push_back(frame);
    return true;
  }));
  CHECK(frames.size() == 1 && frames[0].id == 0x805 && frames[0].data[0] == 254 && frames[0].data[1] == 0);
  CHECK(std::equal(command.begin(), command.end(), frames[0].data.begin() + 2));
  bike::CanReassembler decoder;
  CHECK(decoder.feed(frames[0], 5, 254, 1).empty()); // Requests are never interpreted as replies.
  CHECK(!bike::send_can_packet(command, 255, 5, false, [](auto) { return true; }));
  CHECK(!bike::send_can_packet(command, 5, 5, false, [](auto) { return true; }));
  CHECK(!bike::send_can_packet(Bytes(bike::max_payload + 1), 254, 5, false, [](auto) { return true; }));
  CHECK(!bike::send_can_packet(Bytes(1024), 254, 5, false, [](auto) { return false; }));
}

void bridge_tests() {
  bike::ProxyAuthorization authorization{false, true, 1, 1, 1};
  CHECK(authorization.allows(false, false));
  authorization.pin_required = true;
  CHECK(!authorization.allows(false, false));
  CHECK(!authorization.allows(true, false));
  CHECK(authorization.allows(true, true));
  ++authorization.revision; // New/changed bike PIN immediately rejects the old phone bond.
  CHECK(!authorization.allows(true, true));
  authorization.applied_revision = authorization.revision;
  CHECK(!authorization.allows(true, true)); // A previously encrypted connection is still stale.
  authorization.connection_revision = authorization.revision;
  CHECK(authorization.allows(true, true));
  authorization.pin_known = false; // Boot with hashed PIN: user must unlock before BLE can use that PIN.
  CHECK(!authorization.allows(true, true));
  authorization.pin_known = true;
  authorization.pin_required = false;
  ++authorization.revision;
  authorization.applied_revision = authorization.revision;
  CHECK(!authorization.allows(true, true)); // Removing a PIN also invalidates the old connection.
  authorization.connection_revision = authorization.revision;
  CHECK(authorization.allows(false, false));
  CHECK(bike::valid_proxy_name("WB Disp"));
  CHECK(!bike::valid_proxy_name(""));
  CHECK(!bike::valid_proxy_name("bad\nname"));
  CHECK(!bike::valid_proxy_name(std::string(30, 'x')));
  const Bytes forward{34, 42, 36, 101, 12, 1};
  const auto request = bike::can_bridge_request(forward, 254);
  CHECK(request.operation == bike::BridgeOperation::Forward && request.destination == 42);
  CHECK(Bytes(request.payload.begin(), request.payload.end()) == Bytes({36, 101, 12, 1}));
  CHECK(bike::can_bridge_request(Bytes{0}, 254).operation == bike::BridgeOperation::Firmware);
  CHECK(bike::can_bridge_request(Bytes{62}, 254).operation == bike::BridgeOperation::Scan);
  CHECK(bike::can_bridge_request(Bytes{47}, 254).operation == bike::BridgeOperation::Unsupported);
  CHECK(bike::can_bridge_request(Bytes{34, 254, 0}, 254).operation == bike::BridgeOperation::Unsupported);
  CHECK(bike::can_bridge_request(Bytes{34, 255, 0}, 254).operation == bike::BridgeOperation::Unsupported);
  CHECK(bike::can_bridge_request({}, 254).operation == bike::BridgeOperation::Unsupported);
  CHECK(bike::can_bridge_request(Bytes{34, 42}, 254).operation == bike::BridgeOperation::Unsupported);
  std::array<uint8_t, 12> uuid{};
  std::array<uint8_t, 96> firmware{};
  const auto length = bike::can_bridge_firmware(firmware, uuid);
  CHECK(length > 30 && firmware[0] == 0);
  const auto end = std::find(firmware.begin() + 3, firmware.end(), 0);
  const auto extra = size_t(end - firmware.begin()) + 13;
  CHECK(firmware[extra + 2] == 2 && firmware[extra + 3] == 0); // Generic adapter, no motor/package identity.
  CHECK(bike::can_bridge_firmware(std::span(firmware).first(10), uuid) == 0);
  std::vector<bike::CanFrame> frames;
  CHECK(bike::send_can_packet(Bytes{0, 6, 6}, 42, 254, true, [&](auto frame) {
    frames.push_back(frame);
    return true;
  }));
  bike::CanReassembler decoder;
  const auto payload = decoder.feed(frames.front(), 254, -1, 1);
  CHECK(Bytes(payload.begin(), payload.end()) == Bytes({0, 6, 6}) && decoder.sender() == 42);
}

void discovery_tests() {
  bike::ControllerDiscovery discovery;
  bike::DiscoveryRequest request;
  discovery.start(false, bike::IntegrationId::Wfb, 0);
  CHECK(discovery.next(0, request) && request.scan && request.bytes[0] == 62);
  const Bytes scan{62, 1, 2, 3};
  discovery.feed(scan, -2, 1);
  CHECK(discovery.next(151, request) && request.route == -1 && request.bytes[0] == 0);
  discovery.feed(firmware(1, 0, 0), -2, 152);
  CHECK(discovery.next(302, request) && request.bytes[0] == 36);
  // Direct controller has no matching package; move to the CAN candidates.
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request) && request.route == 1);
  discovery.feed(firmware(2, 1), -2, 1104); // BMS: skip without package writes/probes.
  CHECK(discovery.next(1254, request) && request.route == 2);
  discovery.feed(firmware(3), -2, 1255);
  CHECK(discovery.next(1405, request) && request.route == 2 && request.bytes[1] == 102);
  discovery.feed(wfb_sample(), -2, 1406);
  CHECK(discovery.complete() && discovery.chosen() >= 0);
  CHECK(discovery.controller(discovery.chosen()).route == 2);
  CHECK(!discovery.next(9999, request)); // First verified package wins, with no user prompt.

  discovery.start(true, bike::IntegrationId::Wfb, 0);
  CHECK(discovery.next(0, request) && request.scan);
  const uint8_t ids[]{3, 4};
  discovery.scanned(ids, false, 1);
  CHECK(discovery.next(151, request) && request.route == 3);
  discovery.feed(firmware(3), 4, 152); // Native CAN reply from another controller is ignored.
  CHECK(!discovery.next(200, request));
  discovery.feed(firmware(3), 3, 201);
  CHECK(discovery.next(351, request));
  discovery.feed(wfb_sample(), 3, 352);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 3);

  discovery.start(true, bike::IntegrationId::Vesc, 0);
  discovery.scanned(ids, false, 1);
  CHECK(discovery.next(151, request));
  discovery.feed(firmware(3, 0, 0), 3, 152);
  CHECK(discovery.next(302, request) && request.bytes[0] == 47);
  discovery.feed(setup_sample(), 3, 303);
  CHECK(!discovery.complete()); // Prefer an installed custom package over a plain motor controller.
  CHECK(discovery.next(453, request) && request.route == 4);
  discovery.feed(firmware(4), 4, 454);
  CHECK(discovery.next(604, request));
  discovery.feed(setup_sample(), 4, 605);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 4);

  discovery.start(false, bike::IntegrationId::Wfb, 0);
  CHECK(discovery.next(0, request));
  CHECK(!discovery.next(7150, request));
  CHECK(discovery.next(7300, request) && request.route == -1); // CAN ping timeout still probes the direct controller.
  discovery.feed(Bytes{0, 6, 6, 'N', 'O'}, -2, 7301);          // Unterminated/truncated firmware identity is rejected.
  CHECK(!discovery.next(7951, request));
  CHECK(discovery.complete() && discovery.chosen() < 0);
  discovery.start(true, bike::IntegrationId::Wfb, 0);
  discovery.scanned({}, false, 1);
  CHECK(discovery.complete() && discovery.chosen() < 0); // Reconnect discards earlier identity/route.
}

void auto_discovery_tests() {
  bike::ControllerDiscovery discovery;
  bike::DiscoveryRequest request;
  CHECK(bike::integration_id(-1) == bike::IntegrationId::Auto);
  CHECK(bike::integration_id(255) == bike::IntegrationId::Auto);
  CHECK(!bike::integration(bike::IntegrationId::Auto).features().lock);

  auto direct = [&] {
    discovery.start(false, bike::IntegrationId::Auto, 0);
    CHECK(discovery.next(0, request) && request.scan);
    discovery.feed(Bytes{62}, -2, 1);
    CHECK(discovery.next(151, request) && request.bytes[0] == 0);
    discovery.feed(firmware(1), -2, 152);
    CHECK(discovery.next(302, request) && request.bytes[0] == 36 && request.bytes[1] == 102);
  };
  direct();
  // Neither a firmware custom-config flag nor standard telemetry proves a WFB package.
  discovery.feed(setup_sample(), -2, 303);
  discovery.feed(wheel_e_sample(), -2, 304); // A reply for a probe not sent yet is ignored.
  CHECK(!discovery.complete());
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request) && request.bytes[1] == 101 && request.bytes[2] == 0);
  discovery.feed(wheel_e_sample(), -2, 1104);
  CHECK(!discovery.complete());
  discovery.feed(wheel_e_info(), -2, 1105);
  CHECK(discovery.next(1255, request) && request.bytes[2] == 33);
  auto malformed = wheel_e_sample();
  malformed[7] = 0;
  discovery.feed(malformed, -2, 1256);
  CHECK(!discovery.complete());
  discovery.feed(wheel_e_sample(), -2, 1257);
  CHECK(discovery.complete() && discovery.chosen() >= 0);
  CHECK(discovery.controller(discovery.chosen()).integration == bike::IntegrationId::WheelE);
  CHECK(discovery.selection() == bike::IntegrationId::Auto); // Keep re-probing policy independent of detected package.

  direct(); // A reconnect must probe again rather than restore a detected package as a manual choice.
  discovery.feed(wfb_sample(), -2, 303);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Wfb);

  direct();
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request) && request.bytes[1] == 101);
  CHECK(!discovery.next(1754, request));
  CHECK(discovery.next(1904, request) && request.bytes[0] == 47);
  discovery.feed(setup_sample(), -2, 1905);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Vesc);
  CHECK(!bike::integration(discovery.controller(discovery.chosen()).integration).features().lock);

  direct();
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request));
  CHECK(!discovery.next(1754, request));
  CHECK(discovery.next(1904, request));
  CHECK(!discovery.next(2555, request));
  CHECK(discovery.complete() && discovery.chosen() < 0); // No valid package or VESC telemetry: no ready controller.

  // Auto must continue beyond an unrelated custom configuration to the first known package.
  discovery.start(true, bike::IntegrationId::Auto, 0);
  const uint8_t ids[]{3, 4};
  discovery.scanned(ids, false, 1);
  CHECK(discovery.next(151, request) && request.route == 3);
  discovery.feed(firmware(3), 3, 152);
  CHECK(discovery.next(302, request));
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request));
  CHECK(!discovery.next(1754, request));
  CHECK(discovery.next(1904, request) && request.bytes[0] == 47);
  discovery.feed(setup_sample(), 3, 1905);
  CHECK(!discovery.complete());
  CHECK(discovery.next(2055, request) && request.route == 4);
  discovery.feed(firmware(4), 4, 2056);
  CHECK(discovery.next(2206, request) && request.bytes[1] == 102);
  discovery.feed(wfb_sample(), 4, 2207);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 4);
  CHECK(discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Wfb);

  // The same controller may be reachable directly and over CAN. Prefer the
  // route that verified its custom package over an earlier generic-only path.
  discovery.start(false, bike::IntegrationId::Auto, 0);
  CHECK(discovery.next(0, request) && request.scan);
  discovery.feed(Bytes{62, 7}, -2, 1);
  CHECK(discovery.next(151, request));
  discovery.feed(firmware(9), -2, 152);
  CHECK(discovery.next(302, request));
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request));
  CHECK(!discovery.next(1754, request));
  CHECK(discovery.next(1904, request));
  discovery.feed(setup_sample(), -2, 1905);
  CHECK(discovery.next(2055, request) && request.route == 7);
  discovery.feed(firmware(9), -2, 2056);
  CHECK(discovery.next(2206, request));
  discovery.feed(wfb_sample(), -2, 2207);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 7);
  CHECK(discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Wfb);
  CHECK(discovery.controller(0).duplicate);

  discovery.start(false, bike::IntegrationId::Wfb, 0);
  CHECK(discovery.next(0, request) && request.scan);
  discovery.feed(Bytes{62}, -2, 1);
  CHECK(discovery.next(151, request));
  discovery.feed(firmware(1), -2, 152);
  CHECK(discovery.next(302, request));
  discovery.feed(wheel_e_sample(), -2, 303);
  CHECK(!discovery.next(953, request));
  CHECK(discovery.complete() && discovery.chosen() < 0); // Explicit selections never switch packages or fall back.
}

bike::ControllerHint hint_for(int route, uint8_t uuid) {
  bike::ControllerHint hint;
  hint.route = route;
  for (size_t i = 0; i < hint.uuid.size(); ++i)
    hint.uuid[i] = uint8_t(uuid + i);
  return hint;
}

void hint_codec_tests() {
  bike::HintRecord record{{0, bike::IntegrationId::Auto, {1, 2, 3, 4, 5, 6, 7}}, hint_for(7, 1)};
  const auto bytes = bike::encode_hint(record);
  auto decoded = bike::decode_hint(bytes);
  CHECK(decoded && decoded->context == record.context && decoded->controller == record.controller);
  for (size_t size = 0; size < bytes.size(); ++size)
    CHECK(!bike::decode_hint(std::span(bytes).first(size)));
  Bytes extra(bytes.begin(), bytes.end());
  extra.push_back(0);
  CHECK(!bike::decode_hint(extra));
  for (auto [position, value] : {std::pair<size_t, uint8_t>{0, 2}, {1, 3}, {2, 3}, {3, 4}}) {
    auto corrupt = bytes;
    corrupt[position] = value;
    CHECK(!bike::decode_hint(corrupt));
  }
  auto other = record.context;
  other.peer[6] ^= 1;
  CHECK(other != record.context);
  other = record.context;
  other.selection = bike::IntegrationId::Wfb;
  CHECK(other != record.context);
  for (int route : {-2, 255}) {
    auto invalid = record;
    invalid.controller.route = route;
    CHECK(!bike::valid_hint(invalid) && !bike::decode_hint(bike::encode_hint(invalid)));
  }
  for (uint8_t fill : {uint8_t(0), uint8_t(255)}) {
    auto invalid = record;
    invalid.controller.uuid.fill(fill);
    CHECK(!bike::valid_hint(invalid));
  }
  auto empty_peer = record;
  empty_peer.context.peer.fill(0);
  CHECK(!bike::valid_hint(empty_peer));
  auto can = record;
  can.context.transport = 1;
  CHECK(!bike::valid_hint(can)); // A wired hint must not carry a stale BLE association.
  can.context.peer.fill(0);
  can.controller.route = -1;
  CHECK(!bike::valid_hint(can));
  can.controller.route = 0;
  CHECK(bike::decode_hint(bike::encode_hint(can)));
  auto corrupt_can = bike::encode_hint(can);
  corrupt_can[10] = 0;
  CHECK(!bike::decode_hint(corrupt_can));
  corrupt_can = bike::encode_hint(can);
  corrupt_can[3] = 1;
  CHECK(!bike::decode_hint(corrupt_can));
  auto uart = can;
  uart.context.transport = 2;
  uart.controller.route = -1;
  CHECK(bike::decode_hint(bike::encode_hint(uart)));
}

void cached_discovery_tests() {
  bike::ControllerDiscovery discovery;
  bike::DiscoveryRequest request;
  const auto hint = hint_for(7, 7);
  auto begin = [&](bike::IntegrationId choice) {
    discovery.start(true, choice, 0, hint);
    CHECK(!discovery.next(0, request));
    CHECK(discovery.next(150, request) && !request.scan && request.route == 7 && request.bytes[0] == 0);
    CHECK(!discovery.complete()); // A cached identity alone never makes the link ready.
    discovery.feed(firmware(7, 0, 0), 7, 151);
    CHECK(!discovery.complete());
    CHECK(discovery.next(301, request));
  };
  begin(bike::IntegrationId::Auto);
  CHECK(request.bytes[1] == 102);
  discovery.feed(wfb_sample(), 7, 302);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 7);
  CHECK(discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Wfb);
  CHECK(discovery.selection() == bike::IntegrationId::Auto);

  begin(bike::IntegrationId::WheelE);
  CHECK(request.bytes[1] == 101 && request.bytes[2] == 0);
  discovery.feed(wfb_sample(), 7, 302);
  CHECK(!discovery.complete());
  discovery.feed(wheel_e_info(), 7, 303);
  CHECK(!discovery.complete());
  CHECK(discovery.next(453, request) && request.bytes[2] == 33);
  discovery.feed(wheel_e_sample(), 7, 454);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).integration == bike::IntegrationId::WheelE);

  begin(bike::IntegrationId::Vesc);
  CHECK(request.bytes[0] == 47);
  discovery.feed(setup_sample(), 7, 302);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Vesc);

  // Same route on a different vehicle is not a cache hit: UUID is mandatory.
  discovery.start(true, bike::IntegrationId::Wfb, 0, hint);
  CHECK(discovery.next(150, request) && request.route == 7);
  discovery.feed(firmware(8), 7, 151);
  CHECK(!discovery.complete() && !discovery.awaiting_scan());
  CHECK(discovery.next(152, request) && request.scan && discovery.awaiting_scan());
  const uint8_t ids[]{9};
  discovery.scanned(ids, false, 153);
  CHECK(discovery.next(303, request) && request.route == 9 && request.bytes[0] == 0);
  discovery.feed(firmware(9), 9, 304);
  CHECK(discovery.next(454, request) && request.bytes[1] == 102);
  discovery.feed(wfb_sample(), 9, 455);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 9);

  begin(bike::IntegrationId::Wfb);
  discovery.feed(wheel_e_sample(), 7, 302);
  CHECK(!discovery.complete());
  CHECK(!discovery.next(952, request));
  CHECK(discovery.next(953, request) && request.scan); // Wrong or removed package triggers full discovery.

  // Auto cannot use a generic-only cache hit to hide another controller's custom package.
  begin(bike::IntegrationId::Auto);
  CHECK(!discovery.next(952, request));
  CHECK(discovery.next(1102, request) && request.bytes[1] == 101);
  CHECK(!discovery.next(1753, request));
  CHECK(discovery.next(1903, request) && request.bytes[0] == 47);
  discovery.feed(setup_sample(), 7, 1904);
  CHECK(!discovery.complete());
  CHECK(discovery.next(1905, request) && request.scan);
  discovery.scanned(ids, false, 1906);
  CHECK(discovery.next(2056, request) && request.route == 9);
  discovery.feed(firmware(9), 9, 2057);
  CHECK(discovery.next(2207, request));
  discovery.feed(wfb_sample(), 9, 2208);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).route == 9);

  discovery.start(true, bike::IntegrationId::Wfb, 0, hint);
  CHECK(discovery.next(150, request));
  CHECK(!discovery.next(801, request));
  CHECK(discovery.next(802, request) && request.scan); // A disappeared cached CAN ID cannot stall discovery.
}

void wheel_e_identity_tests() {
  const auto &wheel_e = bike::integration(bike::IntegrationId::WheelE);
  const auto query = wheel_e.identity_request();
  CHECK(Bytes(query.begin(), query.end()) == Bytes({36, 101, 0, 2, 0}));
  const auto info = wheel_e_info();
  CHECK(wheel_e.verify_identity(info));
  CHECK(wheel_e.verify_identity(wheel_e_info("wheel-e")));
  CHECK(!wheel_e.verify_identity(wheel_e_info("Refloat")));
  CHECK(!wheel_e.verify_identity(wheel_e_info("Float")));
  CHECK(!wheel_e.verify_identity(wheel_e_info("Wheel-E-other")));
  for (size_t size = 0; size < info.size(); ++size)
    CHECK(!wheel_e.verify_identity(std::span(info).first(size)));
  auto extended = info;
  extended.push_back(0);
  CHECK(!wheel_e.verify_identity(extended));
  for (auto [offset, value] : {std::pair<size_t, uint8_t>{0, 35}, {1, 102}, {2, 1}, {3, 1}, {4, 1}, {12, 1}}) {
    auto invalid = info;
    invalid[offset] = value;
    CHECK(!wheel_e.verify_identity(invalid));
  }
  auto zero_ticks = info;
  std::fill(zero_ticks.begin() + 52, zero_ticks.begin() + 56, 0);
  CHECK(!wheel_e.verify_identity(zero_ticks));

  bike::ControllerDiscovery discovery;
  bike::DiscoveryRequest request;
  const auto hint = hint_for(7, 7);
  discovery.start(true, bike::IntegrationId::WheelE, 0, hint);
  CHECK(discovery.next(150, request));
  discovery.feed(firmware(7), 7, 151);
  CHECK(discovery.next(301, request) && request.bytes[2] == 0);
  discovery.feed(wheel_e_info("Refloat"), 7, 302);
  discovery.feed(wheel_e_sample(), 7, 303);
  CHECK(!discovery.complete()); // Another interface101 package cannot inherit wheel-e's cached identity.
  CHECK(!discovery.next(952, request));
  CHECK(discovery.next(953, request) && request.scan);

  // Even valid wheel-e identity must be followed by a valid modern telemetry probe.
  discovery.start(true, bike::IntegrationId::WheelE, 0, hint);
  CHECK(discovery.next(150, request));
  discovery.feed(firmware(7), 7, 151);
  CHECK(discovery.next(301, request));
  discovery.feed(info, 7, 302);
  CHECK(discovery.next(452, request) && request.bytes[2] == 33);
  CHECK(!discovery.next(1103, request));
  CHECK(discovery.next(1104, request) && request.scan);

  discovery.start(false, bike::IntegrationId::Auto, 0);
  CHECK(discovery.next(0, request) && request.scan);
  discovery.feed(Bytes{62}, -2, 1);
  CHECK(discovery.next(151, request));
  discovery.feed(firmware(7), -2, 152);
  CHECK(discovery.next(302, request) && request.bytes[1] == 102);
  CHECK(!discovery.next(953, request));
  CHECK(discovery.next(1103, request) && request.bytes[1] == 101 && request.bytes[2] == 0);
  discovery.feed(wheel_e_info("Refloat"), -2, 1104);
  discovery.feed(wheel_e_sample(), -2, 1105);
  CHECK(!discovery.next(1754, request));
  CHECK(discovery.next(1904, request) && request.bytes[0] == 47);
  discovery.feed(setup_sample(), -2, 1905);
  CHECK(discovery.complete() && discovery.controller(discovery.chosen()).integration == bike::IntegrationId::Vesc);
}

int main() {
  codec_tests();
  bridge_tests();
  discovery_tests();
  auto_discovery_tests();
  hint_codec_tests();
  cached_discovery_tests();
  wheel_e_identity_tests();
  std::cout << "CAN codec and controller discovery passed\n";
}
