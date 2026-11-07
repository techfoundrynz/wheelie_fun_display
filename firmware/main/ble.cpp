#include "ble.hpp"
#include "bike/ble_name.hpp"
#include "bike/proxy_lease.hpp"
#include "bike/proxy_tx_drain.hpp"
#include "ble_proxy.hpp"
#include "ble_security.hpp"
#include "board.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs.h"
#include "os/os_mbuf.h"
#include "packet_queue.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace ble
{
namespace
{

const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

const ble_uuid128_t rx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

const ble_uuid128_t tx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

struct Request {
  int action = 0;
  ble_addr_t address{};
  bike::IntegrationId integration = bike::IntegrationId::Auto;
  uint32_t session = 0;
  uint32_t proxy_epoch = 0;
  uint16_t size = 0;
  std::array<uint8_t, 1030> bytes{};
};

Snapshot state;
bike::BleLifecycle lifecycle;
SemaphoreHandle_t mutex;
QueueHandle_t requests, received;
std::array<ble_addr_t, 6> addresses{};
ble_addr_t preferred{};
bool has_preferred = false, synced = false;
uint8_t own_address_type;
uint16_t cccd_handle = 0;
uint16_t connection = BLE_HS_CONN_HANDLE_NONE, rx_handle = 0, tx_handle = 0, service_end = 0;
uint64_t retry_at = 0;
unsigned failures = 0;
std::atomic<bool> paused{false};
std::atomic<uint16_t> pending_terminate{BLE_HS_CONN_HANDLE_NONE};
std::atomic<bool> pending_cancel{false};
std::atomic<uint32_t> proxy_requested{0}, proxy_active{0};
bike::ProxyTxDrain proxy_tx;
std::atomic<bool> reset_requested{false};
std::atomic<bool> display_gap{false};

void lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  xSemaphoreGive(mutex);
}

void set_status(Status s) {
  lock();
  lifecycle.status = s;
  unlock();
}

void save_peer() {
  lock();
  nvs_handle_t h;
  if (nvs_open("ble", NVS_READWRITE, &h) != ESP_OK) {
    unlock();
    return;
  }
  if (has_preferred && lifecycle.remembered) {
    std::array<uint8_t, 41> association{1,
                                        uint8_t(state.integration),
                                        preferred.type,
                                        preferred.val[0],
                                        preferred.val[1],
                                        preferred.val[2],
                                        preferred.val[3],
                                        preferred.val[4],
                                        preferred.val[5]};
    std::copy(std::begin(state.connected_name), std::end(state.connected_name), association.begin() + 9);
    nvs_set_blob(h, "vehicle", association.data(), association.size());
    nvs_set_blob(h, "peer", &preferred, sizeof(preferred));
    nvs_set_u8(h, "peer_ok", 1);
  }
  else {
    nvs_erase_key(h, "peer");
    nvs_erase_key(h, "peer_ok");
    nvs_erase_key(h, "vehicle");
  }
  nvs_commit(h);
  nvs_close(h);
  unlock();
}

void retry() {
  lock();
  connection = BLE_HS_CONN_HANDLE_NONE;
  state.rssi_valid = false;
  state.rssi_sample_ms = 0;
  rx_handle = tx_handle = 0;
  lifecycle.retry();
  const auto delay = has_preferred ? std::min(30000u, 1000u << std::min(failures++, 5u)) : 1000u;
  retry_at = esp_timer_get_time() / 1000 + delay;
  unlock();
}

int gap_event(ble_gap_event *, void *);

void begin_connect(ble_addr_t address) {
  if (paused.load())
    return;
  lock();
  if (!lifecycle.selectable()) {
    unlock();
    return;
  }
  const bool saved_candidate = lifecycle.remembered && !std::memcmp(&preferred, &address, sizeof(address));
  preferred = address;
  has_preferred = true;
  if (!saved_candidate)
    state.connected_name[0] = '\0';
  for (int i = 0; i < state.count; ++i)
    if (!std::memcmp(&addresses[i], &address, sizeof(address)) &&
        (std::strcmp(state.devices[i].name, "VESC UART") || !state.connected_name[0]))
      std::snprintf(state.connected_name, sizeof(state.connected_name), "%s", state.devices[i].name);
  lifecycle.select(saved_candidate);
  const auto token = lifecycle.generation;
  unlock();
  ble_gap_disc_cancel();
  if (ble_gap_connect(own_address_type, &address, 10000, nullptr, gap_event,
                      reinterpret_cast<void *>(static_cast<uintptr_t>(token))))
    retry();
}

void scan() {
  ble_gap_disc_params params{};
  params.passive = 0;
  params.filter_duplicates = 0;
  params.itvl = 160;
  params.window = 80;
  lock();
  lifecycle.start_scan();
  const auto token = lifecycle.generation;
  unlock();
  if (ble_gap_disc(own_address_type, 10000, &params, gap_event,
                   reinterpret_cast<void *>(static_cast<uintptr_t>(token))))
    retry();
}

void fail_connection(uint16_t expected = BLE_HS_CONN_HANDLE_NONE) {
  uint16_t handle;
  lock();
  handle = connection;
  unlock();
  if (expected != BLE_HS_CONN_HANDLE_NONE && expected != handle)
    return;
  if (handle != BLE_HS_CONN_HANDLE_NONE)
    ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
  else
    retry();
}

bool current_callback(uint16_t handle, void *arg) {
  lock();
  const bool current =
      handle == connection && lifecycle.current(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
  unlock();
  return current;
}

bool security_error(int status) {
  return status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) ||
         status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC) ||
         status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR);
}

