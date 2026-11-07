#pragma once
#include "bike/proxy_poll.hpp"
#include "transport.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ble_proxy
{

bool set_update_mode(bool enabled);

bool update_mode();

bool update_owner_valid(uint32_t epoch, uint32_t session);

bool update_send(std::span<const uint8_t> bytes, uint32_t epoch);

void update_reply(uint32_t epoch, uint32_t session, std::span<const uint8_t> payload);

struct Snapshot {
  bool enabled = false, connected = false, subscribed = false, advertising = false, upstream_ready = false;
  uint32_t failures = 0;
  bool authorized = false;
  uint32_t ingress_high_water = 0, ingress_bytes = 0, upstream_batches = 0;
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

// WFB branding is a runtime policy; the saved user name remains untouched.
void set_wfb_name_override(bool enabled);

bool name_locked();

bool set_name(std::string_view name);

// Persisted preference, retained while the WFB name override is active.
std::string name();

// Effective GAP/advertising name.
std::string runtime_name();

void set_paused(bool paused);

// Opaque startup admission gate; does not affect an already connected phone.
void set_advertising_hold(bool hold);

// Atomic against a phone connection being admitted by the GAP callback.

bool begin_maintenance();

Snapshot snapshot();

bool phone_owns_link();

// Diagnostic counters record packet delivery, not successful field decoding.
void note_display_receive(uint8_t command, bool passive);

struct DisplayReply {
  uint32_t token = 0;
  uint16_t size = 0;
  std::array<uint8_t, bike::max_payload> bytes{};
};

bool receive_display_reply(DisplayReply &reply);

void register_poll_key(bike::ProxyReplyKey key);

// Opaque, already-routed wire packet plus its reply correlation prefix.
bool queue_display_poll(std::span<const uint8_t> packet, bike::ProxyReplyKey key, uint32_t token);

} // namespace ble_proxy
