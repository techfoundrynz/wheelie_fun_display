#include "bike/protocol.hpp"
#include "bike/proxy_poll.hpp"
#include "bike/telemetry.hpp"
#include "bike/telemetry_poll.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(condition)                                                                                               \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::cerr << __LINE__ << ": " #condition "\n";                                                                   \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)
using Bytes = std::vector<uint8_t>;

Bytes wire(std::span<const uint8_t> payload) {
  Bytes result;
  if (payload.size() < 256)
    result = {2, uint8_t(payload.size())};
  else if (payload.size() < 65536)
    result = {3, uint8_t(payload.size() >> 8), uint8_t(payload.size())};
  else
    result = {4, uint8_t(payload.size() >> 16), uint8_t(payload.size() >> 8), uint8_t(payload.size())};
  result.insert(result.end(), payload.begin(), payload.end());
  const auto crc = bike::crc16(payload);
  result.insert(result.end(), {uint8_t(crc >> 8), uint8_t(crc), 3});
  return result;
}

Bytes wire(std::initializer_list<uint8_t> payload) {
  return wire(std::span(payload.begin(), payload.size()));
}

void append(Bytes &destination, const Bytes &source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

bike::ProxyReplyKey key(std::initializer_list<uint8_t> bytes) {
  bike::ProxyReplyKey result;
  result.size = bytes.size();
  std::copy(bytes.begin(), bytes.end(), result.bytes.begin());
  return result;
}

void independent_rx_frame_does_not_block_tx_poll() {
  const auto package = key({36, 102, 10, 1});
  Bytes config_payload(600, 0x5a);
  config_payload[0] = 14;
  const auto config = wire(config_payload);
  const auto own_reply = wire({36, 102, 10, 1, 7, 8, 9});
  const auto print = wire({21, 'o', 'k'});
  for (size_t first : {1u, 2u, 3u, 4u, 20u, 512u}) {
    bike::ProxyPollArbiter arbiter;
    CHECK(arbiter.register_key(package));
    Bytes forwarded, owned;
    auto phone = [&](uint8_t byte) { forwarded.push_back(byte); };
    auto display = [&](uint8_t byte) { owned.push_back(byte); };
    CHECK(arbiter.upstream(std::span(config).first(first), phone, display));
    CHECK(arbiter.may_poll(package) == (first >= 4));
    if (first < 4) {
      CHECK(arbiter.upstream(std::span(config).subspan(first, 4 - first), phone, display));
      first = 4;
    }
    CHECK(arbiter.begin(package, 100)); // RX is still in the600-byte phone configuration frame.
    Bytes remaining(config.begin() + first, config.end());
    append(remaining, own_reply);
    append(remaining, print);
    for (size_t offset = 0; offset < remaining.size(); offset += 20)
      CHECK(arbiter.upstream(std::span(remaining).subspan(offset, std::min<size_t>(20, remaining.size() - offset)),
                             phone, display));
    Bytes expected = config;
    append(expected, print);
    CHECK(forwarded == expected && owned == own_reply && !arbiter.pending());
  }

  bike::ProxyPollArbiter arbiter;
  CHECK(arbiter.register_key(package));
  const auto conflicting = wire({36, 102, 10, 1, 7, 8});
  for (size_t i = 0; i < 6; ++i) {
    CHECK(arbiter.upstream(std::span(conflicting).subspan(i, 1), [](uint8_t) {}));
    CHECK(!arbiter.may_poll(package)); // Unknown header/partial matching key/full conflicting key.
  }
  CHECK(arbiter.upstream(std::span(conflicting).subspan(6), [](uint8_t) {}));
  CHECK(arbiter.may_poll(package));
  arbiter.reset();
  const auto different_app = wire({36, 101, 33, 1, 0});
  CHECK(arbiter.upstream(std::span(different_app).first(4), [](uint8_t) {}));
  CHECK(arbiter.may_poll(package)); // First differing prefix byte is proof, even before four bytes.
  const auto phone_request = wire({0});
  CHECK(arbiter.phone(std::span(phone_request).first(3)));
  CHECK(!arbiter.may_poll(package)); // TX's independent partial frame still cannot be split.
  CHECK(arbiter.phone(std::span(phone_request).subspan(3)));
  CHECK(arbiter.may_poll(package));
}

int main() {
  independent_rx_frame_does_not_block_tx_poll();
  const auto setup = key({47}), bms = key({96}), wfb = key({36, 102, 10, 1});
  const std::array<uint8_t, 4> wfb_request{36, 102, 10, 1};
  CHECK(!bike::proxy_setup_poll_needed(1199, 1000));
  CHECK(bike::proxy_setup_poll_needed(1200, 1000));
  CHECK(bike::proxy_setup_poll_needed(1, 0));
  bike::ProxyPollQueueCredits queue_credits;
  CHECK(queue_credits.claim(setup, 1));
  CHECK(!queue_credits.claim(setup, 1));
  CHECK(queue_credits.claim(wfb, 1));
  queue_credits.release(wfb, 1);
  CHECK(queue_credits.claim(wfb, 1));
  CHECK(queue_credits.claim(setup, 2));
  queue_credits.release(setup, 1);
  CHECK(!queue_credits.claim(setup, 2)); // An old callback cannot reopen a newer queued key.
  CHECK(bike::relay_advertising_hold(100, 1, false, 0));
  CHECK(bike::relay_advertising_hold(1299, 1, true, 1000));
  CHECK(!bike::relay_advertising_hold(1300, 1, true, 1000));
  CHECK(!bike::relay_advertising_hold(10001, 1, false, 0)); // Generic relay remains available after failed discovery.
  CHECK(bike::relay_advertising_hold(15000, 14500, false, 0)); // Reconnect gives discovery a fresh opportunity.
  bike::TelemetryPollCredits credits;
  CHECK(credits.available(setup, 1));
  credits.sent(setup, 1);
  CHECK(!credits.available(setup, 1000) && credits.available(setup, 1001));
  CHECK(credits.available(wfb, 2));
  credits.sent(wfb, 2);
  credits.reply(std::array<uint8_t, 1>{47});
  CHECK(credits.available(setup, 3) && !credits.available(wfb, 3));
  credits.reply(std::array<uint8_t, 4>{36, 101, 33, 1});
  CHECK(!credits.available(wfb, 4));
  credits.reply(wfb_request);
  CHECK(credits.available(wfb, 5));
  credits.sent(bms, 5);
  CHECK(!credits.available(bms, 6));
  credits.reset();
  CHECK(credits.available(bms, 6));
  CHECK(bike::telemetry_poll_key(std::array<uint8_t, 1>{47}) == setup);
  CHECK(bike::telemetry_poll_key(std::array<uint8_t, 1>{96}) == bms);
  CHECK(bike::telemetry_poll_key(wfb_request) == wfb);
  CHECK(bike::telemetry_poll_key(std::array<uint8_t, 12>{36, 101, 33, 1, 0, 10, 0, 2, 0, 0, 0, 0}));
  CHECK(!bike::telemetry_poll_key(std::array<uint8_t, 4>{36, 102, 12, 1}));
  CHECK(!bike::telemetry_poll_key(std::array<uint8_t, 4>{36, 102, 11, 2}));
  CHECK(!bike::telemetry_poll_key(std::array<uint8_t, 4>{36, 101, 33, 1}));
  CHECK(!bike::telemetry_poll_key(std::array<uint8_t, 2>{47, 0}));
  for (unsigned command = 0; command < 256; ++command)
    CHECK(bool(bike::telemetry_poll_key(std::array<uint8_t, 1>{uint8_t(command)})) == (command == 47 || command == 96));

  bike::ProxyPollArbiter arbiter;
  CHECK(arbiter.register_key(setup) && arbiter.register_key(wfb));
  CHECK(!arbiter.begin(setup, 0, true)); // Pending phone fragments win before commit.
  CHECK(!arbiter.yield_phone_for_poll(true));
  for (int i = 0; i < 3; ++i)
    CHECK(arbiter.phone(wire({47})));
  CHECK(arbiter.yield_phone_for_poll(true));
  CHECK(arbiter.reserve(setup, 1));
  CHECK(!arbiter.reserve(wfb, 2));
  CHECK(!arbiter.may_poll(wfb));   // An eligible key cannot steal the reserved setup window.
  CHECK(!arbiter.may_poll(setup)); // Yield drains replies; it never mixes same-command ownership.
  CHECK(arbiter.phone_replies(setup) == 3);
  for (int i = 0; i < 3; ++i)
    CHECK(arbiter.upstream(wire({47, 9}), [](uint8_t) {}));
  CHECK(arbiter.phone_replies(setup) == 0 && arbiter.may_poll(setup));
  CHECK(arbiter.begin(setup, 0));
  CHECK(!arbiter.yield_phone_for_poll(true));
  CHECK(arbiter.upstream(wire({47, 8}), [](uint8_t) {}));
  CHECK(arbiter.may_send_phone());
  arbiter.reset();
  const auto request = wire({47});
  CHECK(arbiter.phone(std::span(request).first(1)));
  CHECK(!arbiter.may_poll(setup) && !arbiter.may_poll(wfb)); // Never splice into an unfinished frame.
  CHECK(arbiter.phone(std::span(request).subspan(1)));
  CHECK(!arbiter.may_poll(setup)); // The phone owns the next matching reply.
  CHECK(arbiter.may_poll(wfb));    // A busy telemetry key does not starve another kind.
  Bytes forwarded;
  const auto response = wire({47, 1, 2, 3});
  CHECK(arbiter.upstream(response, [&](uint8_t value) { forwarded.push_back(value); }));
  CHECK(forwarded == response && arbiter.may_poll(setup));

  // A phone's CAN-forwarded setup reply belongs to that phone controller,
  // never to the display. The owned channel only receives our later poll.
  arbiter.reset();
  CHECK(arbiter.phone(wire({34, 8, 47})));
  const auto other_controller = wire({47, 8, 8, 8});
  Bytes owned;
  forwarded.clear();
  CHECK(arbiter.upstream(
      other_controller, [&](uint8_t value) { forwarded.push_back(value); },
      [&](uint8_t value) { owned.push_back(value); }));
  CHECK(forwarded == other_controller && owned.empty());
  CHECK(arbiter.begin(setup, 100));
  CHECK(arbiter.may_forward_phone(std::array<uint8_t, 1>{0}));
  CHECK(arbiter.may_forward_phone(std::array<uint8_t, 4>{36, 101, 33, 1}));
  CHECK(!arbiter.may_forward_phone(std::array<uint8_t, 1>{47}));
  CHECK(!arbiter.may_forward_phone(std::array<uint8_t, 3>{34, 8, 47}));
  CHECK(!arbiter.may_forward_phone(std::array<uint8_t, 1>{1}));
  const auto display_controller = wire({47, 1, 1, 1});
  CHECK(arbiter.upstream(
      display_controller, [&](uint8_t value) { forwarded.push_back(value); },
      [&](uint8_t value) { owned.push_back(value); }));
  CHECK(forwarded == other_controller && owned == display_controller);

  // Captured 70-byte setup reply, including vendor-specific invalid battery
  // percentage. Framing/ownership must deliver it independently of telemetry
  // field validation, including the real link's default 20-byte ATT chunks.
  const Bytes captured_setup{0x2f, 0x01, 0x46, 0xff, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x31, 0x10, 0x10, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x12, 0xc6, 0x84, 0xc0, 0x00,
                             0x57, 0x01, 0x00, 0x03, 0x8d, 0x83, 0x00, 0x00, 0xb4, 0xdf, 0x00, 0x4d, 0xc2, 0x8a};
  Bytes decoded_owned;
  bike::Decoder decoder(
      [](void *context, std::span<const uint8_t> payload) {
        auto &result = *static_cast<Bytes *>(context);
        result.assign(payload.begin(), payload.end());
      },
      &decoded_owned);
  arbiter.reset();
  CHECK(arbiter.begin(setup, 100));
  const auto captured_wire = wire(captured_setup);
  for (size_t offset = 0; offset < captured_wire.size(); offset += 20) {
    owned.clear();
    CHECK(arbiter.upstream(
        std::span(captured_wire).subspan(offset, std::min<size_t>(20, captured_wire.size() - offset)),
        [](uint8_t) { CHECK(false); }, [&](uint8_t value) { owned.push_back(value); }));
    decoder.feed(owned, 110 + offset);
  }
  CHECK(decoded_owned == captured_setup && !arbiter.pending());
  CHECK(bike::setup_controller_identity(captured_setup) == 87);
  CHECK(bike::proxy_setup_matches_controller(captured_setup, 87, false, -2, -1));
  CHECK(!bike::proxy_setup_matches_controller(captured_setup, 8, false, -2, -1));
  CHECK(!bike::proxy_setup_matches_controller(captured_setup, -1, false, -2, -1));
  CHECK(!bike::proxy_setup_matches_controller(std::span(captured_setup).first(56), 87, false, -2, -1));
  CHECK(bike::proxy_setup_matches_controller(captured_setup, 87, true, 87, 87));
  CHECK(!bike::proxy_setup_matches_controller(captured_setup, 87, true, 8, 87));

  // A late phone reply from a different CAN controller after clean handoff
  // must neither update the selected vehicle nor release its outstanding poll.
  auto late_setup = captured_setup;
  late_setup[56] = 8;
  bike::TelemetryPollCredits handoff_credits;
  handoff_credits.sent(setup, 100);
  if (bike::setup_reply_matches_controller(late_setup, 87))
    handoff_credits.reply(late_setup);
  CHECK(!handoff_credits.available(setup, 101));
  CHECK(bike::setup_reply_matches_controller(captured_setup, 87));
  handoff_credits.reply(captured_setup);
  CHECK(handoff_credits.available(setup, 102));
  CHECK(bike::setup_reply_matches_controller(std::span(captured_setup).first(56), 87));
  CHECK(bike::setup_reply_matches_controller(captured_setup, -1));

  // If the phone stops reporting setup values, a valid 600 ms arbitration/radio
  // delay must still leave the display's independent freshness check satisfied.
  bike::Telemetry fallback_sample;
  fallback_sample.setup_valid = true;
  fallback_sample.setup_ms = 1000;
  uint64_t fallback_reply_at = 0;
  for (uint64_t time = 1000; time < 5000; time += 10) {
    if (fallback_reply_at && time >= fallback_reply_at) {
      fallback_sample.setup_ms = time;
      fallback_reply_at = 0;
    }
    if (!fallback_reply_at && bike::proxy_setup_poll_needed(time, 1000))
      fallback_reply_at = time + 600;
    CHECK(fallback_sample.fresh(time));
  }

  // A dropped phone query leaves imperfect reply accounting. Never erase
  // that debt and claim a later phone reply as ours; expire only the unsent
  // reservation. Verified passive setup plus our separate custom key stays
  // live, with at most one queued request per key and no stream reset.
  arbiter.reset();
  for (int i = 0; i < 3; ++i)
    CHECK(arbiter.phone(wire({47})));
  for (int i = 0; i < 2; ++i)
    CHECK(arbiter.upstream(captured_wire, [](uint8_t) {}));
  CHECK(arbiter.phone_replies(setup) == 1);
  CHECK(arbiter.reserve(setup, 100));
  CHECK(!arbiter.may_send_phone() && !arbiter.may_poll(wfb));
  CHECK(!arbiter.reservation_expired(299) && arbiter.reservation_expired(300));
  CHECK(!arbiter.reservation_expired(110, 7) && arbiter.reservation_expired(110, 8));
  arbiter.release_reservation();
  CHECK(arbiter.phone_replies(setup) == 1 && arbiter.may_send_phone() && !arbiter.broken());
  bike::ProxyPollQueueCredits shared_queue;
  CHECK(shared_queue.claim(setup, 3));
  CHECK(!shared_queue.claim(setup, 3));
  shared_queue.release(setup, 3); // The unsent, expired setup reservation is skipped.
  unsigned passive_updates = 0, custom_updates = 0;
  uint64_t passive_ms = 0;
  for (unsigned cycle = 0; cycle < 200; ++cycle) {
    const uint64_t now = 1000 + cycle * 100;
    CHECK(arbiter.phone(wire({47})));
    forwarded.clear();
    CHECK(arbiter.upstream(captured_wire, [&](uint8_t value) { forwarded.push_back(value); }));
    CHECK(forwarded == captured_wire);
    CHECK(bike::proxy_setup_matches_controller(captured_setup, 87, false, -2, -1));
    passive_ms = now;
    ++passive_updates;
    CHECK(!bike::proxy_setup_poll_needed(now, passive_ms));
    CHECK(arbiter.phone_replies(setup) == 1); // Lost-query accounting is not silently fabricated away.
    CHECK(shared_queue.claim(wfb, 3));
    CHECK(!shared_queue.claim(wfb, 3));
    CHECK(arbiter.begin(wfb, now));
    owned.clear();
    CHECK(arbiter.upstream(
        wire({36, 102, 10, 1, 7}), [](uint8_t) { CHECK(false); }, [&](uint8_t value) { owned.push_back(value); }));
    CHECK(!owned.empty());
    shared_queue.release(wfb, 3);
    ++custom_updates;
    CHECK(arbiter.may_send_phone() && !arbiter.broken());
  }
  CHECK(passive_updates == 200 && custom_updates == 200);

  // At every fragmentation point, preserve unrelated phone packets byte for
  // byte while withholding only the display-owned response, including its CRC.
  Bytes incoming = wire({0, 6, 2});
  const auto before = incoming;
  append(incoming, response);
  const auto after = wire({21, 'o', 'k'});
  append(incoming, after);
  Bytes expected = before;
  append(expected, after);
  for (size_t split = 0; split <= incoming.size(); ++split) {
    arbiter.reset();
    CHECK(arbiter.begin(setup, 100));
    CHECK(!arbiter.may_send_phone());
    forwarded.clear();
    auto emit = [&](uint8_t value) { forwarded.push_back(value); };
    CHECK(arbiter.upstream(std::span(incoming).first(split), emit));
    CHECK(arbiter.upstream(std::span(incoming).subspan(split), emit));
    CHECK(forwarded == expected && !arbiter.pending() && arbiter.may_send_phone());
  }

  arbiter.reset();
  CHECK(arbiter.begin(wfb, 0));
  forwarded.clear();
  const auto other_package = wire({36, 101, 33, 1, 9, 8, 7});
  CHECK(arbiter.upstream(other_package, [&](uint8_t value) { forwarded.push_back(value); }));
  CHECK(forwarded == other_package && arbiter.pending());
  const auto own_package = wire({36, 102, 10, 1, 5, 6, 7});
  for (auto value : own_package)
    CHECK(arbiter.upstream(std::span(&value, 1), [&](uint8_t output) { forwarded.push_back(output); }));
  CHECK(forwarded == other_package && !arbiter.pending());

  arbiter.reset();
  CHECK(arbiter.phone(wire({34, 8, 47})));
  CHECK(!arbiter.may_poll(setup)); // Forwarded phone requests keep their own replies too.
  forwarded.clear();
  CHECK(arbiter.upstream(response, [&](uint8_t value) { forwarded.push_back(value); }));
  CHECK(forwarded == response && arbiter.may_poll(setup));

  for (size_t length : {255u, 256u, 65535u, 65536u, 70000u}) {
    arbiter.reset();
    Bytes payload(length, 0x5a);
    payload[0] = 37; // An opaque phone command/reply; not a display poll.
    const auto packet = wire(payload);
    for (size_t offset = 0; offset < packet.size(); offset += 37) {
      CHECK(arbiter.phone(std::span(packet).subspan(offset, std::min<size_t>(37, packet.size() - offset))));
      if (offset + 37 < packet.size())
        CHECK(!arbiter.may_poll(setup));
    }
    CHECK(arbiter.may_poll(setup));
    forwarded.clear();
    for (size_t offset = 0; offset < packet.size(); offset += 113)
      CHECK(arbiter.upstream(std::span(packet).subspan(offset, std::min<size_t>(113, packet.size() - offset)),
                             [&](uint8_t value) { forwarded.push_back(value); }));
    CHECK(forwarded == packet && !arbiter.broken());
  }

  arbiter.reset();
  CHECK(arbiter.begin(setup, 500));
  CHECK(!arbiter.timed_out(3499) && arbiter.timed_out(3500));
  CHECK(!arbiter.begin(wfb, 3500)); // Late response isolation survives timeout until link reset.
  auto corrupted = response;
  corrupted[corrupted.size() - 2] ^= 1;
  CHECK(!arbiter.upstream(corrupted, [](uint8_t) {}));
  CHECK(arbiter.broken());
  arbiter.reset();
  CHECK(arbiter.may_poll(setup));
  for (uint8_t command : {1,  2,  3,  29, 59, 60, 61,  66,  67,  68,  69,  70,  71,  72, 73,
                          74, 80, 81, 82, 83, 85, 102, 103, 104, 105, 106, 107, 108, 109}) {
    arbiter.reset();
    CHECK(arbiter.phone(wire({command, 0, 0, 0})));
    CHECK(!arbiter.may_poll(setup)); // Do not issue telemetry to a bootloader/flash transaction.
    arbiter.reset();
    CHECK(arbiter.phone(wire({34, 8, command, 0})));
    CHECK(!arbiter.may_poll(setup));
  }
  std::cout << "Proxy read-only polling, phone priority, reply isolation and streaming boundaries passed\n";
}
