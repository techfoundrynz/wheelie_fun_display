#pragma once

#include "bike/polling.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

// Opt-in diagnostic image only. All requests are existing read-only telemetry
// queries. Keep one request per reply family in flight to measure useful reply
// throughput rather than how quickly the transmit queue can be filled.
class BleBenchmark {
  static constexpr std::array<unsigned, 6> rates{5, 10, 15, 20, 30, 40};
  static constexpr uint64_t duration_ms = 12000, drain_ms = 1500;

  struct Pending {
    uint64_t sent = 0;
  };

  std::array<Pending, 2> pending{};
  std::array<unsigned, 101> latency{};
  unsigned stage = 0, sent = 0, replies = 0, failed = 0, timed_out = 0, deferred = 0;
  uint64_t begin = 0, next = 0, drain_until = 0, latency_sum = 0, bytes = 0;
  unsigned max_latency = 0, invalid = 0;
  std::array<unsigned, 2> received_by_family{};
  bool draining = false, done = false, phone_mode = false;
  uint8_t completed_modes = 0;

  void report(uint64_t now, const char *result) {
    unsigned accumulated = 0, percentile = 0;
    for (size_t i = 0; i < latency.size(); ++i) {
      accumulated += latency[i];
      if (accumulated * 100 >= replies * 95) {
        percentile = unsigned(i) * 10;
        break;
      }
    }
    const double seconds = (now - begin) / 1000.;
    ESP_LOGI("ble_benchmark",
             "%s phone_proxy=%d bundle_hz=%u seconds=%.3f accepted=%u replies=%u reply_hz=%.2f payload_Bps=%.1f "
             "queue_failed=%u timeout=%u deferred=%u rtt_avg_ms=%.1f rtt_p95_ms=%u rtt_max_ms=%u "
             "decoder_rejected=%u setup_replies=%u package_replies=%u internal_free=%u internal_largest=%u",
             result, phone_mode, rates[stage], seconds, sent, replies, replies / seconds, bytes / seconds, failed,
             timed_out, deferred, replies ? double(latency_sum) / replies : 0., percentile, max_latency, invalid,
             received_by_family[0], received_by_family[1], unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  }

public:

  void receive(std::span<const uint8_t> payload, uint64_t now, bool decoded) {
    if (!begin || draining || done || payload.empty())
      return;
    const int family = payload[0] == 47 ? 0 : payload[0] == 36 ? 1 : -1;
    if (family < 0 || !pending[family].sent)
      return;
    const auto elapsed = unsigned(now - pending[family].sent);
    pending[family].sent = 0;
    if (received_by_family[family]++ == 0) {
      ESP_LOGI("ble_benchmark", "reply command=%u bytes=%u decoder_accepted=%d", payload[0], unsigned(payload.size()),
               decoded);
      if (!decoded && payload[0] == 47)
        ESP_LOG_BUFFER_HEX_LEVEL("ble_benchmark", payload.data(), payload.size(), ESP_LOG_INFO);
    }
    invalid += !decoded;
    ++replies;
    bytes += payload.size();
    latency_sum += elapsed;
    max_latency = std::max(max_latency, elapsed);
    ++latency[std::min<size_t>((elapsed + 9) / 10, latency.size() - 1)];
  }

  template <typename Send> void tick(uint64_t now, bool ready, std::span<const uint8_t> custom, Send send, bool phone) {
    if (phone != phone_mode) {
      if (begin && !done && !draining)
        report(now, "ABORTED_MODE_CHANGE");
      phone_mode = phone;
      done = (completed_modes & (1u << unsigned(phone))) != 0;
      stage = 0;
      begin = next = drain_until = 0;
      pending = {};
      draining = false;
    }
    if (done) {
      if (ready && now >= next) {
        constexpr uint8_t setup[] = {47};
        send(std::span<const uint8_t>(setup, sizeof(setup)));
        if (!custom.empty())
          send(custom);
        next = now + bike::telemetry_poll_interval_ms;
      }
      return;
    }
    if (!ready) {
      if (begin) {
        report(now, "ABORTED_LINK");
        done = true;
      }
      return;
    }
    if (!drain_until) {
      drain_until = now + 3000;
      ESP_LOGI("ble_benchmark", "Starting read-only BLE benchmark in 3 s");
      esp_log_level_set("NimBLE", ESP_LOG_WARN);
    }
    if (now < drain_until)
      return;
    if (draining) {
      if (++stage == rates.size()) {
        ESP_LOGI("ble_benchmark", "COMPLETE: restore a normal image before everyday use");
        done = true;
        completed_modes |= 1u << unsigned(phone_mode);
        return;
      }
      draining = false;
      begin = 0;
    }
    if (!begin) {
      begin = next = now;
      sent = replies = failed = timed_out = deferred = max_latency = 0;
      latency_sum = bytes = 0;
      pending = {};
      received_by_family = {};
      invalid = 0;
      latency = {};
      ESP_LOGI("ble_benchmark", "STAGE phone_proxy=%d bundle_hz=%u queries_per_bundle=%u", phone_mode, rates[stage],
               custom.empty() ? 1 : 2);
    }
    for (auto &request : pending) {
      if (request.sent && now - request.sent >= 1000) {
        // A lost reply makes subsequent same-command latency ambiguous. Abort
        // this measurement rather than crediting a late reply to a new query.
        ++timed_out;
        report(now, "ABORTED_TIMEOUT");
        done = true;
        return;
      }
    }
    if (now - begin >= duration_ms) {
      report(now, "RESULT");
      draining = true;
      drain_until = now + drain_ms;
      return;
    }
    if (now < next)
      return;
    next = now + (1000 + rates[stage] - 1) / rates[stage];
    constexpr uint8_t setup[] = {47};
    const std::array<std::span<const uint8_t>, 2> requests{std::span<const uint8_t>(setup, sizeof(setup)), custom};
    for (size_t i = 0; i < requests.size(); ++i) {
      if (requests[i].empty())
        continue;
      if (pending[i].sent) {
        ++deferred;
        continue;
      }
      if (send(requests[i])) {
        pending[i].sent = now;
        ++sent;
      }
      else {
        ++failed;
      }
    }
  }
};
