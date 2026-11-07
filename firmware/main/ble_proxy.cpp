#include "ble_proxy.hpp"
#include "bike/proxy_byte_ring.hpp"
#include "bike/proxy_frame_batcher.hpp"
#include "bike/proxy_lease.hpp"
#include "bike/security_policy.hpp"
#include "ble_security.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "external_state.hpp"
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

struct Poll {
  uint32_t epoch = 0, session = 0, token = 0;
  uint16_t size = 0;
  bike::ProxyReplyKey key;
  std::array<uint8_t, 32> bytes{};
};

SemaphoreHandle_t mutex;
QueueHandle_t polls, display_replies;

struct TelemetryCounters {
  std::array<uint32_t, 2> queued{}, injected{}, owned{}, delivered{};
  std::array<uint64_t, 2> ui_ms{};
  uint32_t passive47 = 0;
};

TelemetryCounters telemetry_counters;

int telemetry_family(uint8_t command) {
  return command == 47 ? 0 : command == 36 ? 1 : -1;
}

struct RelayBuffers {
  bike::ProxyByteRing<> ingress, upstream, outgoing;
  bike::ProxyFrameBatcher batcher;
  uint32_t epoch = 0, session = 0, batches = 0;
  uint64_t last_rx = 0;

  void reset(uint32_t next_epoch, uint32_t next_session) {
    epoch = next_epoch;
    session = next_session;
    batches = 0;
    last_rx = 0;
    ingress.reset(epoch);
    upstream.reset(epoch);
    outgoing.reset(epoch);
    batcher.reset();
  }
};

ExternalState<RelayBuffers> relay;
bool accepting = false, handoff = false, source_continuity = true;
uint64_t handoff_drained_at = 0;
uint32_t last_high_water = 0, last_ingress_bytes = 0, last_batches = 0;
ExternalState<bike::ProxyPollQueueCredits> poll_queue_credits;

struct OwnedReceive {
  DisplayReply reply;
  bike::ProxyReplyKey key;
  uint32_t epoch = 0;
  bike::Decoder decoder{[](void *context, std::span<const uint8_t> payload) {
                          auto &self = *static_cast<OwnedReceive *>(context);
                          self.reply.size = payload.size();
                          std::copy(payload.begin(), payload.end(), self.reply.bytes.begin());
                          const bool queued = xQueueSend(display_replies, &self.reply, 0) == pdTRUE;
                          xSemaphoreTake(mutex, portMAX_DELAY);
                          poll_queue_credits->release(self.key, self.epoch);
                          const auto family = telemetry_family(payload[0]);
                          if (family >= 0)
                            ++telemetry_counters.owned[family];
                          xSemaphoreGive(mutex);
#ifdef WFD_BLE_BENCHMARK
                          static unsigned logged = 0;
                          if (logged++ < 8)
                            ESP_LOGI("ble_proxy", "Owned reply command=%u bytes=%u token=%u queued=%d", payload[0],
                                     unsigned(payload.size()), unsigned(self.reply.token), queued);
#else
                          (void)queued;
#endif
                        },
                        this};
};

ExternalState<OwnedReceive> display_input;
bike::ProxyLease lease;
bike::ProxyPollArbiter arbiter;
uint8_t address_type;
uint16_t phone = BLE_HS_CONN_HANDLE_NONE, tx_handle;
bool advertising = false, wfb_name_override = false, advertising_hold = false;
transport::Kind selected_source = transport::Kind::Ble;
std::array<char, 32> proxy_name{};
std::array<char, 32> advertised_name{}, gap_name{}; // Proxy worker only.
uint32_t failures = 0;
std::atomic<bool> disconnect_requested{false};

void lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  xSemaphoreGive(mutex);
}

void fail_stream(const char *reason = "stream failure") {
  lock();
  if (!lease.connected || !accepting) {
    unlock();
    return;
  }
  ++failures;
  accepting = false;
  const auto handle = phone;
  unlock();
  if (handle != BLE_HS_CONN_HANDLE_NONE)
    ESP_LOGW("ble_proxy", "Ending phone stream: handle=%u reason=%s", unsigned(handle), reason);
  disconnect_requested.store(true);
}

