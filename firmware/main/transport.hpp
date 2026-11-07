#pragma once
#include "bike/integration.hpp"
#include "bike/link_status.hpp"
#include <array>
#include <span>
#include <string_view>

namespace transport
{

enum class Kind : uint8_t {
  Ble = 0,
  Can = 1,
  Uart = 2
};
using Status = bike::LinkStatus;

struct Device {
  char name[32]{}, address[20]{};
  int rssi = 0;
};

struct Snapshot {
  Status status = Status::Disconnected;
  uint32_t session = 0;
  int rssi = 0;
  bool rssi_valid = false;
  uint64_t rssi_sample_ms = 0;
  bike::IntegrationId integration = bike::IntegrationId::Vesc;
  bike::IntegrationId selection = bike::IntegrationId::Auto;
  std::array<uint8_t, 7> peer_identity{};
  char connected_name[32]{};
  std::array<Device, 6> devices{};
  int count = 0;
  bool link_present = false;
  int controller_id = -1;
  char controller_info[96]{};
};

struct Packet {
  uint32_t session = 0;
  uint16_t size = 0;
  bool discontinuity = false;
  bool passive_proxy = false; // Verified controller-ID setup from the phone's own query.
  int controller = -2;        // -2: untagged serial reply; -1: direct controller.
  uint32_t proxy_epoch = 0;
  std::array<uint8_t, bike::max_payload> bytes{};
};

struct CanScan {
  std::array<uint8_t, 255> ids{};
  size_t count = 0;
  bool complete = false, overflow = false;
};

// Integrations exchange complete, unframed VESC application payloads. Each
// backend owns wire framing and rejects work from an earlier connection.
class Interface {
public:

  virtual ~Interface() = default;

  virtual void activate(bike::IntegrationId integration) = 0;

  virtual void deactivate() = 0;

  virtual Snapshot snapshot() = 0;

  virtual bool send(std::span<const uint8_t> payload) = 0;

  virtual bool receive(Packet &packet) = 0;

  virtual void pause(bool paused) = 0;

  virtual bool send_to(std::span<const uint8_t> payload, int controller) = 0;

  virtual void scan_can() {
  }

  virtual CanScan can_scan() {
    return {};
  }
};

bool available(Kind kind);

const char *name(Kind kind);

Kind selected();

void start();

bool select(Kind kind); // UI task; callers enforce stationary/access guards.

Snapshot snapshot();

bool send(std::span<const uint8_t> payload);

// Read-only telemetry; arbitrated with the phone instead of being suspended.
bool poll(std::span<const uint8_t> payload);

bool receive(Packet &packet);

void set_paused(bool paused);

void connect(std::string_view address, int integration = -1);

void forget();

bool rediscover();

// Transparent phone client: no display controller route is consulted.

void proxy_ownership(bool owned, uint32_t epoch);

bool send_proxy_fragment(std::span<const uint8_t> bytes, uint32_t epoch);

void reset_proxy_link(Kind source);

bool proxy_drained(Kind source, uint32_t epoch);

} // namespace transport
