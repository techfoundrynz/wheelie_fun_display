#include "ble_proxy.hpp"
#include "bike/proxy_lease.hpp"
#include "bike/security_policy.hpp"
#include "ble_security.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nvs.h"
#include "os/os_mbuf.h"
#include "packet_queue.hpp"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "transport.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace ble_proxy
{
namespace
{

const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

const ble_uuid128_t rx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

const ble_uuid128_t tx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

struct Fragment {
  uint32_t epoch = 0, session = 0;
  uint16_t size = 0;
  std::array<uint8_t, 512> bytes{};
};

SemaphoreHandle_t mutex;
QueueHandle_t ingress, egress;
bike::ProxyLease lease;
uint8_t address_type;
uint16_t phone = BLE_HS_CONN_HANDLE_NONE, tx_handle;
bool advertising = false;
transport::Kind selected_source = transport::Kind::Ble;
std::array<char, 32> proxy_name{};
std::array<char, 32> advertised_name{}; // Proxy worker only.
uint32_t failures = 0;
std::atomic<bool> disconnect_requested{false};

void lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  xSemaphoreGive(mutex);
}

void fail_stream() {
  lock();
  ++failures;
  ++lease.generation;
  lease.subscribed = false;
  unlock();
  disconnect_requested.store(true);
}

bool current(const Fragment &fragment, uint16_t *handle = nullptr) {
  lock();
  const bool valid = lease.accepts(fragment.epoch, fragment.session);
  if (handle)
    *handle = phone;
  const auto current_phone = phone;
  unlock();
  return valid && ble_security::proxy_authorized(current_phone);
}

void release_phone(bool reset_upstream) {
  lock();
  const bool existed = lease.connected;
  lease.detach();
  const auto old_phone = phone;
  phone = BLE_HS_CONN_HANDLE_NONE;
  const auto epoch = lease.generation;
  const auto source = selected_source;
  unlock();
  ble_security::phone_disconnected(old_phone);
  xQueueReset(ingress);
  xQueueReset(egress);
  transport::proxy_ownership(false, epoch);
  if (existed && reset_upstream)
    transport::reset_proxy_link(source);
}

int gap_event(ble_gap_event *event, void *) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT: {
    const bool security_ready = ble_security::proxy_ready();
    lock();
    advertising = false;
    const bool allowed = lease.advertise() && security_ready && !event->connect.status;
    if (allowed) {
      phone = event->connect.conn_handle;
      lease.attach();
    }
    const auto epoch = lease.generation;
    unlock();
    if (allowed) {
      transport::proxy_ownership(true, epoch);
      ble_security::phone_connected(event->connect.conn_handle);
    }
    else if (!event->connect.status)
      ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    break;
  }
  case BLE_GAP_EVENT_DISCONNECT: {
    lock();
    const bool ours = phone == event->disconnect.conn.conn_handle;
    unlock();
    if (ours)
      release_phone(true);
    break;
  }
  case BLE_GAP_EVENT_SUBSCRIBE:
    lock();
    if (event->subscribe.conn_handle == phone && event->subscribe.attr_handle == tx_handle) {
      if (lease.subscribed && !event->subscribe.cur_notify)
        disconnect_requested.store(true);
      lease.subscribed = event->subscribe.cur_notify;
    }
    unlock();
    break;
  case BLE_GAP_EVENT_PASSKEY_ACTION:
    ble_security::phone_passkey(event->passkey.conn_handle, event->passkey.params.action);
    break;
  case BLE_GAP_EVENT_ENC_CHANGE:
    ble_security::phone_result(event->enc_change.conn_handle, event->enc_change.status == 0);
    break;
  case BLE_GAP_EVENT_REPEAT_PAIRING:
    return ble_security::repeat_pairing(event->repeat_pairing.conn_handle, true);
  case BLE_GAP_EVENT_ADV_COMPLETE:
    lock();
    advertising = false;
    unlock();
    break;
  default:
    break;
  }
  return 0;
}

