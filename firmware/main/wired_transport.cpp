#include "wired_transport.hpp"
#include "bike/can_bridge.hpp"
#include "bike/can_packet.hpp"
#include "bike/proxy_tx_drain.hpp"
#include "ble_proxy.hpp"
#include "can_driver.hpp"
#include "driver/uart.h"
#include "esp_timer.h"
#include "external_state.hpp"
#include "freertos/task.h"
#include "nvs.h"
#include "packet_queue.hpp"
#include "vesc_update.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>

#if defined(CAN_TX) != defined(CAN_RX)
  #error "CAN_TX and CAN_RX must be configured together"
#endif
#if defined(UART_TX) != defined(UART_RX)
  #error "UART_TX and UART_RX must be configured together"
#endif
#ifndef CAN_TX
  #define CAN_TX -1
  #define CAN_RX -1
#endif
#ifndef UART_TX
  #define UART_TX -1
  #define UART_RX -1
#endif
#ifndef CAN_BITRATE
  #define CAN_BITRATE 500000
#endif
#ifndef CAN_LOCAL_ID
  #define CAN_LOCAL_ID 254
#endif
#ifndef UART_BAUD
  #define UART_BAUD 115200
#endif
#ifndef BIKE_UART_PORT
  #define BIKE_UART_PORT 1
#endif

namespace transport
{
namespace
{

static_assert(CAN_TX < 0 || CAN_TX != CAN_RX);

static_assert(UART_TX < 0 || UART_TX != UART_RX);

static_assert(CAN_LOCAL_ID >= 0 && CAN_LOCAL_ID < 255);

static_assert(BIKE_UART_PORT == 1 || BIKE_UART_PORT == 2, "Vehicle UART must not use the console UART");

static_assert(UART_BAUD >= 9600 && UART_BAUD <= 1000000);

constexpr uart_port_t uart_port = uart_port_t(BIKE_UART_PORT);

uint64_t now_ms() {
  return esp_timer_get_time() / 1000;
}

StaticQueue_t tx_control, rx_control;
std::atomic<uint32_t> proxy_requested{0}, proxy_active{0};
bike::ProxyTxDrain proxy_tx;
std::atomic<bool> proxy_reset{false};

struct Runtime {
  SemaphoreHandle_t mutex = nullptr;
  QueueHandle_t tx = nullptr, rx = nullptr;
  Kind kind = Kind::Can;
  bool enabled = false, paused = false;
  uint8_t node = 0;
  uint32_t generation = 1;
  Snapshot state;
  std::atomic<bool> gap{false};
  Packet outgoing, incoming;
  std::array<uint8_t, bike::max_payload + 6> serial_bytes{};
  bike::CanReassembler can, endpoint;
  uint32_t processing_generation = 0;
  uint64_t last_rx = 0;
  CanScan scan;
  uint32_t scan_generation = 0;
  uint32_t proxy_processing_epoch = 0, proxy_scan_epoch = 0;
  bool proxy_failed = false;
  std::array<uint8_t, bike::max_payload> proxy_reply_payload{};
  bike::Decoder phone_decoder{
      [](void *context, std::span<const uint8_t> payload) { static_cast<Runtime *>(context)->phone_command(payload); },
      this};
  bike::Decoder decoder{
      [](void *context, std::span<const uint8_t> payload) { static_cast<Runtime *>(context)->accept(payload); }, this};

  void lock() {
    xSemaphoreTake(mutex, portMAX_DELAY);
  }

  void unlock() {
    xSemaphoreGive(mutex);
  }

  bool current(uint32_t generation_value) {
    lock();
    const bool valid = enabled && !paused && generation == generation_value;
    unlock();
    return valid;
  }

  void proxy_reply(std::span<const uint8_t> payload) {
    const auto size = bike::frame(payload, serial_bytes);
    if (!size) {
      proxy_failed = true;
      return;
    }
    for (size_t offset = 0; offset < size; offset += 512)
      ble_proxy::upstream(kind, {serial_bytes.data() + offset, std::min<size_t>(512, size - offset)},
                          processing_generation);
  }

  void phone_command(std::span<const uint8_t> payload);