void discovery_failed(uint16_t handle, int error) {
  if (security_error(error)) {
    if (ble_security::secure_upstream(handle))
      return;
    lock();
    lifecycle.security_required();
    unlock();
  }
  fail_connection(handle);
}

int subscribed(uint16_t handle, const ble_gatt_error *error, ble_gatt_attr *, void *arg) {
  if (!current_callback(handle, arg))
    return 0;
  if (error->status) {
    discovery_failed(handle, error->status);
    return 0;
  }
  if (ble_security::upstream_waiting())
    return 0;
  lock();
  const bool ready = lifecycle.subscribed(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
  if (ready) {
    failures = 0;
  }
  unlock();
  if (ready)
    save_peer();
  return 0;
}

int descriptor(uint16_t handle, const ble_gatt_error *error, uint16_t chr_value, const ble_gatt_dsc *dsc, void *arg) {
  if (!current_callback(handle, arg))
    return 0;
  if (!error->status && chr_value == tx_handle && ble_uuid_u16(&dsc->uuid.u) == 0x2902) {
    lock();
    cccd_handle = dsc->handle;
    unlock();
    const uint8_t enabled[] = {1, 0};
    if (const int rc = ble_gattc_write_flat(handle, dsc->handle, enabled, sizeof(enabled), subscribed, arg))
      discovery_failed(handle, rc);
  }
  else if (error->status && error->status != BLE_HS_EDONE)
    discovery_failed(handle, error->status);
  // Absence of a CCCD is caught by the discovery watchdog in the worker.
  return 0;
}

int characteristic(uint16_t handle, const ble_gatt_error *error, const ble_gatt_chr *chr, void *arg) {
  if (!current_callback(handle, arg))
    return 0;
  if (!error->status) {
    lock();
    if (!ble_uuid_cmp(&chr->uuid.u, &rx_uuid.u) && (chr->properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP))
      rx_handle = chr->val_handle;
    if (!ble_uuid_cmp(&chr->uuid.u, &tx_uuid.u) && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY))
      tx_handle = chr->val_handle;
    unlock();
  }
  else if (error->status == BLE_HS_EDONE) {
    if (!rx_handle || !tx_handle)
      fail_connection(handle);
    else if (const int rc = ble_gattc_disc_all_dscs(handle, tx_handle, service_end, descriptor, arg))
      discovery_failed(handle, rc);
  }
  else
    discovery_failed(handle, error->status);
  return 0;
}