void release_phone(bool graceful) {
  lock();
  const auto old_phone = phone;
  const bool existed = lease.connected;
  const bool drain = graceful && existed && source_continuity && lease.synced && lease.upstream_ready &&
                     relay->epoch == lease.generation && relay->session == lease.upstream_session;
  accepting = false;
  phone = BLE_HS_CONN_HANDLE_NONE;
  if (drain) {
    // Keep the old backend epoch valid until all previously accepted complete
    // frames are written. No new phone writes are admitted during this phase.
    lease.connected = lease.subscribed = false;
    handoff = true;
    handoff_drained_at = 0;
    arbiter.abandon_display_polls();
    relay->outgoing.reset(relay->epoch);
  }
  else {
    lease.detach();
    handoff = false;
    relay->reset(lease.generation, lease.upstream_session);
  }
  const auto epoch = lease.generation;
  unlock();
  ble_security::phone_disconnected(old_phone);
  xQueueReset(polls);
  if (!drain) {
    xQueueReset(display_replies);
    transport::proxy_ownership(false, epoch);
  }
}

int gap_event(ble_gap_event *event, void *) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT: {
    const bool security_ready = ble_security::proxy_ready();
    lock();
    advertising = false;
    const bool allowed = lease.advertise() && !advertising_hold && !handoff && security_ready && !event->connect.status;
    if (allowed) {
      phone = event->connect.conn_handle;
      lease.attach();
      handoff = false;
      accepting = source_continuity = true;
      relay->reset(lease.generation, lease.upstream_session);
    }
    const auto epoch = lease.generation;
    unlock();
    if (allowed) {
      transport::proxy_ownership(true, epoch);
      ble_security::phone_connected(event->connect.conn_handle);
      ble_gap_conn_desc description{};
      const bool found = !ble_gap_conn_find(event->connect.conn_handle, &description);
      ESP_LOGI("ble_proxy", "Phone connected: handle=%u lease=%u peripheral=%d authorized=%d",
               unsigned(event->connect.conn_handle), unsigned(epoch), found && description.role == BLE_GAP_ROLE_SLAVE,
               ble_security::proxy_authorized(event->connect.conn_handle));
    }
    else if (!event->connect.status)
      ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    break;
  }
  case BLE_GAP_EVENT_DISCONNECT: {
    lock();
    const bool ours = phone == event->disconnect.conn.conn_handle;
    unlock();
    if (ours) {
      ESP_LOGI("ble_proxy", "Phone disconnected: handle=%u reason=%d", unsigned(event->disconnect.conn.conn_handle),
               event->disconnect.reason);
      release_phone(true);
    }
    break;
  }
  case BLE_GAP_EVENT_SUBSCRIBE: {
    lock();
    const bool ours = event->subscribe.conn_handle == phone && event->subscribe.attr_handle == tx_handle;
    if (ours) {
      if (lease.subscribed && !event->subscribe.cur_notify)
        disconnect_requested.store(true);
      lease.subscribed = event->subscribe.cur_notify;
    }
    unlock();
    if (ours)
      ESP_LOGI("ble_proxy", "Phone notifications: handle=%u enabled=%d authorized=%d",
               unsigned(event->subscribe.conn_handle), bool(event->subscribe.cur_notify),
               ble_security::proxy_authorized(event->subscribe.conn_handle));
    break;
  }
  case BLE_GAP_EVENT_PASSKEY_ACTION:
    ble_security::phone_passkey(event->passkey.conn_handle, event->passkey.params.action);
    break;
  case BLE_GAP_EVENT_ENC_CHANGE:
    ble_security::phone_result(event->enc_change.conn_handle, event->enc_change.status == 0);
    ESP_LOGI("ble_proxy", "Phone security result: handle=%u status=%d authorized=%d",
             unsigned(event->enc_change.conn_handle), event->enc_change.status,
             ble_security::proxy_authorized(event->enc_change.conn_handle));
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
  const bool allowed = connection == phone && accepting && lease.accepts(fragment.epoch, fragment.session);
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
  lock();
  const bool still_current = accepting && lease.accepts(fragment.epoch, fragment.session);
  const bool appended = still_current && relay->ingress.append({fragment.bytes.data(), fragment.size}, fragment.epoch);
  unlock();
  if (!still_current)
    return BLE_ATT_ERR_UNLIKELY;
  if (!appended) {
    fail_stream("phone ingress byte buffer exceeded");
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};

