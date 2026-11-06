#include "ble_security.hpp"
#include "bike/security_policy.hpp"
#include "ble.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "nimble/nimble_port.h"
#include "nvs.h"
#include "store/config/ble_store_config.h"
#include <algorithm>
#include <cstring>

extern "C" void ble_store_config_init();

namespace ble_security
{
namespace
{

constexpr uint16_t none = BLE_HS_CONN_HANDLE_NONE;

struct Policy {
  uint8_t required = 0;
  std::array<uint8_t, 32> fingerprint{};
};

struct Phones {
  uint8_t count = 0;
  std::array<ble_addr_t, 8> peers{};
};

SemaphoreHandle_t mutex;
ble_npl_event work_event;
Policy policy, stored_policy;
Phones phones;
ble_addr_t central_identity{};
bool central_identity_valid = false, forget_pending = false;
bool known = false, policy_valid = false, policy_dirty = true, submit_pending = false, cancel_pending = false;
uint32_t ticket_counter = 0;
uint32_t passkey = 0, revision = 1, applied = 0, phone_revision = 0, submitted_pin = 0, submitted_ticket = 0;
uint16_t upstream = none, phone = none;
uint32_t upstream_generation = 0;
bool authenticating = false;
Prompt state;

void lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  xSemaphoreGive(mutex);
}

void schedule() {
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &work_event);
}

void configure(bool display, bool encrypted) {
  ble_hs_cfg.sm_io_cap =
      display ? (encrypted ? BLE_HS_IO_DISPLAY_ONLY : BLE_HS_IO_NO_INPUT_OUTPUT) : BLE_HS_IO_KEYBOARD_ONLY;
  ble_hs_cfg.sm_bonding = display ? encrypted : 1;
  ble_hs_cfg.sm_mitm = display ? encrypted : 1;
}

void delete_peer(uint16_t handle) {
  ble_gap_conn_desc desc{};
  if (handle != none && !ble_gap_conn_find(handle, &desc))
    ble_store_util_delete_peer(&desc.peer_id_addr);
}

bool persist(const Policy &value, const Phones &list) {
  nvs_handle_t h;
  if (nvs_open("ble_sec", NVS_READWRITE, &h) != ESP_OK)
    return false;
  auto error = nvs_set_blob(h, "policy", &value, sizeof(value));
  if (error == ESP_OK)
    error = nvs_set_blob(h, "phones", &list, sizeof(list));
  if (error == ESP_OK)
    error = nvs_commit(h);
  nvs_close(h);
  return error == ESP_OK;
}

void work(ble_npl_event *) {
  lock();
  const auto current_policy = policy;
  const auto current_revision = revision;
  const bool dirty = policy_dirty;
  policy_dirty = false;
  const auto current_phone = phone;
  const bool enter = submit_pending, cancel = cancel_pending;
  const auto pin = submitted_pin, ticket = submitted_ticket;
  submit_pending = cancel_pending = false;
  submitted_pin = 0;
  const auto handle = upstream;
  const bool same_ticket = ticket == state.ticket;
  const bool valid = same_ticket && state.waiting && handle != none;
  const bool changed = !policy_valid || current_policy.required != stored_policy.required ||
                       current_policy.fingerprint != stored_policy.fingerprint;
  const auto old_phones = phones;
  const bool forget = forget_pending;
  forget_pending = false;
  auto central = central_identity;
  bool keep_central = central_identity_valid;
  unlock();
  ble_gap_conn_desc central_desc{};
  if (handle != none && !ble_gap_conn_find(handle, &central_desc)) {
    central = central_desc.peer_id_addr;
    keep_central = true;
  }
  if (forget && keep_central) {
    ble_store_util_delete_peer(&central);
    nvs_handle_t h;
    if (nvs_open("ble_sec", NVS_READWRITE, &h) == ESP_OK) {
      nvs_erase_key(h, "central");
      nvs_commit(h);
      nvs_close(h);
    }
    lock();
    central_identity_valid = false;
    unlock();
  }
  if (dirty) {
    bool revoked = true;
    if (changed) {
      if (current_phone != none) {
        ble_gap_terminate(current_phone, BLE_ERR_REM_USER_CONN_TERM);
        delete_peer(current_phone);
      }
      std::array<ble_addr_t, 8> peers{};
      int count = 0;
      if (!ble_store_util_bonded_peers(peers.data(), &count, peers.size())) {
        for (int i = 0; i < count; ++i)
          if (!keep_central || std::memcmp(&peers[i], &central, sizeof(central))) {
            const auto error = ble_store_util_delete_peer(&peers[i]);
            if (error && error != BLE_HS_ENOENT)
              revoked = false;
          }
      }
      else
        revoked = false;
    }
    const Phones next_phones = changed ? Phones{} : old_phones;
    const bool saved = revoked && persist(current_policy, next_phones);
    lock();
    if (revision == current_revision && saved) {
      stored_policy = current_policy;
      policy_valid = true;
      applied = current_revision;
      phones = next_phones;
    }
    unlock();
  }
  if (valid && enter) {
    ble_sm_io io{};
    io.action = BLE_SM_IOACT_INPUT;
    io.passkey = pin;
    const int error = ble_sm_inject_io(handle, &io);
    lock();
    if (state.ticket == ticket) {
      state.open = error != 0;
      state.failed = error != 0;
      state.waiting = false;
    }
    unlock();
    if (error)
      ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
  }
  if (cancel && same_ticket && handle != none)
    ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
}

} // namespace

