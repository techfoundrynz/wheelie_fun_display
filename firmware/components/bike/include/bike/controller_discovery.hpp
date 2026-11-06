#pragma once
#include "controller_hint.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace bike
{

struct Controller {
  int route = -1; // -1 is the directly connected UART/BLE controller.
  int can_id = -1;
  IntegrationId integration = IntegrationId::Vesc; // Verified package, independent of selection policy.
  char hardware[32]{};
  std::array<uint8_t, 12> uuid{};
  uint8_t major = 0, minor = 0, hardware_type = 255, custom_configs = 0;
  bool firmware = false, compatible = false, duplicate = false;
};

struct DiscoveryRequest {
  int route = -1;
  bool scan = false;
  std::array<uint8_t, 16> bytes{};
  size_t size = 0;
};

class ControllerDiscovery {
public:

  static constexpr size_t capacity = 256;

  void start(bool native_can, IntegrationId integration, uint64_t now,
             std::optional<ControllerHint> hint = std::nullopt) {
    for (auto &controller : controllers_)
      controller = {};
    count_ = index_ = 0;
    chosen_ = -1;
    sent_ = overflow_ = false;
    native_ = native_can;
    integration_ = integration;
    probe_ = first_probe();
    hint_ = hint;
    trying_hint_ = hint && hint->route >= (native_can ? 0 : -1) && hint->route <= 254;
    if (trying_hint_) {
      add(hint->route);
      quiet(Stage::Firmware, now);
    }
    else
      begin_scan(now);
  }

  void add(int id) {
    if (id < -1 || id > 254)
      return;
    for (size_t i = 0; i < count_; ++i)
      if (controllers_[i].route == id)
        return;
    if (count_ == capacity) {
      overflow_ = true;
      return;
    }
    auto &controller = controllers_[count_++];
    controller.route = id;
    controller.can_id = id;
  }

  void scanned(std::span<const uint8_t> ids, bool overflow, uint64_t now) {
    if (stage_ != Stage::Scan)
      return;
    for (auto id : ids)
      add(id);
    overflow_ |= overflow;
    begin_queries(now);
  }

  bool next(uint64_t now, DiscoveryRequest &request) {
    if (stage_ == Stage::Idle || stage_ == Stage::Complete)
      return false;
    if (stage_ == Stage::Scan) {
      if (!sent_) {
        sent_ = true;
        request = {};
        request.scan = true;
        request.bytes[0] = 62;
        request.size = 1;
        return true;
      }
      if (now < deadline_)
        return false;
      begin_queries(now);
    }
    if (stage_ == Stage::Quiet) {
      if (now < deadline_)
        return false;
      stage_ = next_stage_;
      sent_ = false;
    }
    if ((stage_ == Stage::Firmware || stage_ == Stage::Identity || stage_ == Stage::Package) && sent_) {
      if (now < deadline_)
        return false;
      if (stage_ == Stage::Identity || stage_ == Stage::Package)
        advance_package(now);
      else
        advance(now);
      return false;
    }
    if (stage_ != Stage::Firmware && stage_ != Stage::Identity && stage_ != Stage::Package)
      return false;
    request = {};
    request.route = controllers_[index_].route;
    if (stage_ == Stage::Firmware) {
      request.bytes[0] = 0;
      request.size = 1;
    }
    else {
      const auto payload =
          stage_ == Stage::Identity ? integration(probe_).identity_request() : integration(probe_).poll();
      if (payload.empty()) {
        request.bytes[0] = 47;
        request.size = 1;
      }
      else {
        std::copy(payload.begin(), payload.end(), request.bytes.begin());
        request.size = payload.size();
      }
    }
    sent_ = true;
    deadline_ = now + 650;
    return true;
  }

  void feed(std::span<const uint8_t> payload, int source, uint64_t now) {
    if (payload.empty())
      return;
    if (stage_ == Stage::Scan && sent_ && !native_ && payload[0] == 62) {
      scanned(payload.subspan(1), false, now);
      return;
    }
    if ((stage_ != Stage::Firmware && stage_ != Stage::Identity && stage_ != Stage::Package) || !sent_ ||
        index_ >= count_)
      return;
    auto &controller = controllers_[index_];
    // BLE forwarded responses have no source tag (-2); native CAN does.
    if (source != -2 && source != controller.route)
      return;
    if (stage_ == Stage::Firmware && payload[0] == 0) {
      if (!firmware(payload, controller))
        return;
      if (trying_hint_ && controller.uuid != hint_->uuid) {
        begin_scan(now);
        return;
      }
      if (controller.hardware_type != 0 && controller.hardware_type != 255) {
        advance(now);
        return;
      }
      probe_ = first_probe();
      begin_package(now);
    }
    else if (stage_ == Stage::Identity) {
      if (integration(probe_).verify_identity(payload))
        quiet(Stage::Package, now);
    }
    else if (stage_ == Stage::Package) {
      Telemetry sample;
      if (!integration(probe_).decode(payload, now, sample))
        return;
      const bool match = probe_ == IntegrationId::Vesc ? sample.setup_valid : sample.bike_valid;
      if (!match)
        return;
      controller.compatible = true;
      controller.integration = probe_;
      if (probe_ != IntegrationId::Vesc ||
          (integration_ == IntegrationId::Vesc && (trying_hint_ || controller.custom_configs))) {
        finish();
        return; // First verified package controller is the main one.
      }
      advance(now);
    }
  }

  bool complete() const {
    return stage_ == Stage::Complete;
  }

  bool running() const {
    return stage_ != Stage::Idle && stage_ != Stage::Complete;
  }

  bool awaiting_scan() const {
    return stage_ == Stage::Scan && sent_;
  }

  IntegrationId selection() const {
    return integration_;
  }

  size_t count() const {
    return count_;
  }

  bool overflow() const {
    return overflow_;
  }

  const Controller &controller(size_t index) const {
    return controllers_[index];
  }

  int chosen() const {
    return chosen_;
  }

  bool choose(size_t index) {
    if (!complete() || index >= count_ || !controllers_[index].compatible || controllers_[index].duplicate)
      return false;
    chosen_ = index;
    return true;
  }

private:

  enum class Stage {
    Idle,
    Scan,
    Firmware,
    Identity,
    Package,
    Quiet,
    Complete
  };
  Stage stage_ = Stage::Idle, next_stage_ = Stage::Idle;
  bool native_ = false, sent_ = false, overflow_ = false, trying_hint_ = false;
  std::optional<ControllerHint> hint_;
  IntegrationId integration_ = IntegrationId::Auto, probe_ = IntegrationId::Wfb;
  std::array<Controller, capacity> controllers_{};
  size_t count_ = 0, index_ = 0;
  int chosen_ = -1;
  uint64_t deadline_ = 0;

  void quiet(Stage stage, uint64_t now) {
    next_stage_ = stage;
    stage_ = Stage::Quiet;
    deadline_ = now + 150;
    sent_ = false;
  }

  void begin_scan(uint64_t now) {
    trying_hint_ = false;
    hint_.reset();
    for (auto &controller : controllers_)
      controller = {};
    count_ = index_ = 0;
    chosen_ = -1;
    sent_ = overflow_ = false;
    stage_ = Stage::Scan;
    deadline_ = now + (native_ ? 3000 : 7000);
    if (!native_)
      add(-1);
  }

  void begin_queries(uint64_t now) {
    if (!count_)
      finish();
    else
      quiet(Stage::Firmware, now);
  }

  IntegrationId first_probe() const {
    return integration_ == IntegrationId::Auto ? IntegrationId::Wfb : integration_;
  }

  void begin_package(uint64_t now) {
    quiet(integration(probe_).identity_request().empty() ? Stage::Package : Stage::Identity, now);
  }

  void advance_package(uint64_t now) {
    if (integration_ == IntegrationId::Auto && probe_ != IntegrationId::Vesc) {
      probe_ = probe_ == IntegrationId::Wfb ? IntegrationId::WheelE : IntegrationId::Vesc;
      begin_package(now);
    }
    else
      advance(now);
  }

  void advance(uint64_t now) {
    if (trying_hint_) {
      begin_scan(now);
      return;
    }
    if (++index_ >= count_)
      finish();
    else
      quiet(Stage::Firmware, now);
  }

  void finish() {
    stage_ = Stage::Complete;
    chosen_ = -1;
    for (size_t i = 0; i < count_; ++i) {
      auto &a = controllers_[i];
      if (!a.firmware)
        continue;
      for (size_t j = 0; j < i; ++j) {
        auto &b = controllers_[j];
        if (!b.firmware || a.uuid != b.uuid)
          continue;
        if (b.route == -1 && a.route >= 0) {
          b.can_id = a.route;
          // Retain the route that actually proved the package. A generic or
          // silent direct path must not hide a later verified CAN package.
          const bool stronger =
              a.compatible &&
              (!b.compatible || (a.integration != IntegrationId::Vesc && b.integration == IntegrationId::Vesc));
          if (stronger)
            b.duplicate = true;
          else
            a.duplicate = true;
        }
        else {
          a.duplicate = b.duplicate = true;
        } // Conflicting untagged replies: never guess.
      }
    }
    for (size_t i = 0; i < count_; ++i)
      if (controllers_[i].compatible && !controllers_[i].duplicate &&
          (controllers_[i].integration != IntegrationId::Vesc ||
           (integration_ == IntegrationId::Vesc && controllers_[i].custom_configs))) {
        chosen_ = i;
        return;
      }
    // Auto exhausts both known package probes before accepting read-only VESC telemetry.
    if (integration_ == IntegrationId::Vesc || integration_ == IntegrationId::Auto)
      for (size_t i = 0; i < count_; ++i)
        if (controllers_[i].compatible && !controllers_[i].duplicate) {
          chosen_ = i;
          return;
        }
  }

  static bool firmware(std::span<const uint8_t> payload, Controller &out) {
    if (payload.size() < 3 + 1 + 12 || payload[0] != 0)
      return false;
    size_t end = 3;
    while (end < payload.size() && payload[end] && end < 100)
      ++end;
    if (end == payload.size() || end == 100 || end + 13 > payload.size())
      return false;
    out.major = payload[1];
    out.minor = payload[2];
    for (size_t i = 3; i < end && i - 3 < sizeof(out.hardware) - 1; ++i)
      out.hardware[i - 3] = payload[i] >= 32 && payload[i] < 127 ? char(payload[i]) : '?';
    std::copy_n(payload.begin() + end + 1, 12, out.uuid.begin());
    const auto extra = end + 13;
    out.hardware_type = payload.size() > extra + 2 ? payload[extra + 2] : 255;
    out.custom_configs = payload.size() > extra + 3 ? payload[extra + 3] : 0;
    out.firmware = true;
    return true;
  }
};

} // namespace bike