void update_advertising() {
  const bool security_ready = ble_security::proxy_ready();
  const auto name = runtime_name();
  lock();
  const bool wanted = lease.advertise() && !advertising_hold && !handoff && security_ready && !name.empty();
  const bool active = advertising;
  const bool synced = lease.synced;
  unlock();
  if (!synced)
    return;
  // GAP reads must reflect integration changes even while a phone is connected.
  if (std::strcmp(gap_name.data(), name.c_str())) {
    if (ble_svc_gap_device_name_set(name.c_str()))
      return;
    gap_name.fill(0);
    std::copy(name.begin(), name.end(), gap_name.begin());
  }
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
  int rc = ble_gap_adv_set_fields(&fields);
  if (!rc) {
    ble_hs_adv_fields response{};
    response.name = reinterpret_cast<const uint8_t *>(name.data());
    const auto length = std::strlen(name.data());
    response.name_len = std::min<size_t>(length, 29);
    response.name_is_complete = length <= 29;
    rc = ble_gap_adv_rsp_set_fields(&response);
  }
  if (!rc) {
    advertised_name.fill(0);
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
  Fragment input;
  std::array<uint8_t, 521> output{}, owned{};
  Poll poll;
  bool have_poll = false;
  uint32_t arbiter_epoch = 0;
  uint64_t next_telemetry_log = 0;
#ifdef WFD_BLE_BENCHMARK
  uint64_t debug_at = 0;
  unsigned injected_logged = 0;
#endif
  for (;;) {
    bool worked = false;
    const auto now = uint64_t(esp_timer_get_time() / 1000);
    lock();
    if (arbiter_epoch != lease.generation) {
      arbiter_epoch = lease.generation;
      arbiter.reset();
      display_input->decoder.reset();
      xQueueReset(display_replies);
      have_poll = false;
    }
    const bool timed_out = !handoff && arbiter.timed_out(now);
    unlock();
    if (timed_out)
      fail_stream("injected display reply timeout");
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
    if (!have_poll)
      have_poll = xQueueReceive(polls, &poll, 0) == pdTRUE;
    lock();
    const bool reservation_expired = arbiter.reservation_expired(now, relay->ingress.size() / 20);
    if (reservation_expired)
      arbiter.release_reservation();
    else if (have_poll && !handoff && lease.accepts_display(poll.epoch, poll.session))
      arbiter.reserve(poll.key, now);
    unlock();
    if (reservation_expired && have_poll) {
      lock();
      poll_queue_credits->release(poll.key, poll.epoch);
      unlock();
      have_poll = false;
    }

    // Drain complete, CRC-validated phone frames in coalesced batches. Never
    // spend a fixed delay per ATT write, nor block this path on phone notify.
    for (unsigned service = 0; service < 4; ++service) {
      lock();
      const bool can_forward = source_continuity && relay->epoch == lease.generation &&
                               relay->session == lease.upstream_session && (lease.connected || handoff);
      input.epoch = relay->epoch;
      input.session = relay->session;
      const bool draining = handoff;
      const bool fair_pending = have_poll && !arbiter.pending() && lease.accepts_display(poll.epoch, poll.session);
      const auto already_forwarded = arbiter.phone_since_poll();
      unsigned planned_frames = relay->batcher.partial_frame() ? 1 : 0;
      const auto length = can_forward
                              ? relay->batcher.prepare(relay->ingress, input.bytes,
                                                       [&](std::span<const uint8_t> prefix) {
                                                         if (draining)
                                                           return true;
                                                         if (fair_pending && already_forwarded + planned_frames >= 3)
                                                           return false;
                                                         if (!arbiter.may_forward_phone(prefix))
                                                           return false;
                                                         ++planned_frames;
                                                         return true;
                                                       })
                              : 0;
      const bool invalid = relay->batcher.invalid() || relay->batcher.oversized();
      if (draining && !length && !relay->batcher.partial_frame() && relay->ingress.size()) {
        // These bytes never reached the bike. Discard an abandoned partial
        // frame; VESC's own parser has no idle timeout we could rely on.
        relay->ingress.consume(relay->ingress.size());
        relay->batcher.reset();
      }
      unlock();
      if (!length) {
        if (invalid && !draining)
          fail_stream("invalid or oversized phone frame before forwarding");
        break;
      }
      if (!transport::send_proxy_fragment({input.bytes.data(), length}, input.epoch))
        break; // Backpressure: retain the exact bytes and service other work.
      lock();
      if (relay->epoch == input.epoch && relay->session == input.session) {
        configASSERT(relay->batcher.commit(relay->ingress, length));
        const bool healthy = arbiter.phone({input.bytes.data(), length});
        ++relay->batches;
        if (!healthy)
          accepting = false;
      }
      unlock();
      worked = true;
      if (invalid && !draining)
        fail_stream("invalid phone frame after accepted complete prefix");
    }

    // Parse upstream bytes independently of notification allocation pressure.
    for (unsigned service = 0; service < 4; ++service) {
      lock();
      const auto notify_handle = phone;
      unlock();
      const bool authorized = notify_handle != BLE_HS_CONN_HANDLE_NONE && ble_security::proxy_authorized(notify_handle);
      lock();
      const auto length = relay->upstream.pop(input.bytes);
      if (!length) {
        unlock();
        break;
      }
      size_t size = 0, owned_size = 0;
      const bool healthy = arbiter.upstream(
          {input.bytes.data(), length},
          [&](uint8_t byte) {
            configASSERT(size < output.size());
            output[size++] = byte;
          },
          [&](uint8_t byte) {
            configASSERT(owned_size < owned.size());
            owned[owned_size++] = byte;
          });
      const bool phone_present = lease.connected && lease.subscribed && phone == notify_handle && authorized;
      const bool appended = !phone_present || relay->outgoing.append({output.data(), size}, relay->epoch);
      const bool draining = handoff;
      unlock();
      if (!healthy && !draining)
        fail_stream("invalid upstream frame");
      if (!appended)
        fail_stream("phone notification byte buffer exceeded");
      if (healthy && owned_size && !draining)
        display_input->decoder.feed({owned.data(), owned_size}, now);
      worked = true;
    }

    // One nonblocking notify admission per attempt. If mbufs/ACL credits are
    // busy, the ring retains bytes; upstream phone writes keep making progress.
    for (unsigned service = 0; service < 4; ++service) {
      lock();
      const auto handle = phone;
      const auto epoch = relay->epoch;
      const bool notify = lease.connected && lease.subscribed;
      unlock();
      const size_t maximum = notify ? size_t(std::max(20, int(ble_att_mtu(handle)) - 3)) : 0;
      lock();
      const auto length =
          notify && relay->epoch == epoch && phone == handle
              ? relay->outgoing.copy(std::span(input.bytes).first(std::min(maximum, input.bytes.size())))
              : 0;
      unlock();
      if (!length || !ble_security::proxy_authorized(handle))
        break;
      auto *buffer = ble_hs_mbuf_from_flat(input.bytes.data(), length);
      if (!buffer || ble_gatts_notify_custom(handle, tx_handle, buffer))
        break;
      lock();
      if (relay->epoch == epoch && lease.connected && phone == handle)
        configASSERT(relay->outgoing.consume(length));
      unlock();
      worked = true;
    }

    lock();
    const bool draining = handoff;
    const auto drain_epoch = relay->epoch;
    const auto drain_source = selected_source;
    const bool drained_input = !relay->ingress.size() && !relay->batcher.partial_frame();
    unlock();
    if (draining && drained_input && transport::proxy_drained(drain_source, drain_epoch)) {
      lock();
      const auto handoff_now = uint64_t(esp_timer_get_time() / 1000);
      if (!handoff_drained_at)
        handoff_drained_at = handoff_now;
      const bool quiet = handoff_now >= relay->last_rx && handoff_now - relay->last_rx >= 100 &&
                         handoff_now - handoff_drained_at >= 200;
      const bool local_partial_quiet = handoff_now - handoff_drained_at >= 1000;
      const bool finish = handoff && relay->epoch == drain_epoch && !relay->upstream.size() && quiet &&
                          (arbiter.upstream_boundary() || local_partial_quiet);
      if (finish) {
        last_high_water = relay->ingress.high_water();
        last_ingress_bytes = relay->ingress.pushed();
        last_batches = relay->batches;
        handoff = false;
        lease.detach();
        relay->reset(lease.generation, lease.upstream_session);
      }
      const auto next_epoch = lease.generation;
      unlock();
      if (finish) {
        transport::proxy_ownership(false, next_epoch);
        ESP_LOGI("ble_proxy", "Phone relay drained: bytes=%u input_high_water=%u batches=%u; bike link retained",
                 unsigned(last_ingress_bytes), unsigned(last_high_water), unsigned(last_batches));
      }
    }

    if (!have_poll)
      have_poll = xQueueReceive(polls, &poll, 0) == pdTRUE;
    if (have_poll) {
      lock();
      const bool valid = !handoff && lease.accepts_display(poll.epoch, poll.session);
      if (valid)
        arbiter.reserve(poll.key, now);
      const bool available = valid && arbiter.may_poll(poll.key);
      const bool fair_turn = valid && arbiter.yield_phone_for_poll(true);
      const bool phone_waiting = relay->ingress.size() != 0;
      const bool upstream_waiting = relay->upstream.size() != 0;
#ifdef WFD_BLE_BENCHMARK
      const auto owed = arbiter.phone_replies(poll.key);
      const bool broker_pending = arbiter.pending();
#endif
      unlock();
#ifdef WFD_BLE_BENCHMARK
      if (valid && !available && now >= debug_at) {
        ESP_LOGI("ble_proxy", "Deferred display poll: command=%u token=%u phone_replies=%u broker_pending=%d",
                 unsigned(poll.key.bytes[0]), unsigned(poll.token), unsigned(owed), broker_pending);
        debug_at = now + 1000;
      }
#endif
      if (!valid) {
        have_poll = false;
      }
      else if (available && (fair_turn || !phone_waiting) && !upstream_waiting &&
               transport::proxy_drained(drain_source, poll.epoch)) {
        lock();
        const bool committed = arbiter.begin(
            poll.key, now, (!arbiter.yield_phone_for_poll(true) && relay->ingress.size()) || relay->upstream.size());
        unlock();
        if (committed) {
#ifdef WFD_BLE_BENCHMARK
          if (injected_logged++ < 8)
            ESP_LOGI("ble_proxy", "Inject display poll: command=%u token=%u lease=%u", unsigned(poll.key.bytes[0]),
                     unsigned(poll.token), unsigned(poll.epoch));
#endif
          display_input->reply.token = poll.token;
          display_input->key = poll.key;
          display_input->epoch = poll.epoch;
          display_input->decoder.reset();
          const bool admitted = transport::send_proxy_fragment({poll.bytes.data(), poll.size}, poll.epoch);
          if (!admitted)
            fail_stream("injected display write failed");
          else {
            lock();
            const auto family = telemetry_family(poll.key.bytes[0]);
            if (family >= 0)
              ++telemetry_counters.injected[family];
            unlock();
          }
          have_poll = false;
          worked = true;
        }
      }
      else {
        lock();
        const bool reserved = arbiter.reserved();
        unlock();
        if (!reserved && xQueueSend(polls, &poll, 0) == pdTRUE)
          have_poll = false;
      }
    }
    lock();
    const bool active = lease.connected || handoff;
    const bool report = lease.connected && now >= next_telemetry_log;
    const auto counters = telemetry_counters;
    bike::ProxyReplyKey setup_key;
    setup_key.bytes[0] = 47;
    setup_key.size = 1;
    const auto owed47 = arbiter.phone_replies(setup_key);
    const auto input_bytes = relay->ingress.size(), rx_bytes = relay->upstream.size(),
               notify_bytes = relay->outgoing.size();
    unlock();
    if (report) {
      const auto report_now = uint64_t(esp_timer_get_time() / 1000);
      const auto ui_age = [&](size_t family) {
        return counters.ui_ms[family]
                   ? unsigned(report_now >= counters.ui_ms[family] ? report_now - counters.ui_ms[family] : 0)
                   : UINT32_MAX;
      };
      ESP_LOGI("ble_proxy",
               "Relay telemetry: 47 queued=%u tx=%u owned=%u ui=%u age_ms=%u passive=%u owed=%u; "
               "36 queued=%u tx=%u owned=%u ui=%u age_ms=%u; buffers=%u/%u/%u bytes",
               unsigned(counters.queued[0]), unsigned(counters.injected[0]), unsigned(counters.owned[0]),
               unsigned(counters.delivered[0]), ui_age(0), unsigned(counters.passive47), unsigned(owed47),
               unsigned(counters.queued[1]), unsigned(counters.injected[1]), unsigned(counters.owned[1]),
               unsigned(counters.delivered[1]), ui_age(1), unsigned(input_bytes), unsigned(rx_bytes),
               unsigned(notify_bytes));
      next_telemetry_log = now + 5000;
    }
    vTaskDelay(pdMS_TO_TICKS(worked || active ? 1 : 20));
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
  static StaticQueue_t polls_control, display_control;
  relay = make_external_state<RelayBuffers>();
  relay->reset(lease.generation, lease.upstream_session);
  polls = create_packet_queue<Poll>(4, polls_control);
  display_replies = create_packet_queue<DisplayReply>(4, display_control);
  display_input = make_external_state<OwnedReceive>();
  poll_queue_credits = make_external_state<bike::ProxyPollQueueCredits>();
  configASSERT(mutex && polls && display_replies);
  ble_svc_gap_init();
  ble_svc_gatt_init();
  const auto effective_name = runtime_name();
  configASSERT(!ble_svc_gap_device_name_set(effective_name.c_str()));
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
  source_continuity = accepting = false;
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
  const bool changed = lease.upstream_ready != ready || lease.upstream_session != session;
  if (changed)
    source_continuity = accepting = false;
  const bool abort_handoff = handoff && changed;
  const bool disconnect = lease.set_upstream(ready, session);
  unlock();
  if (abort_handoff)
    release_phone(false);
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
  lock();
  const bool normal = lease.accepts_display(lease.generation, session);
  const bool draining = handoff && relay->epoch == lease.generation && relay->session == session;
  const bool wanted = source == selected_source && (normal || draining) && relay->epoch == lease.generation &&
                      relay->session == session;
  const bool appended = wanted && relay->upstream.append(bytes, relay->epoch);
  if (appended)
    relay->last_rx = esp_timer_get_time() / 1000;
  unlock();
  if (wanted && !appended)
    fail_stream("upstream byte buffer exceeded");
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

void set_wfb_name_override(bool enabled) {
  lock();
  wfb_name_override = enabled;
  unlock();
}

bool name_locked() {
  lock();
  const bool locked = wfb_name_override;
  unlock();
  return locked;
}

std::string runtime_name() {
  lock();
  const std::string value(wfb_name_override ? "WFB" : proxy_name.data());
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
  const bool abort_handoff = paused && handoff;
  unlock();
  if (abort_handoff)
    release_phone(false);
  if (disconnect)
    disconnect_requested.store(true);
}

void set_advertising_hold(bool hold) {
  lock();
  advertising_hold = hold;
  unlock();
}

bool begin_maintenance() {
  lock();
  const bool reserved = !handoff && lease.begin_maintenance();
  unlock();
  return reserved;
}

Snapshot snapshot() {
  lock();
  Snapshot result{lease.enabled, lease.connected, lease.subscribed, advertising, lease.upstream_ready, failures};
  const auto handle = phone;
  result.ingress_high_water = std::max(last_high_water, uint32_t(relay->ingress.high_water()));
  result.ingress_bytes = lease.connected || handoff ? relay->ingress.pushed() : last_ingress_bytes;
  result.upstream_batches = lease.connected || handoff ? relay->batches : last_batches;
  unlock();
  result.authorized = result.connected && ble_security::proxy_authorized(handle);
  return result;
}

bool phone_owns_link() {
  lock();
  const bool connected = lease.connected || handoff;
  unlock();
  return connected;
}

void register_poll_key(bike::ProxyReplyKey key) {
  lock();
  arbiter.register_key(key);
  unlock();
}

bool queue_display_poll(std::span<const uint8_t> packet, bike::ProxyReplyKey key, uint32_t token) {
  if (packet.empty() || packet.size() > Poll{}.bytes.size())
    return false;
  Poll poll;
  lock();
  poll.epoch = lease.generation;
  poll.session = lease.upstream_session;
  const bool valid = lease.accepts_display(poll.epoch, poll.session) && arbiter.register_key(key);
  unlock();
  if (!valid)
    return false;
  lock();
  const bool unique = poll_queue_credits->claim(key, poll.epoch);
  unlock();
  if (!unique)
    return false;
  poll.key = key;
  poll.token = token;
  poll.size = packet.size();
  std::copy(packet.begin(), packet.end(), poll.bytes.begin());
  if (xQueueSend(polls, &poll, 0) == pdTRUE) {
    lock();
    const auto family = telemetry_family(key.bytes[0]);
    if (family >= 0)
      ++telemetry_counters.queued[family];
    unlock();
    return true;
  }
  lock();
  poll_queue_credits->release(key, poll.epoch);
  unlock();
  return false;
}

bool receive_display_reply(DisplayReply &reply) {
  return xQueueReceive(display_replies, &reply, 0) == pdTRUE;
}

void note_display_receive(uint8_t command, bool passive) {
  const auto family = telemetry_family(command);
  if (family < 0)
    return;
  lock();
  ++telemetry_counters.delivered[family];
  telemetry_counters.ui_ms[family] = esp_timer_get_time() / 1000;
  if (passive && command == 47)
    ++telemetry_counters.passive47;
  unlock();
}

} // namespace ble_proxy