void init() {
  mutex = xSemaphoreCreateMutex();
  configASSERT(mutex);
  ble_npl_event_init(&work_event, work, nullptr);
  nvs_handle_t h;
  if (nvs_open("ble_sec", NVS_READONLY, &h) == ESP_OK) {
    size_t size = sizeof(stored_policy);
    policy_valid = nvs_get_blob(h, "policy", &stored_policy, &size) == ESP_OK && size == sizeof(stored_policy) &&
                   stored_policy.required <= 1;
    size = sizeof(phones);
    if (nvs_get_blob(h, "phones", &phones, &size) != ESP_OK || size != sizeof(phones) ||
        phones.count > phones.peers.size())
      phones = {};
    size = sizeof(central_identity);
    central_identity_valid = nvs_get_blob(h, "central", &central_identity, &size) == ESP_OK &&
                             size == sizeof(central_identity) && central_identity.type <= 3;
    nvs_close(h);
  }
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  configure(false, true);
  ble_store_config_init();
}

void upstream_connected(uint16_t handle, uint32_t generation) {
  configure(false, true);
  lock();
  upstream = handle;
  upstream_generation = generation;
  authenticating = false;
  state = {};
  state.ticket = ++ticket_counter;
  unlock();
}

bool secure_upstream(uint16_t handle) {
  configure(false, true);
  const auto error = ble_gap_security_initiate(handle);
  const bool started = error == 0 || error == BLE_HS_EALREADY;
  lock();
  if (handle == upstream && started)
    authenticating = true;
  unlock();
  return started;
}

void upstream_passkey(uint16_t handle, int action) {
  lock();
  if (handle == upstream) {
    authenticating = true;
    state.ticket = ++ticket_counter;
    state.open = true;
    state.failed = action != BLE_SM_IOACT_INPUT;
    state.waiting = !state.failed;
  }
  unlock();
  if (action != BLE_SM_IOACT_INPUT)
    ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
}

void upstream_result(uint16_t handle, bool success) {
  if (!success)
    delete_peer(handle);
  else {
    ble_gap_conn_desc desc{};
    if (!ble_gap_conn_find(handle, &desc)) {
      lock();
      central_identity = desc.peer_id_addr;
      central_identity_valid = true;
      unlock();
      nvs_handle_t h;
      if (nvs_open("ble_sec", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "central", &desc.peer_id_addr, sizeof(desc.peer_id_addr));
        nvs_commit(h);
        nvs_close(h);
      }
    }
  }
  lock();
  if (handle == upstream) {
    authenticating = false;
    state.ticket = ++ticket_counter;
    state.open = !success;
    state.failed = !success;
    state.waiting = false;
  }
  unlock();
}

void upstream_disconnected(uint16_t handle, uint32_t generation) {
  lock();
  if (handle == upstream && generation == upstream_generation) {
    upstream = none;
    if (state.waiting || authenticating) {
      state.ticket = ++ticket_counter;
      state.failed = state.open = true;
      state.waiting = false;
    }
    authenticating = false;
  }
  unlock();
}

