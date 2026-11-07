#pragma once
#include "bike/ble_lifecycle.hpp"
#include "bike/integration.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace ble
{

using Status = bike::LinkStatus;

struct Device {
  char name[32]{};
  char address[20]{};
  int rssi = 0;
};

struct Snapshot {
  Status status = Status::Disconnected;
  std::array<Device, 6> devices{};
  int count = 0;
  int rssi = 0;
  bool rssi_valid = false;
  uint64_t rssi_sample_ms = 0; // Last successful connected-link HCI measurement.
  uint32_t session = 0;
  char connected_name[32]{};
  bike::IntegrationId integration = bike::IntegrationId::Auto;
  std::array<uint8_t, 7> peer_identity{};
};

struct Received {
  uint32_t session;
  uint16_t size;
  std::array<uint8_t, 512> bytes;
  bool discontinuity = false; // Passive UI decoding fell behind the proxy stream.
};

void start();

Snapshot snapshot();

void connect(std::string_view address, int integration = -1);

void forget();

void retry_pairing();

void cancel_pairing();

bool send(std::span<const uint8_t> packet);

bool receive(Received &packet);

void set_paused(bool paused);

// The peripheral proxy reserves the UART conversation until its phone leaves.

void set_proxy_owner(bool owned, uint32_t epoch);

bool proxy_ready(uint32_t epoch);

bool proxy_drained(uint32_t epoch);

bool send_proxy_fragment(std::span<const uint8_t> bytes, uint32_t epoch);

void reset_link();

} // namespace ble
