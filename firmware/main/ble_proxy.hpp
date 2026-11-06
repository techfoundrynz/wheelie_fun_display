#pragma once
#include "transport.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ble_proxy
{

struct Snapshot {
  bool enabled = false, connected = false, subscribed = false, advertising = false, upstream_ready = false;
  uint32_t failures = 0;
};

// init() runs after nimble_port_init(), before the NimBLE host starts.

void init();

void host_sync(uint8_t own_address_type);

void host_reset();

void select_upstream(transport::Kind source);

void upstream_state(transport::Kind source, bool ready, uint32_t session);

void source_paused(transport::Kind source, bool paused);

void upstream(transport::Kind source, std::span<const uint8_t> bytes, uint32_t session);

void upstream_failed(transport::Kind source);

void set_enabled(bool enabled);

bool set_name(std::string_view name);

std::string name();

void set_paused(bool paused);

// Atomic against a phone connection being admitted by the GAP callback.

bool begin_maintenance();

Snapshot snapshot();

bool phone_owns_link();

} // namespace ble_proxy