int service(uint16_t handle, const ble_gatt_error *error, const ble_gatt_svc *svc, void *arg) {
  if (!current_callback(handle, arg))
    return 0;
  if (!error->status) {
    service_end = svc->end_handle;
    rx_handle = svc->start_handle; // temporarily carry the start until service discovery completes
  }
  else if (error->status == BLE_HS_EDONE) {
    const auto start_handle = rx_handle;
    rx_handle = 0;
    if (!service_end)
      fail_connection(handle);
    else if (const int rc = ble_gattc_disc_all_chrs(handle, start_handle, service_end, characteristic, arg))
      discovery_failed(handle, rc);
  }
  else
    discovery_failed(handle, error->status);
  return 0;
}

void discover_service(uint16_t handle, void *arg) {
  if (const int rc = ble_gattc_disc_svc_by_uuid(handle, &service_uuid.u, service, arg))
    discovery_failed(handle, rc);
}

int mtu_exchanged(uint16_t handle, const ble_gatt_error *, uint16_t, void *arg) {
  // Even peers that refuse a larger MTU can use the default 20-byte chunks.
  // Complete the exchange before starting the next ATT discovery procedure.
  lock();
  const bool current = connection == handle &&
                       lifecycle.current(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg))) &&
                       lifecycle.status == Status::Discovering;
  unlock();
  if (current) {
    ESP_LOGI("ble", "VESC ATT MTU: %u bytes", unsigned(ble_att_mtu(handle)));
    discover_service(handle, arg);
  }
  return 0;
}