int access(uint16_t connection, uint16_t, ble_gatt_access_ctxt *context, void *) {
  if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
  Fragment fragment;
  lock();
  fragment.epoch = lease.generation;
  fragment.session = lease.upstream_session;
  const bool allowed = connection == phone && lease.accepts(fragment.epoch, fragment.session);
  unlock();
  if (!allowed)
    return BLE_ATT_ERR_UNLIKELY;
  if (!ble_security::proxy_authorized(connection))
    return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
  const auto length = OS_MBUF_PKTLEN(context->om);
  if (!length || length > fragment.bytes.size())
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  if (os_mbuf_copydata(context->om, 0, length, fragment.bytes.data()))
    return BLE_ATT_ERR_UNLIKELY;
  fragment.size = length;
  if (xQueueSend(ingress, &fragment, 0) != pdTRUE) {
    fail_stream();
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};

void update_advertising() {
  const bool security_ready = ble_security::proxy_ready();
  lock();
  const auto name = proxy_name;
  const bool wanted = lease.advertise() && security_ready && name[0];
  const bool active = advertising;
  const bool synced = lease.synced;
  unlock();
  if (!synced)
    return;
  if (!wanted) {
    if (active)
      ble_gap_adv_stop();
    lock();
    advertising = false;
    unlock();
    return;
  }
  if (active) {
    if (!std::strcmp(advertised_name.data(), name.data()))
      return;
    ble_gap_adv_stop();
    lock();
    advertising = false;
    unlock();
  }
  ble_hs_adv_fields fields{};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.uuids128 = &service_uuid;
  fields.num_uuids128 = 1;
  fields.uuids128_is_complete = 1;
  int rc = ble_svc_gap_device_name_set(name.data());
  if (!rc)
    rc = ble_gap_adv_set_fields(&fields);
  if (!rc) {
    ble_hs_adv_fields response{};
    response.name = reinterpret_cast<const uint8_t *>(name.data());
    const auto length = std::strlen(name.data());
    response.name_len = std::min<size_t>(length, 29);
    response.name_is_complete = length <= 29;
    rc = ble_gap_adv_rsp_set_fields(&response);
  }
  if (!rc) {
    std::copy(name.begin(), name.end(), advertised_name.begin());
    ble_gap_adv_params parameters{};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(address_type, nullptr, BLE_HS_FOREVER, &parameters, gap_event, nullptr);
  }
  if (!rc) {
    lock();
    advertising = true;
    unlock();
  }
  // Transient busy while a central connection procedure is running is retried.
}

void worker(void *) {
  Fragment from_phone, to_phone;
  for (;;) {
    if (disconnect_requested.exchange(false)) {
      lock();
      const auto handle = phone;
      const bool synced = lease.synced;
      unlock();
      if (handle != BLE_HS_CONN_HANDLE_NONE && synced) {
        if (ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM))
          release_phone(true);
      }
    }
    update_advertising();
    if (xQueueReceive(ingress, &from_phone, 0) == pdTRUE && current(from_phone)) {
      bool sent = false;
      for (int attempt = 0; attempt < 20 && current(from_phone); ++attempt) {
        if (transport::send_proxy_fragment(std::span<const uint8_t>(from_phone.bytes.data(), from_phone.size),
                                           from_phone.epoch)) {
          sent = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      if (!sent && current(from_phone))
        fail_stream();
    }
    lock();
    const bool active = lease.connected;
    unlock();
    if (xQueueReceive(egress, &to_phone, pdMS_TO_TICKS(active ? 5 : 100)) == pdTRUE) {
      size_t offset = 0;
      while (offset < to_phone.size) {
        uint16_t handle;
        if (!current(to_phone, &handle))
          break;
        const size_t length = std::min<size_t>(std::max(20, int(ble_att_mtu(handle)) - 3), to_phone.size - offset);
        bool sent = false;
        for (int attempt = 0; attempt < 20 && current(to_phone); ++attempt) {
          auto *buffer = ble_hs_mbuf_from_flat(to_phone.bytes.data() + offset, length);
          if (buffer && !ble_gatts_notify_custom(handle, tx_handle, buffer)) {
            sent = true;
            break;
          }
          // notify_custom consumes the mbuf on success and failure.
          vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (!sent) {
          if (current(to_phone))
            fail_stream();
          break;
        }
        offset += length;
      }
    }
  }
}

} // namespace

void init() {
  mutex = xSemaphoreCreateMutex();
  std::copy_n("WB Disp", 7, proxy_name.begin());
  nvs_handle_t settings;
  if (nvs_open("display", NVS_READONLY, &settings) == ESP_OK) {
    std::array<char, 32> stored{};
    size_t size = stored.size();
    if (nvs_get_str(settings, "proxy_name", stored.data(), &size) == ESP_OK && size > 1 && size <= 30 &&
        bike::valid_proxy_name(stored.data()))
      proxy_name = stored;
    nvs_close(settings);
  }
  static StaticQueue_t ingress_control, egress_control;
  ingress = create_packet_queue<Fragment>(12, ingress_control);
  egress = create_packet_queue<Fragment>(16, egress_control);
  configASSERT(mutex && ingress && egress);
  ble_svc_gap_init();
  ble_svc_gatt_init();
  configASSERT(!ble_svc_gap_device_name_set(proxy_name.data()));
  characteristics[0].uuid = &rx_uuid.u;
  characteristics[0].access_cb = access;
  characteristics[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP;
  characteristics[1].uuid = &tx_uuid.u;
  characteristics[1].access_cb = access;
  characteristics[1].flags = BLE_GATT_CHR_F_NOTIFY;
  characteristics[1].val_handle = &tx_handle;
  services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
  services[0].uuid = &service_uuid.u;
  services[0].characteristics = characteristics;
  configASSERT(!ble_gatts_count_cfg(services));
  configASSERT(!ble_gatts_add_svcs(services));
  configASSERT(xTaskCreate(worker, "vesc_proxy", 6144, nullptr, 4, nullptr) == pdPASS);
}

void host_sync(uint8_t type) {
  lock();
  address_type = type;
  lease.synced = true;
  unlock();
}

void host_reset() {
  lock();
  lease.synced = false;
  lease.upstream_ready = false;
  advertising = false;
  unlock();
  release_phone(false);
}

void select_upstream(transport::Kind source) {
  lock();
  selected_source = source;
  lease.set_upstream(false, 0);
  lease.paused = false;
  unlock();
}

void upstream_state(transport::Kind source, bool ready, uint32_t session) {
  lock();
  if (source != selected_source) {
    unlock();
    return;
  }
  const bool disconnect = lease.set_upstream(ready, session);
  unlock();
  if (disconnect)
    disconnect_requested.store(true);
}

void source_paused(transport::Kind source, bool paused) {
  lock();
  const bool selected = source == selected_source;
  unlock();
  if (selected)
    set_paused(paused);
}

void upstream_failed(transport::Kind source) {
  lock();
  const bool selected = source == selected_source;
  unlock();
  if (selected)
    fail_stream();
}

void upstream(transport::Kind source, std::span<const uint8_t> bytes, uint32_t session) {
  Fragment fragment;
  lock();
  fragment.epoch = lease.generation;
  fragment.session = session;
  const bool wanted = source == selected_source && lease.accepts(fragment.epoch, session);
  const auto current_phone = phone;
  unlock();
  if (!wanted || !ble_security::proxy_authorized(current_phone))
    return;
  if (bytes.empty() || bytes.size() > fragment.bytes.size()) {
    fail_stream();
    return;
  }
  fragment.size = bytes.size();
  std::copy(bytes.begin(), bytes.end(), fragment.bytes.begin());
  if (xQueueSend(egress, &fragment, 0) != pdTRUE)
    fail_stream();
}

bool set_name(std::string_view value) {
  if (!bike::valid_proxy_name(value))
    return false;
  const std::string copy(value);
  nvs_handle_t h;
  if (nvs_open("display", NVS_READWRITE, &h) != ESP_OK)
    return false;
  auto error = nvs_set_str(h, "proxy_name", copy.c_str());
  if (error == ESP_OK)
    error = nvs_commit(h);
  nvs_close(h);
  if (error != ESP_OK)
    return false;
  lock();
  proxy_name.fill(0);
  std::copy(value.begin(), value.end(), proxy_name.begin());
  unlock();
  return true;
}

std::string name() {
  lock();
  const std::string value(proxy_name.data());
  unlock();
  return value;
}

void set_enabled(bool enabled) {
  lock();
  lease.enabled = enabled;
  const bool disconnect = !enabled && lease.connected;
  unlock();
  if (disconnect)
    disconnect_requested.store(true);
}

void set_paused(bool paused) {
  lock();
  lease.paused = paused;
  const bool disconnect = paused && lease.connected;
  unlock();
  if (disconnect)
    disconnect_requested.store(true);
}

bool begin_maintenance() {
  lock();
  const bool reserved = lease.begin_maintenance();
  unlock();
  return reserved;
}

Snapshot snapshot() {
  lock();
  const Snapshot result{lease.enabled, lease.connected, lease.subscribed, advertising, lease.upstream_ready, failures};
  unlock();
  return result;
}

bool phone_owns_link() {
  lock();
  const bool connected = lease.connected;
  unlock();
  return connected;
}

} // namespace ble_proxy