  void accept(std::span<const uint8_t> payload, int source = -2) {
    lock();
    if (!enabled || paused || generation != processing_generation || payload.empty() ||
        payload.size() > bike::max_payload ||
        (state.status != Status::Ready && (payload[0] != 0 || payload.size() < 3))) {
      unlock();
      return;
    }
    state.status = Status::Ready;
    last_rx = now_ms();
    incoming.session = generation;
    incoming.size = payload.size();
    incoming.discontinuity = false;
    incoming.controller = source;
    std::copy(payload.begin(), payload.end(), incoming.bytes.begin());
    unlock();
    if (xQueueSend(rx, &incoming, 0) != pdTRUE) {
      xQueueReset(rx);
      gap.store(true);
    }
  }
};

ExternalState<Runtime> runtime;

CanDriver can_driver;

static_assert(CAN_BITRATE == 125000 || CAN_BITRATE == 250000 || CAN_BITRATE == 500000 || CAN_BITRATE == 1000000);

esp_err_t open_driver(Kind kind) {
  if (kind == Kind::Can) {
    return can_driver.open(CAN_TX, CAN_RX, CAN_BITRATE);
  }
  uart_config_t config{};
  config.baud_rate = UART_BAUD;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;
  auto result = uart_param_config(uart_port, &config);
  if (result == ESP_OK)
    result = uart_set_pin(uart_port, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (result == ESP_OK)
    result = uart_driver_install(uart_port, 2048, 0, 0, nullptr, 0);
  if (result == ESP_OK)
    uart_flush_input(uart_port);
  return result;
}

void close_driver(Kind kind) {
  if (kind == Kind::Can) {
    can_driver.close();
  }
  else
    uart_driver_delete(uart_port);
}

bool write_serial(Runtime &r, uint32_t generation, std::span<const uint8_t> bytes) {
  // Use nonblocking FIFO writes so a switch/OTA pause can cancel an unfinished
  // serial packet without waiting behind a full maximum-size UART transfer.
  size_t sent = 0;
  const auto deadline = now_ms() + 2000;
  while (sent < bytes.size() && r.current(generation) && now_ms() < deadline) {
    const int count =
        uart_tx_chars(uart_port, reinterpret_cast<const char *>(bytes.data() + sent), bytes.size() - sent);
    if (count < 0)
      return false;
    sent += count;
    if (sent < bytes.size())
      vTaskDelay(1);
  }
  return !bytes.empty() && sent == bytes.size() && uart_wait_tx_done(uart_port, pdMS_TO_TICKS(200)) == ESP_OK;
}

bool transmit(Runtime &r, Kind kind, uint8_t node, uint32_t generation, std::span<const uint8_t> payload) {
  if (!r.current(generation))
    return false;
  if (kind == Kind::Can) {
    return bike::send_can_packet(payload, CAN_LOCAL_ID, node, false, [&](const bike::CanFrame &f) {
      if (!r.current(generation))
        return false;
      return can_driver.send(f);
    });
  }
  const auto size = bike::frame(payload, r.serial_bytes);
  return size && write_serial(r, generation, {r.serial_bytes.data(), size});
}

void Runtime::phone_command(std::span<const uint8_t> payload) {
  // Only the native CAN worker feeds this decoder. BLE/UART relay the actual
  // upstream byte stream and must never acquire a local adapter identity.
  lock();
  const bool can_adapter = kind == Kind::Can && enabled && !paused && generation == processing_generation;
  unlock();
  if (!can_adapter)
    return;
  if (vesc_update::handles(payload)) {
    vesc_update::submit({false, proxy_processing_epoch, processing_generation, 0}, payload);
    return;
  }
  const auto command = bike::can_bridge_request(payload, CAN_LOCAL_ID);
  if (command.operation == bike::BridgeOperation::Scan) {
    lock();
    scan = {};
    ++scan_generation;
    proxy_scan_epoch = proxy_processing_epoch;
    unlock();
  }
  else if (command.operation == bike::BridgeOperation::Forward) {
    if (vesc_update::busy())
      return;
    if (!transmit(*this, Kind::Can, command.destination, processing_generation, command.payload))
      proxy_failed = true;
  }
  else if (command.operation == bike::BridgeOperation::RawFrame) {
    if (vesc_update::busy())
      return;
    if (!current(processing_generation) || !can_driver.send(command.frame))
      proxy_failed = true;
  }
  else if (command.operation == bike::BridgeOperation::Ignore) {
    // COMM_ALIVE has no local watchdog or reply on this adapter.
  }
  else {
    const std::string_view text = command.operation == bike::BridgeOperation::Invalid
                                      ? "Invalid CAN adapter request"
                                      : "Unsupported CAN adapter command; select a CAN controller in the app";
    proxy_reply_payload[0] = 21; // COMM_PRINT: adapter has no implicit motor controller.
    std::copy(text.begin(), text.end(), proxy_reply_payload.begin() + 1);
    proxy_reply({proxy_reply_payload.data(), text.size() + 1});
  }
}

void worker(void *context) {
  auto &r = *static_cast<Runtime *>(context);
  bool open = false;
  Kind opened = Kind::Can;
  uint32_t generation = 0;
  uint64_t probe_at = 0;
  uint32_t scan_generation = 0;
  unsigned ping_id = 255;
  uint64_t last_ping = 0;
  std::array<uint8_t, 128> bytes{};
  for (;;) {
    if (proxy_reset.exchange(false)) {
      r.lock();
      ++r.generation;
      r.state.session = r.generation;
      r.state.status = r.paused ? Status::Paused : r.enabled ? Status::Connecting : Status::Disconnected;
      r.unlock();
    }
    const auto requested_owner = proxy_requested.load();
    if (requested_owner != r.proxy_processing_epoch) {
      r.proxy_processing_epoch = requested_owner;
      r.proxy_scan_epoch = 0;
      r.phone_decoder.reset();
      r.decoder.reset();
      r.can.reset();
      r.endpoint.reset();
    }
    proxy_active.store(requested_owner);
    r.lock();
    const auto next_generation = r.generation;
    const auto kind = r.kind;
    const auto node = r.node;
    const bool wanted = r.enabled && !r.paused;
    const auto status = r.state.status;
    const auto requested_scan = r.scan_generation;
    r.unlock();
    if (next_generation != generation) {
      if (open)
        close_driver(opened);
      open = false;
      ping_id = 255;
      last_ping = 0;
      generation = next_generation;
      probe_at = 0;
      r.processing_generation = generation;
      r.decoder.reset();
      r.can.reset();
      r.endpoint.reset();
      if (wanted) {
        open = open_driver(kind) == ESP_OK;
        opened = kind;
        if (open && kind == Kind::Can) {
          r.lock();
          if (r.generation == generation) {
            r.scan = {};
            r.state.status = Status::Discovering;
          }
          r.unlock();
        }
        if (!open) {
          r.lock();
          if (r.generation == generation)
            r.state.status = Status::Error;
          r.unlock();
        }
      }
    }
    ble_proxy::upstream_state(kind, wanted && open, generation);
    if (!wanted || !open) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (requested_scan != scan_generation) {
      scan_generation = requested_scan;
      ping_id = 0;
      last_ping = 0;
    }
    bool failed = false;
    if (kind == Kind::Can) {
      bike::CanFrame f;
      for (unsigned reads = 0; reads < 64 && can_driver.receive(f); ++reads) {
        if (f.extended && !f.remote && f.id == ((17u << 8) | CAN_LOCAL_ID) && f.size >= 1 && f.data[0] != 255 &&
            f.data[0] != CAN_LOCAL_ID) {
          bike::CanFrame pong;
          pong.id = (18u << 8) | f.data[0];
          pong.size = 2;
          pong.data[0] = CAN_LOCAL_ID;
          pong.data[1] = 2;
          can_driver.send(pong);
        }
        const auto request = r.endpoint.feed(f, CAN_LOCAL_ID, -1, now_ms(), false);
        if (!request.empty() && r.endpoint.sender() >= 0 && r.endpoint.sender() != CAN_LOCAL_ID &&
            r.endpoint.sender() != 255 && vesc_update::handles(request))
          vesc_update::submit({true, 0, generation, uint8_t(r.endpoint.sender())}, request);
        if (f.extended && !f.remote && f.id == ((18u << 8) | CAN_LOCAL_ID) && f.size >= 1 && f.data[0] != 255 &&
            f.data[0] != CAN_LOCAL_ID) {
          r.lock();
          const auto end = r.scan.ids.begin() + r.scan.count;
          if (std::find(r.scan.ids.begin(), end, f.data[0]) == end) {
            if (r.scan.count < r.scan.ids.size())
              r.scan.ids[r.scan.count++] = f.data[0];
            else
              r.scan.overflow = true;
          }
          r.unlock();
        }
        const auto payload = r.can.feed(f, CAN_LOCAL_ID, requested_owner ? -1 : int(node), now_ms());
        if (!payload.empty()) {
          if (requested_owner)
            r.proxy_reply(payload);
          r.accept(payload, r.can.sender());
        }
      }
      failed = !can_driver.healthy();
      if (!failed && ping_id < 255) {
        if (ping_id == CAN_LOCAL_ID)
          ++ping_id;
        if (ping_id < 255) {
          bike::CanFrame ping;
          ping.id = (17u << 8) | ping_id++;
          ping.size = 1;
          ping.data[0] = CAN_LOCAL_ID;
          failed = !can_driver.send(ping);
          last_ping = now_ms();
        }
      }
      if (ping_id >= 255 && last_ping && now_ms() - last_ping > 100) {
        r.lock();
        r.scan.complete = true;
        r.unlock();
      }
    }
    else {
      for (unsigned reads = 0; reads < 16; ++reads) {
        const int count = uart_read_bytes(uart_port, bytes.data(), bytes.size(), 0);
        if (count < 0) {
          failed = true;
          break;
        }
        if (!count)
          break;
        if (requested_owner)
          ble_proxy::upstream(Kind::Uart, {bytes.data(), size_t(count)}, generation);
        r.decoder.feed({bytes.data(), size_t(count)}, now_ms());
      }
    }
    if (!failed && xQueueReceive(r.tx, &r.outgoing, 0) == pdTRUE) {
      bike::ProxyTxDrain::WriteGuard writing(proxy_tx, r.outgoing.proxy_epoch);
      if (r.outgoing.session != generation) {
        // Stale requests still relinquish their queued count through the guard.
      }
      else if (r.outgoing.can_reply && kind == Kind::Can) {
        failed = !bike::send_can_packet(
            std::span(r.outgoing.bytes).first(r.outgoing.size), CAN_LOCAL_ID, uint8_t(r.outgoing.controller), true,
            [&](const bike::CanFrame &frame) { return r.current(generation) && can_driver.send(frame); });
      }
      else if (r.outgoing.proxy_epoch) {
        if (r.outgoing.proxy_epoch == requested_owner && proxy_requested.load() == requested_owner) {
          if (kind == Kind::Can) {
            const auto rejected = r.phone_decoder.rejected();
            r.phone_decoder.feed({r.outgoing.bytes.data(), r.outgoing.size}, now_ms());
            failed = r.proxy_failed || r.phone_decoder.rejected() != rejected;
          }
          else
            failed = !write_serial(r, generation, {r.outgoing.bytes.data(), r.outgoing.size});
        }
      }
      else if (!requested_owner) {
        if (kind == Kind::Can) {
          r.lock();
          r.node = r.outgoing.controller;
          r.unlock();
        }
        failed = !transmit(r, kind, kind == Kind::Can ? uint8_t(r.outgoing.controller) : node, generation,
                           {r.outgoing.bytes.data(), r.outgoing.size});
      }
    }
    if (!failed && !requested_owner && kind == Kind::Uart && status != Status::Ready && now_ms() >= probe_at) {
      const uint8_t firmware[] = {0};
      failed = !transmit(r, kind, node, generation, firmware);
      probe_at = now_ms() + 1000;
    }
    if (requested_owner && kind == Kind::Can && r.proxy_scan_epoch == requested_owner) {
      r.lock();
      const auto scan = r.scan;
      r.unlock();
      if (scan.complete) {
        r.proxy_reply_payload[0] = 62;
        std::copy_n(scan.ids.begin(), scan.count, r.proxy_reply_payload.begin() + 1);
        r.proxy_reply({r.proxy_reply_payload.data(), scan.count + 1});
        r.proxy_scan_epoch = 0;
      }
    }
    if (r.proxy_failed) {
      failed = true;
      r.proxy_failed = false;
    }
    if (failed && requested_owner)
      ble_proxy::upstream_failed(kind);
    r.lock();
    if (r.generation == generation && r.enabled && !r.paused &&
        (failed || (!vesc_update::busy() && !ble_proxy::update_mode() && !requested_owner &&
                    r.state.status == Status::Ready && now_ms() - r.last_rx >= 3000))) {
      ++r.generation;
      r.state.session = r.generation;
      r.state.status = Status::Retry;
    }
    r.unlock();
    vTaskDelay(pdMS_TO_TICKS(failed ? 100 : 2));
  }
}

Runtime &get_runtime() {
  if (!runtime) {
    runtime = make_external_state<Runtime>();
    runtime->mutex = xSemaphoreCreateMutex();
    runtime->tx = create_packet_queue<Packet>(4, tx_control);
    runtime->rx = create_packet_queue<Packet>(6, rx_control);
    configASSERT(runtime->mutex && runtime->tx && runtime->rx);
    configASSERT(xTaskCreate(worker, "bike_wired", 4096, runtime.get(), 5, nullptr) == pdPASS);
  }
  return *runtime;
}

class WiredTransport final : public Interface {
  Kind kind;

public:

  explicit WiredTransport(Kind value) : kind(value) {
  }

  void activate(bike::IntegrationId integration) override {
    auto &r = get_runtime();
    r.lock();
    r.kind = kind;
    r.node = 0;
    r.enabled = true;
    r.paused = false;
    ++r.generation;
    r.state = {};
    r.state.session = r.generation;
    r.state.status = Status::Connecting;
    r.state.integration = integration;
    const char *brand = integration == bike::IntegrationId::Wfb      ? "WFB"
                        : integration == bike::IntegrationId::WheelE ? "wheel-e"
                        : integration == bike::IntegrationId::Auto   ? "Auto"
                                                                     : "VESC";
    std::snprintf(r.state.connected_name, sizeof(r.state.connected_name), "%s %s bridge", brand, name(kind));
    r.unlock();
  }

  void deactivate() override {
    if (!runtime)
      return;
    auto &r = *runtime;
    r.lock();
    r.enabled = false;
    ++r.generation;
    r.state.session = r.generation;
    r.state.status = Status::Disconnected;
    r.unlock();
  }

  void pause(bool paused) override {
    if (!runtime)
      return;
    auto &r = *runtime;
    r.lock();
    r.paused = paused;
    ++r.generation;
    r.state.session = r.generation;
    r.state.status = paused ? Status::Paused : r.enabled ? Status::Connecting : Status::Disconnected;
    r.unlock();
  }

  Snapshot snapshot() override {
    if (!runtime)
      return {};
    runtime->lock();
    const auto result = runtime->state;
    runtime->unlock();
    return result;
  }

  bool send(std::span<const uint8_t> payload) override {
    if (kind != Kind::Can)
      return send_to(payload, -1);
    runtime->lock();
    const auto node = runtime->node;
    runtime->unlock();
    return send_to(payload, node);
  }

  bool send_to(std::span<const uint8_t> payload, int controller_id) override {
    auto &r = *runtime;
    r.lock();
    // Keep ordinary writes outside the retained phone epoch, including the
    // backend-drain/quiet handoff after the phone itself has disconnected.
    const bool ready = r.enabled && !r.paused && r.state.status != Status::Error && proxy_requested.load() == 0;
    const auto generation = r.generation;
    r.unlock();
    if (!ready || payload.empty() || payload.size() > bike::max_payload)
      return false;
    Packet packet;
    packet.session = generation;
    packet.size = payload.size();
    if (kind == Kind::Can) {
      if (controller_id < 0 || controller_id > 254 || controller_id == CAN_LOCAL_ID)
        return false;
      packet.controller = controller_id;
      std::copy(payload.begin(), payload.end(), packet.bytes.begin());
    }
    else if (controller_id >= 0) {
      if (controller_id > 254 || payload.size() > bike::max_payload - 2)
        return false;
      packet.bytes[0] = 34;
      packet.bytes[1] = controller_id;
      packet.size += 2;
      std::copy(payload.begin(), payload.end(), packet.bytes.begin() + 2);
    }
    else
      std::copy(payload.begin(), payload.end(), packet.bytes.begin());
    return xQueueSend(r.tx, &packet, 0) == pdTRUE;
  }

  void scan_can() override {
    if (kind != Kind::Can || !runtime)
      return;
    runtime->lock();
    runtime->scan = {};
    ++runtime->scan_generation;
    runtime->unlock();
  }

  CanScan can_scan() override {
    if (kind != Kind::Can || !runtime)
      return {};
    runtime->lock();
    auto scan = runtime->scan;
    runtime->unlock();
    return scan;
  }

  bool receive(Packet &packet) override {
    if (!runtime)
      return false;
    if (runtime->gap.exchange(false)) {
      packet = {};
      packet.session = snapshot().session;
      packet.discontinuity = true;
      return true;
    }
    return xQueueReceive(runtime->rx, &packet, 0) == pdTRUE;
  }
};

WiredTransport can{Kind::Can}, uart{Kind::Uart};

} // namespace

bool wired_available(Kind kind) {
  return kind == Kind::Can ? CAN_TX >= 0 && CAN_RX >= 0 : kind == Kind::Uart && UART_TX >= 0 && UART_RX >= 0;
}

Interface &wired_backend(Kind kind) {
  return kind == Kind::Can ? static_cast<Interface &>(can) : uart;
}

bool wired_update_valid(uint32_t session) {
  if (!runtime)
    return false;
  runtime->lock();
  const bool valid =
      runtime->kind == Kind::Can && runtime->enabled && !runtime->paused && runtime->generation == session;
  runtime->unlock();
  return valid;
}

void wired_update_reply(uint8_t peer, uint32_t session, std::span<const uint8_t> payload) {
  if (!wired_update_valid(session) || peer == 255 || peer == CAN_LOCAL_ID || payload.size() > bike::max_payload)
    return;
  Packet packet;
  packet.session = session;
  packet.controller = peer;
  packet.can_reply = true;
  packet.size = payload.size();
  std::copy(payload.begin(), payload.end(), packet.bytes.begin());
  xQueueSend(runtime->tx, &packet, 0);
}

void wired_proxy_ownership(bool owned, uint32_t epoch) {
  proxy_requested.store(owned ? epoch : 0);
}

bool wired_send_proxy_fragment(std::span<const uint8_t> bytes, uint32_t epoch) {
  if (!runtime || !epoch || proxy_requested.load() != epoch || proxy_active.load() != epoch || bytes.empty() ||
      bytes.size() > 512)
    return false;
  auto &r = *runtime;
  r.lock();
  const bool ready = r.enabled && !r.paused;
  const auto generation = r.generation;
  r.unlock();
  if (!ready)
    return false;
  Packet packet;
  packet.session = generation;
  packet.proxy_epoch = epoch;
  packet.size = bytes.size();
  std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
  proxy_tx.reserve();
  if (xQueueSend(r.tx, &packet, 0) == pdTRUE)
    return true;
  proxy_tx.rejected();
  return false;
}

bool wired_proxy_drained(uint32_t epoch) {
  if (!runtime || !epoch || !proxy_tx.drained() || proxy_requested.load() != epoch || proxy_active.load() != epoch)
    return false;
  runtime->lock();
  // A CAN/UART adapter can relay phone traffic before display discovery marks
  // a main controller Ready. Match the raw-proxy admission gate, not that UI state.
  const bool ready = runtime->enabled && !runtime->paused;
  runtime->unlock();
  return ready;
}

void wired_reset_proxy_link() {
  proxy_reset.store(true);
}

} // namespace transport