int gap_event(ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_DISC: {
    if (paused.load())
      break;
    ble_hs_adv_fields fields{};
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data))
      break;
    bool nus = false;
    for (int i = 0; i < fields.num_uuids128; ++i)
      nus |= !ble_uuid_cmp(&fields.uuids128[i].u, &service_uuid.u);
    char name[32]{};
    if (fields.name)
      std::memcpy(name, fields.name, std::min<size_t>(fields.name_len, sizeof(name) - 1));
    const bool named = bike::preferred_bike_name(name) || std::strstr(name, "VESC");
    bool wanted, known = false;
    lock();
    const bool active_scan =
        lifecycle.scanning && lifecycle.current(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
    wanted = has_preferred && !std::memcmp(&preferred, &event->disc.addr, sizeof(preferred));
    for (int i = 0; i < state.count; ++i)
      known |= !std::memcmp(&addresses[i], &event->disc.addr, sizeof(ble_addr_t));
    unlock();
    if (!active_scan)
      break;
    if (!nus && !named && !wanted && !known)
      break;
    lock();
    int i = 0;
    for (; i < state.count; ++i)
      if (!std::memcmp(&addresses[i], &event->disc.addr, sizeof(ble_addr_t)))
        break;
    if (i == int(state.devices.size()) && bike::preferred_bike_name(name)) {
      for (int candidate = state.count - 1; candidate >= 0; --candidate)
        if (!bike::preferred_bike_name(state.devices[candidate].name)) {
          i = candidate;
          addresses[i] = event->disc.addr;
          state.devices[i] = {};
          break;
        }
    }
    if (i < int(state.devices.size())) {
      if (i == state.count) {
        addresses[i] = event->disc.addr;
        ++state.count;
      }
      auto &d = state.devices[i];
      if (name[0])
        std::snprintf(d.name, sizeof(d.name), "%s", name);
      else if (!d.name[0])
        std::snprintf(d.name, sizeof(d.name), "VESC UART");
      const auto *a = event->disc.addr.val;
      std::snprintf(d.address, sizeof(d.address), "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
      d.rssi = event->disc.rssi;
    }
    unlock();
    if (wanted)
      begin_connect(event->disc.addr);
    break;
  }
  case BLE_GAP_EVENT_DISC_COMPLETE: {
    lock();
    const bool scanning =
        lifecycle.scanning && lifecycle.current(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
    unlock();
    if (scanning)
      retry();
    break;
  }
  case BLE_GAP_EVENT_CONNECT: {
    lock();
    const bool current = lifecycle.current(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg))) &&
                         lifecycle.status == Status::Connecting;
    if (current && !event->connect.status) {
      connection = event->connect.conn_handle;
      state.rssi_valid = false;
      state.rssi_sample_ms = 0;
      cccd_handle = service_end = rx_handle = tx_handle = 0;
      lifecycle.connected(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
    }
    unlock();
    if (!current) {
      if (!event->connect.status)
        ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      break;
    }
    if (event->connect.status) {
      retry();
      break;
    }
    ble_gap_conn_desc description{};
    if (!ble_gap_conn_find(event->connect.conn_handle, &description))
      ESP_LOGI("ble", "VESC link: interval=%.2f ms latency=%u timeout=%u ms", description.conn_itvl * 1.25,
               unsigned(description.conn_latency), unsigned(description.supervision_timeout) * 10);
    ble_security::upstream_connected(event->connect.conn_handle,
                                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
    if (ble_gattc_exchange_mtu(event->connect.conn_handle, mtu_exchanged, arg))
      discover_service(event->connect.conn_handle, arg);
    break;
  }
  case BLE_GAP_EVENT_DISCONNECT:
    ble_security::upstream_disconnected(event->disconnect.conn.conn_handle,
                                        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
    if (current_callback(event->disconnect.conn.conn_handle, arg)) {
      if (ble_security::prompt().failed) {
        lock();
        lifecycle.security_required();
        unlock();
      }
      retry();
    }
    break;
  case BLE_GAP_EVENT_CONN_UPDATE:
    if (current_callback(event->conn_update.conn_handle, arg) && !event->conn_update.status) {
      ble_gap_conn_desc description{};
      if (!ble_gap_conn_find(event->conn_update.conn_handle, &description))
        ESP_LOGI("ble", "VESC link updated: interval=%.2f ms latency=%u timeout=%u ms", description.conn_itvl * 1.25,
                 unsigned(description.conn_latency), unsigned(description.supervision_timeout) * 10);
    }
    break;
  case BLE_GAP_EVENT_NOTIFY_RX: {
    Received packet{};
    lock();
    packet.session = lifecycle.session;
    const bool ready = lifecycle.status == Status::Ready && connection == event->notify_rx.conn_handle &&
                       tx_handle == event->notify_rx.attr_handle &&
                       lifecycle.current(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(arg)));
    unlock();
    auto n = OS_MBUF_PKTLEN(event->notify_rx.om);
    if (!ready)
      break;
    if (n > packet.bytes.size() || os_mbuf_copydata(event->notify_rx.om, 0, n, packet.bytes.data())) {
      fail_connection();
      break;
    }
    packet.size = n;
    ble_proxy::upstream(transport::Kind::Ble, std::span<const uint8_t>(packet.bytes.data(), packet.size),
                        packet.session);
    // Dropping any stream fragment would corrupt packet boundaries. Reconnect instead.
    if (xQueueSend(received, &packet, 0) != pdTRUE) {
      if (proxy_requested.load()) {
        // The phone's bounded stream queue remains authoritative. Passive
        // dashboard parsing must not break an app's bulk transfer.
        xQueueReset(received);
        display_gap.store(true);
      }
      else
        fail_connection();
    }
    break;
  }
  case BLE_GAP_EVENT_PASSKEY_ACTION:
    if (current_callback(event->passkey.conn_handle, arg)) {
      lock();
      if (lifecycle.status == Status::Ready)
        ++lifecycle.session;
      lifecycle.status = Status::Discovering;
      unlock();
      ble_security::upstream_passkey(event->passkey.conn_handle, event->passkey.params.action);
    }
    break;
  case BLE_GAP_EVENT_ENC_CHANGE:
    if (current_callback(event->enc_change.conn_handle, arg)) {
      ble_gap_conn_desc desc{};
      const bool authenticated = !event->enc_change.status &&
                                 !ble_gap_conn_find(event->enc_change.conn_handle, &desc) && desc.sec_state.encrypted &&
                                 desc.sec_state.authenticated;
      ble_security::upstream_result(event->enc_change.conn_handle, authenticated);
      if (!authenticated) {
        lock();
        lifecycle.security_required();
        unlock();
        fail_connection(event->enc_change.conn_handle);
      }
      else {
        lock();
        const auto cccd = cccd_handle;
        const bool discovering = lifecycle.status == Status::Discovering;
        unlock();
        if (cccd && discovering) {
          const uint8_t enabled[]{1, 0};
          if (const int error =
                  ble_gattc_write_flat(event->enc_change.conn_handle, cccd, enabled, sizeof(enabled), subscribed, arg))
            discovery_failed(event->enc_change.conn_handle, error);
        }
      }
    }
    break;
  case BLE_GAP_EVENT_REPEAT_PAIRING:
    if (current_callback(event->repeat_pairing.conn_handle, arg))
      return ble_security::repeat_pairing(event->repeat_pairing.conn_handle, false);
    return BLE_GAP_REPEAT_PAIRING_IGNORE;
  default:
    break;
  }
  return 0;
}

