#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace ble_security
{

struct Prompt {
  bool open = false, failed = false, waiting = false;
  uint32_t ticket = 0;
};

void init(); // After NimBLE initialization; before its host starts.

void upstream_connected(uint16_t handle, uint32_t generation);

bool secure_upstream(uint16_t handle);

void upstream_passkey(uint16_t handle, int action);

void upstream_result(uint16_t handle, bool success);

void upstream_disconnected(uint16_t handle, uint32_t generation);

void forget_upstream();

Prompt prompt();

void submit(uint32_t ticket, uint32_t pin);

void cancel(uint32_t ticket);

void retry();

bool upstream_waiting();

void set_bike_pin(bool required, bool known, uint32_t pin, const std::array<uint8_t, 32> &fingerprint);

bool proxy_ready();

bool proxy_authorized(uint16_t handle);

void phone_connected(uint16_t handle);

void phone_disconnected(uint16_t handle);

void phone_passkey(uint16_t handle, int action);

void phone_result(uint16_t handle, bool success);

int repeat_pairing(uint16_t handle, bool phone);

} // namespace ble_security