void forget_upstream() {
  lock();
  state = {};
  state.ticket = ++ticket_counter;
  forget_pending = true;
  unlock();
  schedule();
}

Prompt prompt() {
  lock();
  const auto result = state;
  unlock();
  return result;
}

bool upstream_waiting() {
  lock();
  const bool result = state.waiting || authenticating;
  unlock();
  return result;
}

void submit(uint32_t ticket, uint32_t pin) {
  lock();
  if (pin <= 999999 && ticket == state.ticket && state.waiting) {
    submitted_pin = pin;
    submitted_ticket = ticket;
    submit_pending = true;
  }
  unlock();
  schedule();
}

void cancel(uint32_t ticket) {
  lock();
  if (ticket == state.ticket) {
    state.open = false;
    state.failed = false;
    state.waiting = false;
    submitted_ticket = ticket;
    cancel_pending = true;
  }
  unlock();
  ble::cancel_pairing();
  schedule();
}

void retry() {
  lock();
  state.open = state.failed = state.waiting = false;
  unlock();
  ble::retry_pairing();
}

void set_bike_pin(bool required, bool ready, uint32_t pin, const std::array<uint8_t, 32> &fingerprint) {
  lock();
  Policy next;
  next.required = required;
  if (required)
    next.fingerprint = fingerprint;
  if (next.required != policy.required || next.fingerprint != policy.fingerprint)
    ++revision;
  policy = next;
  known = !required || ready;
  passkey = known && required ? pin : 0;
  policy_dirty = true;
  unlock();
  schedule();
}

bool proxy_ready() {
  lock();
  const bool ready = known && applied == revision;
  unlock();
  return ready;
}

bool proxy_authorized(uint16_t handle) {
  lock();
  const bike::ProxyAuthorization authorization{bool(policy.required), known, revision, applied, phone_revision};
  const bool connected = phone == handle;
  unlock();
  if (!connected || !authorization.ready())
    return false;
  ble_gap_conn_desc desc{};
  if (authorization.pin_required && ble_gap_conn_find(handle, &desc))
    return false;
  return authorization.allows(desc.sec_state.encrypted, desc.sec_state.authenticated);
}

void phone_connected(uint16_t handle) {
  lock();
  phone = handle;
  phone_revision = revision;
  const bool required = policy.required;
  unlock();
  configure(true, required);
  if (required) {
    const auto error = ble_gap_security_initiate(handle);
    if (error && error != BLE_HS_EALREADY)
      ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
  }
}

void phone_disconnected(uint16_t handle) {
  lock();
  if (phone == handle)
    phone = none;
  unlock();
}

void phone_passkey(uint16_t handle, int action) {
  lock();
  const bool allowed = phone == handle && known && policy.required && applied == revision && phone_revision == revision;
  const auto pin = passkey;
  unlock();
  if (!allowed || action != BLE_SM_IOACT_DISP) {
    ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
    return;
  }
  ble_sm_io io{};
  io.action = BLE_SM_IOACT_DISP;
  io.passkey = pin;
  if (ble_sm_inject_io(handle, &io))
    ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
}

void phone_result(uint16_t handle, bool success) {
  if (!success || !proxy_authorized(handle)) {
    delete_peer(handle);
    ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
    return;
  }
  ble_gap_conn_desc desc{};
  if (ble_gap_conn_find(handle, &desc) || !desc.sec_state.bonded)
    return;
  lock();
  bool exists = false;
  for (size_t i = 0; i < phones.count; ++i)
    exists |= !std::memcmp(&phones.peers[i], &desc.peer_id_addr, sizeof(ble_addr_t));
  if (!exists && phones.count < phones.peers.size())
    phones.peers[phones.count++] = desc.peer_id_addr;
  const auto list = phones;
  const auto value = policy;
  unlock();
  if (!persist(value, list)) {
    delete_peer(handle);
    ble_gap_terminate(handle, BLE_ERR_AUTH_FAIL);
  }
}

int repeat_pairing(uint16_t handle, bool is_phone) {
  lock();
  const bool required = policy.required;
  unlock();
  configure(is_phone, is_phone ? required : true);
  delete_peer(handle);
  return BLE_GAP_REPEAT_PAIRING_RETRY;
}

} // namespace ble_security