void sync() {
  if (ble_hs_id_infer_auto(0, &own_address_type)) {
    set_status(Status::Error);
    return;
  }
  lock();
  synced = true;
  retry_at = 0;
  lifecycle.status = lifecycle.paused             ? Status::Paused
                     : lifecycle.security_blocked ? Status::SecurityRequired
                     : lifecycle.remembered       ? Status::Retry
                                                  : Status::Disconnected;
  unlock();
  ble_proxy::host_sync(own_address_type);
}

void reset(int reason) {
  ESP_LOGW("ble", "Host reset: %d", reason);
  lock();
  synced = false;
  unlock();
  retry();
  ble_proxy::host_reset();
}

void host(void *) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void worker(void *) {
  Request request;
  uint64_t discovering_since = 0, next_rssi = 0;
  uint32_t rssi_session = 0;
  int rssi_error = 0;
  for (;;) {
    // A lease changes only between complete queued packets. A dashboard
    // packet already being written finishes before any phone fragment starts.
    proxy_active.store(proxy_requested.load());
    if (reset_requested.exchange(false)) {
      lock();
      const auto handle = connection;
      unlock();
      retry();
      xQueueReset(received);
      if (handle != BLE_HS_CONN_HANDLE_NONE)
        ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    const auto link = snapshot();
    ble_proxy::upstream_state(transport::Kind::Ble, link.status == Status::Ready, link.session);
    if (pending_cancel.exchange(false)) {
      ble_gap_disc_cancel();
      ble_gap_conn_cancel();
    }
    const auto terminate = pending_terminate.exchange(BLE_HS_CONN_HANDLE_NONE);
    if (terminate != BLE_HS_CONN_HANDLE_NONE) {
      ble_gap_disc_cancel();
      ble_gap_conn_cancel();
      ble_gap_terminate(terminate, BLE_ERR_REM_USER_CONN_TERM);
    }
    if (paused.load()) {
      ble_gap_disc_cancel();
      ble_gap_conn_cancel();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (xQueueReceive(requests, &request, pdMS_TO_TICKS(20)) == pdTRUE) {
      // Includes stale-session/epoch discards and every early continue below.
      bike::ProxyTxDrain::WriteGuard writing(proxy_tx, request.proxy_epoch);
      if (request.action == 1) {
        lock();
        const bool valid = synced && request.session == lifecycle.session && lifecycle.selectable();
        unlock();
        if (valid) {
          lock();
          state.integration = request.integration;
          unlock();
          begin_connect(request.address);
        }
      }
      else if (request.action == 2) {
        ble_security::forget_upstream();
        lock();
        has_preferred = false;
        state.count = 0;
        const auto h = connection;
        connection = BLE_HS_CONN_HANDLE_NONE;
        rx_handle = tx_handle = 0;
        lifecycle.forget();
        failures = 0;
        retry_at = 0;
        unlock();
        save_peer();
        ble_gap_conn_cancel();
        ble_gap_disc_cancel();
        if (h != BLE_HS_CONN_HANDLE_NONE)
          ble_gap_terminate(h, BLE_ERR_REM_USER_CONN_TERM);
      }
      else if (request.action == 4) {
        lock();
        lifecycle.security_required();
        unlock();
        fail_connection();
      }
      else if (request.action == 3) {
        lock();
        lifecycle.security_blocked = false;
        lifecycle.retry();
        retry_at = 0;
        unlock();
      }
      else {
        lock();
        const bool ownership = bike::proxy_accepts_tx(request.proxy_epoch, proxy_requested.load(), proxy_active.load());
        const bool ready = lifecycle.status == Status::Ready && request.session == lifecycle.session && ownership;
        const auto h = connection, rx = rx_handle;
        unlock();
        if (!ready)
          continue;
        const size_t chunk = std::max(20, int(ble_att_mtu(h)) - 3);
        for (size_t offset = 0; offset < request.size;) {
          const size_t len = std::min(chunk, request.size - offset);
          const auto started = uint64_t(esp_timer_get_time() / 1000);
          const auto deadline = started + 2000;
          int rc = 0;
          bool canceled = false;
          for (;;) {
            lock();
            // Accepted old-epoch batches finish during a normal phone handoff.
            // A real pause/reset/session change still cancels the current batch.
            const bool owner_current = bike::proxy_continues_tx(request.proxy_epoch, proxy_requested.load());
            const bool same = connection == h && lifecycle.session == request.session &&
                              lifecycle.status == Status::Ready && !paused.load() && !reset_requested.load() &&
                              owner_current;
            unlock();
            if (!same) {
              canceled = true;
              break;
            }
            rc = ble_gattc_write_no_rsp_flat(h, rx, request.bytes.data() + offset, len);
            // NimBLE already retains controller-credit EAGAIN internally and
            // returns success. Retry only errors that did not accept this write.
            if (!rc || (rc != BLE_HS_ENOMEM && rc != BLE_HS_EBUSY))
              break;
            if (uint64_t(esp_timer_get_time() / 1000) >= deadline)
              break;
            // Shared central/peripheral pools may need several BLE intervals to
            // recover. Yield so the host and notification worker can make progress.
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(10)));
          }
          if (canceled)
            break; // The caller already owns the reset; never terminate its replacement link.
          if (rc) {
            ESP_LOGW("ble", "VESC write failed: error=%d offset=%u/%u waited_ms=%u", rc, unsigned(offset),
                     unsigned(request.size), unsigned(uint64_t(esp_timer_get_time() / 1000) - started));
            fail_connection(h);
            break;
          }
          offset += len; // Advance only after NimBLE accepts the exact current bytes.
        }
      }
    }
    const auto now = uint64_t(esp_timer_get_time() / 1000);
    lock();
    const auto rssi_handle = connection;
    const auto current_session = lifecycle.session;
    const bool rssi_ready = lifecycle.status == Status::Ready;
    if (!rssi_ready || rssi_session != current_session) {
      state.rssi_valid = false;
      state.rssi_sample_ms = 0;
      next_rssi = 0;
      rssi_session = current_session;
      rssi_error = 0;
    }
    unlock();
    if (rssi_ready && now >= next_rssi) {
      // HCI access belongs to the BLE worker, outside application mutexes and
      // between complete queued writes. Never sample the phone's handle.
      int8_t signal = 127;
      const int error = ble_gap_conn_rssi(rssi_handle, &signal);
      const bool valid = error == 0 && signal >= -127 && signal <= 20;
      const auto sampled_ms = uint64_t(esp_timer_get_time() / 1000);
      lock();
      bool first_sample = false;
      if (connection == rssi_handle && lifecycle.session == current_session && lifecycle.status == Status::Ready) {
        first_sample = valid && !state.rssi_sample_ms;
        state.rssi = signal;
        state.rssi_valid = valid;
        if (valid)
          state.rssi_sample_ms = sampled_ms;
      }
      unlock();
      if (first_sample)
        ESP_LOGI("ble", "VESC RSSI: handle=%u session=%u signal=%d dBm", unsigned(rssi_handle),
                 unsigned(current_session), int(signal));
      const int sample_error = valid ? 0 : error ? error : BLE_HS_ECONTROLLER;
      if (sample_error && sample_error != rssi_error)
        ESP_LOGW("ble", "VESC RSSI unavailable: handle=%u session=%u error=%d raw=%d", unsigned(rssi_handle),
                 unsigned(current_session), error, int(signal));
      rssi_error = sample_error;
      next_rssi = now + 1000;
    }
    lock();
    const bool do_scan = synced && lifecycle.can_scan() && now >= retry_at;
    const bool discovering = lifecycle.status == Status::Discovering;
    unlock();
    if (do_scan)
      scan();
    if (!discovering)
      discovering_since = 0;
    else if (!discovering_since || ble_security::upstream_waiting())
      discovering_since = now;
    else if (now - discovering_since > 10000) {
      fail_connection();
      discovering_since = now;
    }
  }
}

} // namespace

void start() {
  // Keep normal operation comparable with throughput measurements; retain
  // warnings/errors and the concise link/MTU diagnostics emitted by this file.
  esp_log_level_set("NimBLE", ESP_LOG_WARN);
  mutex = xSemaphoreCreateMutex();
  static StaticQueue_t request_control, receive_control;
  requests = create_packet_queue<Request>(8, request_control);
  received = create_packet_queue<Received>(12, receive_control);
  configASSERT(mutex && requests && received);
  bool verified_peer = false;
  nvs_handle_t h;
  if (nvs_open("ble", NVS_READONLY, &h) == ESP_OK) {
    size_t size = sizeof(preferred);
    has_preferred = nvs_get_blob(h, "peer", &preferred, &size) == ESP_OK && size == sizeof(preferred);
    uint8_t verified = 0;
    verified_peer = has_preferred && nvs_get_u8(h, "peer_ok", &verified) == ESP_OK && verified == 1;
    std::array<uint8_t, 41> association{};
    size = association.size();
    const auto association_error = nvs_get_blob(h, "vehicle", association.data(), &size);
    if (association_error != ESP_ERR_NVS_NOT_FOUND) {
      has_preferred = verified_peer = association_error == ESP_OK && size == association.size() &&
                                      association[0] == 1 && (association[1] <= 2 || association[1] == 255) &&
                                      association[2] <= 3 && association.back() == 0;
      if (has_preferred) {
        state.integration = bike::integration_id(association[1]);
        preferred.type = association[2];
        std::copy(association.begin() + 3, association.begin() + 9, preferred.val);
        std::copy(association.begin() + 9, association.end(), state.connected_name);
      }
    }
    nvs_close(h);
  }
  lifecycle.restore(verified_peer);
  ESP_ERROR_CHECK(board::init_radio());
  ESP_ERROR_CHECK(nimble_port_init());
  ble_security::init();
  ble_proxy::init();
  ble_hs_cfg.sync_cb = sync;
  ble_hs_cfg.reset_cb = reset;
  ble_att_set_preferred_mtu(185);
  nimble_port_freertos_init(host);
  configASSERT(xTaskCreate(worker, "vesc_ble_tx", 6144, nullptr, 5, nullptr) == pdPASS);
}

Snapshot snapshot() {
  lock();
  auto copy = state;
  copy.status = lifecycle.status;
  copy.session = lifecycle.session;
  copy.rssi_valid = copy.rssi_valid && lifecycle.status == Status::Ready;
  if (has_preferred) {
    copy.peer_identity[0] = preferred.type;
    std::copy_n(preferred.val, 6, copy.peer_identity.begin() + 1);
  }
  if (copy.status != Status::Ready)
    copy.connected_name[0] = '\0';
  unlock();
  return copy;
}

void connect(std::string_view address, int integration) {
  Request r;
  r.action = 1;
  lock();
  int i = 0;
  for (; i < state.count; ++i)
    if (address == state.devices[i].address)
      break;
  const bool valid = i >= 0 && i < state.count && lifecycle.selectable();
  if (valid) {
    r.address = addresses[i];
    r.session = lifecycle.session;
    r.integration = bike::integration_id(integration);
  }
  unlock();
  if (!valid)
    return;
  xQueueSend(requests, &r, 0);
}

void forget() {
  Request r;
  r.action = 2;
  xQueueSend(requests, &r, 0);
}

void cancel_pairing() {
  Request request;
  request.action = 4;
  xQueueSend(requests, &request, 0);
}

void retry_pairing() {
  Request request;
  request.action = 3;
  xQueueSend(requests, &request, 0);
}

bool send(std::span<const uint8_t> bytes) {
  if (paused.load() || proxy_requested.load())
    return false;
  if (bytes.empty() || bytes.size() > Request{}.bytes.size())
    return false;
  Request r;
  const auto link = snapshot();
  if (link.status != Status::Ready)
    return false;
  r.session = link.session;
  r.size = bytes.size();
  std::copy(bytes.begin(), bytes.end(), r.bytes.begin());
  return xQueueSend(requests, &r, 0) == pdTRUE;
}

bool receive(Received &packet) {
  if (display_gap.exchange(false)) {
    packet = {};
    packet.session = snapshot().session;
    packet.discontinuity = true;
    return true;
  }
  return xQueueReceive(received, &packet, 0) == pdTRUE;
}

void set_paused(bool value) {
  ble_proxy::source_paused(transport::Kind::Ble, value);
  paused.store(value);
  lock();
  lifecycle.set_paused(value);
  if (value)
    pending_cancel.store(true);
  if (value && connection != BLE_HS_CONN_HANDLE_NONE) {
    pending_terminate.store(connection);
    connection = BLE_HS_CONN_HANDLE_NONE;
    rx_handle = tx_handle = 0;
  }
  retry_at = 0;
  unlock();
}

void set_proxy_owner(bool owned, uint32_t epoch) {
  proxy_requested.store(owned ? epoch : 0);
}

bool proxy_ready(uint32_t epoch) {
  return epoch && proxy_requested.load() == epoch && proxy_active.load() == epoch &&
         snapshot().status == Status::Ready && !paused.load();
}

bool proxy_drained(uint32_t epoch) {
  return epoch && proxy_tx.drained() && proxy_requested.load() == epoch && proxy_active.load() == epoch &&
         snapshot().status == Status::Ready && !paused.load();
}

bool send_proxy_fragment(std::span<const uint8_t> bytes, uint32_t epoch) {
  if (!proxy_ready(epoch) || bytes.empty() || bytes.size() > 512)
    return false;
  Request request;
  request.session = snapshot().session;
  request.proxy_epoch = epoch;
  request.size = bytes.size();
  std::copy(bytes.begin(), bytes.end(), request.bytes.begin());
  proxy_tx.reserve();
  if (xQueueSend(requests, &request, 0) == pdTRUE)
    return true;
  proxy_tx.rejected();
  return false;
}

void reset_link() {
  reset_requested.store(true);
}

} // namespace ble
