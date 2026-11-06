#include "transport.hpp"
#include "bike/controller_discovery.hpp"
#include "ble.hpp"
#include "ble_proxy.hpp"
#include "esp_timer.h"
#include "external_state.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "wired_transport.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>

namespace transport
{
namespace
{

struct BleReceive {
  std::array<Packet, 3> packets{};
  size_t head = 0, count = 0;
  uint32_t session = 0;
  bool gap = false;
  bike::Decoder decoder{[](void *context, std::span<const uint8_t> payload) {
                          auto &self = *static_cast<BleReceive *>(context);
                          if (self.count == self.packets.size()) {
                            self.head = self.count = 0;
                            self.gap = true;
                            return;
                          }
                          auto &packet = self.packets[(self.head + self.count++) % self.packets.size()];
                          packet.session = self.session;
                          packet.size = payload.size();
                          packet.discontinuity = false;
                          std::copy(payload.begin(), payload.end(), packet.bytes.begin());
                        },
                        this};

  void reset(uint32_t next) {
    session = next;
    head = count = 0;
    gap = false;
    decoder.reset();
  }
};

class BleTransport final : public Interface {
  ExternalState<BleReceive> input;

public:

  void activate(bike::IntegrationId) override {
    ble::set_paused(false);
  }

  void deactivate() override {
    ble::set_paused(true);
    if (input)
      input->reset(0);
  }

  void pause(bool paused) override {
    ble::set_paused(paused);
    if (input)
      input->reset(0);
  }

  Snapshot snapshot() override {
    const auto source = ble::snapshot();
    Snapshot result;
    result.status = source.status;
    result.session = source.session;
    result.integration = source.integration;
    result.peer_identity = source.peer_identity;
    std::copy(std::begin(source.connected_name), std::end(source.connected_name), result.connected_name);
    result.count = source.count;
    for (int i = 0; i < source.count; ++i) {
      std::copy(std::begin(source.devices[i].name), std::end(source.devices[i].name), result.devices[i].name);
      std::copy(std::begin(source.devices[i].address), std::end(source.devices[i].address), result.devices[i].address);
      result.devices[i].rssi = source.devices[i].rssi;
    }
    return result;
  }

  bool send(std::span<const uint8_t> payload) override {
    std::array<uint8_t, bike::max_payload + 6> bytes{};
    const auto size = bike::frame(payload, bytes);
    return size && ble::send({bytes.data(), size});
  }

  bool send_to(std::span<const uint8_t> payload, int controller) override {
    if (controller < 0)
      return send(payload);
    if (controller > 254 || payload.size() > bike::max_payload - 2)
      return false;
    std::array<uint8_t, bike::max_payload> forwarded{};
    forwarded[0] = 34;
    forwarded[1] = controller;
    std::copy(payload.begin(), payload.end(), forwarded.begin() + 2);
    return send({forwarded.data(), payload.size() + 2});
  }

  bool receive(Packet &packet) override {
    if (!input)
      input = make_external_state<BleReceive>();
    const auto link = ble::snapshot();
    if (input->session != link.session)
      input->reset(link.session);
    if (input->count) {
      packet = input->packets[input->head];
      input->head = (input->head + 1) % input->packets.size();
      --input->count;
      return true;
    }
    ble::Received bytes;
    for (unsigned reads = 0; reads < 8 && ble::receive(bytes); ++reads) {
      if (bytes.session != link.session)
        continue;
      if (bytes.discontinuity) {
        input->reset(link.session);
        input->gap = true;
      }
      else
        input->decoder.feed({bytes.bytes.data(), bytes.size}, esp_timer_get_time() / 1000);
      if (input->gap) {
        input->reset(link.session);
        packet = {};
        packet.session = link.session;
        packet.discontinuity = true;
        return true;
      }
      if (input->count) {
        packet = input->packets[input->head];
        input->head = (input->head + 1) % input->packets.size();
        --input->count;
        return true;
      }
    }
    return false;
  }
};

SemaphoreHandle_t mutex;
BleTransport bluetooth;
Interface *active = &bluetooth;
Kind kind = Kind::Ble;
std::atomic<Kind> proxy_source{Kind::Ble};
bool paused = false;
uint32_t source_session = 0, epoch = 1;

std::optional<bike::ControllerHint> load_hint(const bike::HintContext &context) {
  nvs_handle_t handle;
  if (nvs_open("route_hint", NVS_READONLY, &handle) != ESP_OK)
    return std::nullopt;
  std::optional<bike::ControllerHint> result;
  for (unsigned slot = 0; slot < bike::controller_hint_slots; ++slot) {
    char key[8];
    std::snprintf(key, sizeof(key), "h%u", slot);
    bike::HintBytes bytes{};
    size_t size = bytes.size();
    if (nvs_get_blob(handle, key, bytes.data(), &size) != ESP_OK)
      continue;
    const auto record = bike::decode_hint(std::span(bytes).first(size));
    if (record && record->context == context) {
      result = record->controller;
      break;
    }
  }
  nvs_close(handle);
  return result;
}

void save_hint(const bike::HintRecord &record) {
  if (!bike::valid_hint(record))
    return;
  nvs_handle_t handle;
  if (nvs_open("route_hint", NVS_READWRITE, &handle) != ESP_OK)
    return;
  int match = -1, empty = -1;
  for (unsigned slot = 0; slot < bike::controller_hint_slots; ++slot) {
    char key[8];
    std::snprintf(key, sizeof(key), "h%u", slot);
    bike::HintBytes bytes{};
    size_t size = bytes.size();
    std::optional<bike::HintRecord> saved;
    if (nvs_get_blob(handle, key, bytes.data(), &size) == ESP_OK)
      saved = bike::decode_hint(std::span(bytes).first(size));
    if (!saved) {
      if (empty < 0)
        empty = int(slot);
      continue;
    }
    if (saved->context == record.context) {
      if (saved->controller == record.controller) {
        nvs_close(handle);
        return; // Unchanged verified hints need no flash write.
      }
      match = int(slot);
      break;
    }
  }
  uint8_t next = 0;
  nvs_get_u8(handle, "next", &next);
  const unsigned slot = match >= 0   ? unsigned(match)
                        : empty >= 0 ? unsigned(empty)
                                     : next % bike::controller_hint_slots;
  char key[8];
  std::snprintf(key, sizeof(key), "h%u", slot);
  const auto bytes = bike::encode_hint(record);
  auto error = nvs_set_blob(handle, key, bytes.data(), bytes.size());
  if (error == ESP_OK)
    error = nvs_set_u8(handle, "next", uint8_t((slot + 1) % bike::controller_hint_slots));
  if (error == ESP_OK)
    nvs_commit(handle);
  nvs_close(handle);
}

struct Route {
  bike::ControllerDiscovery discovery;
  bool started = false, ready = false, hint_tried = false;
  bike::HintContext context;
  int destination = -1;
  uint64_t settle_until = 0, retry_at = 0;
};

ExternalState<Route> route;

void reset_route() {
  route->started = route->ready = route->hint_tried = false;
  route->settle_until = route->retry_at = 0;
}

Snapshot snapshot_locked() {
  auto result = active->snapshot();
  result.selection = result.integration;
  result.integration = bike::IntegrationId::Vesc; // Custom features require a verified package reply.
  if (result.session != source_session) {
    source_session = result.session;
    ++epoch;
    reset_route();
  }
  const auto now = uint64_t(esp_timer_get_time() / 1000);
  result.link_present = result.status == Status::Ready || (kind == Kind::Can && result.status == Status::Discovering);
  if (result.link_present) {
    if (!route->started || (route->discovery.complete() && route->discovery.chosen() < 0 && now >= route->retry_at)) {
      route->context = {uint8_t(kind), result.selection, result.peer_identity};
      const auto hint = route->hint_tried ? std::nullopt : load_hint(route->context);
      route->hint_tried = true;
      route->discovery.start(kind == Kind::Can, result.selection, now, hint);
      route->started = true;
      route->ready = false;
      route->settle_until = 0;
      route->retry_at = now + 10000;
    }
    if (!route->ready) {
      result.status = Status::Discovering;
      std::snprintf(result.controller_info, sizeof(result.controller_info), "%s",
                    ble_proxy::phone_owns_link() ? "Phone owns link / display discovery waits"
                    : route->discovery.running() ? "Finding main VESC / CAN ping and package discovery"
                                                 : "No matching package controller found / retrying");
    }
    else {
      const auto &controller = route->discovery.controller(route->discovery.chosen());
      result.integration = controller.integration;
      result.controller_id = controller.can_id;
      std::snprintf(result.controller_info, sizeof(result.controller_info), "Direct main VESC / %s / %u.%u",
                    controller.hardware, unsigned(controller.major), unsigned(controller.minor));
      if (route->destination >= 0)
        std::snprintf(result.controller_info, sizeof(result.controller_info), "Main VESC CAN %d / %s / %u.%u",
                      route->destination, controller.hardware, unsigned(controller.major), unsigned(controller.minor));
    }
  }
  result.session = epoch;
  return result;
}

void pump_discovery() {
  if (!route->started || route->ready || ble_proxy::phone_owns_link())
    return;
  const auto now = uint64_t(esp_timer_get_time() / 1000);
  const auto native_scan = kind == Kind::Can ? active->can_scan() : CanScan{};
  if (kind == Kind::Can && native_scan.complete && route->discovery.awaiting_scan())
    route->discovery.scanned({native_scan.ids.data(), native_scan.count}, native_scan.overflow, now);
  bike::DiscoveryRequest request;
  if (route->discovery.next(now, request)) {
    if (request.scan && kind == Kind::Can)
      active->scan_can();
    else
      active->send_to({request.bytes.data(), request.size}, request.scan ? -1 : request.route);
  }
  Packet packet;
  for (unsigned reads = 0; reads < 16 && active->receive(packet); ++reads)
    if (packet.session == source_session && !packet.discontinuity)
      route->discovery.feed({packet.bytes.data(), packet.size}, packet.controller, now);
  if (route->discovery.complete() && route->discovery.chosen() >= 0) {
    if (!route->settle_until)
      route->settle_until = now + 300;
    if (now >= route->settle_until) {
      const auto &controller = route->discovery.controller(route->discovery.chosen());
      route->destination = controller.route;
      save_hint({route->context, {controller.route, controller.uuid}});
      route->ready = true;
      ++epoch;
    }
  }
}

bool persist_kind(Kind value) {
  nvs_handle_t h;
  if (nvs_open("transport", NVS_READWRITE, &h) != ESP_OK)
    return false;
  auto error = nvs_set_u8(h, "kind", uint8_t(value));
  if (error == ESP_OK)
    error = nvs_commit(h);
  nvs_close(h);
  return error == ESP_OK;
}

bike::IntegrationId load_integration(Kind value) {
  if (value == Kind::Ble)
    return bike::IntegrationId::Auto;
  uint8_t id = uint8_t(bike::IntegrationId::Auto);
  nvs_handle_t h;
  if (nvs_open("transport", NVS_READONLY, &h) == ESP_OK) {
    nvs_get_u8(h, value == Kind::Can ? "can_integ" : "uart_integ", &id);
    nvs_close(h);
  }
  return bike::integration_id(id);
}

} // namespace

bool available(Kind value) {
  return value == Kind::Ble || wired_available(value);
}

const char *name(Kind value) {
  return value == Kind::Can ? "CAN" : value == Kind::Uart ? "UART" : "BLE";
}

Kind selected() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  const auto value = kind;
  xSemaphoreGive(mutex);
  return value;
}

void start() {
  mutex = xSemaphoreCreateMutex();
  configASSERT(mutex);
  route = make_external_state<Route>();
  ble::start();
  ble_proxy::select_upstream(Kind::Ble);
  uint8_t value = 0;
  nvs_handle_t h;
  if (nvs_open("transport", NVS_READONLY, &h) == ESP_OK) {
    nvs_get_u8(h, "kind", &value);
    nvs_close(h);
  }
  if (value <= 2 && available(Kind(value)) && value != 0) {
    kind = Kind(value);
    bluetooth.deactivate();
    active = &wired_backend(kind);
    active->activate(load_integration(kind));
    proxy_source.store(kind);
    ble_proxy::select_upstream(kind);
  }
}

bool select(Kind value) {
  if (!available(value) || ble_proxy::phone_owns_link())
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (paused || value == kind) {
    const bool same = value == kind;
    xSemaphoreGive(mutex);
    return same;
  }
  if (!ble_proxy::begin_maintenance()) {
    xSemaphoreGive(mutex);
    return false;
  }
  if (!persist_kind(value)) {
    ble_proxy::set_paused(kind != Kind::Ble);
    xSemaphoreGive(mutex);
    return false;
  }
  active->deactivate();
  kind = value;
  active = value == Kind::Ble ? static_cast<Interface *>(&bluetooth) : &wired_backend(value);
  ++epoch;
  source_session = 0;
  active->activate(load_integration(value));
  proxy_source.store(kind);
  ble_proxy::select_upstream(kind);
  xSemaphoreGive(mutex);
  return true;
}

Snapshot snapshot() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  auto value = snapshot_locked();
  xSemaphoreGive(mutex);
  return value;
}

bool send(std::span<const uint8_t> payload) {
  if (payload.empty() || payload.size() > bike::max_payload)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  const bool sent = !paused && route->ready && active->send_to(payload, route->destination);
  xSemaphoreGive(mutex);
  return sent;
}

bool receive(Packet &packet) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  snapshot_locked();
  pump_discovery();
  if (!route->ready) {
    xSemaphoreGive(mutex);
    return false;
  }
  bool received = false;
  while (!paused && active->receive(packet)) {
    if (packet.session != source_session)
      continue;
    if (kind == Kind::Can && packet.controller != route->destination)
      continue;
    packet.session = epoch;
    received = true;
    break;
  }
  xSemaphoreGive(mutex);
  return received;
}

void set_paused(bool value) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  paused = value;
  ble_proxy::set_paused(value);
  active->pause(value);
  ++epoch;
  source_session = 0;
  xSemaphoreGive(mutex);
}

void connect(std::string_view address, int integration) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (!paused && !ble_proxy::phone_owns_link()) {
    if (kind == Kind::Ble)
      ble::connect(address, integration);
    else {
      const auto id = bike::integration_id(integration);
      nvs_handle_t h;
      if (nvs_open("transport", NVS_READWRITE, &h) == ESP_OK) {
        auto error = nvs_set_u8(h, kind == Kind::Can ? "can_integ" : "uart_integ", uint8_t(id));
        if (error == ESP_OK)
          error = nvs_commit(h);
        nvs_close(h);
        if (error == ESP_OK) {
          active->deactivate();
          active->activate(id);
          ++epoch;
          source_session = 0;
        }
      }
    }
  }
  xSemaphoreGive(mutex);
}

void forget() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (kind == Kind::Ble)
    ble::forget();
  else
    active->deactivate();
  ++epoch;
  source_session = 0;
  xSemaphoreGive(mutex);
}

bool rediscover() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  const bool allowed = !paused && !ble_proxy::phone_owns_link();
  if (allowed) {
    reset_route();
    ++epoch;
  }
  xSemaphoreGive(mutex);
  return allowed;
}

void proxy_ownership(bool owned, uint32_t epoch) {
  ble::set_proxy_owner(owned, epoch);
  wired_proxy_ownership(owned, epoch);
}

bool send_proxy_fragment(std::span<const uint8_t> bytes, uint32_t epoch) {
  return proxy_source.load() == Kind::Ble ? ble::send_proxy_fragment(bytes, epoch)
                                          : wired_send_proxy_fragment(bytes, epoch);
}

void reset_proxy_link(Kind source) {
  if (source == Kind::Ble)
    ble::reset_link();
  else
    wired_reset_proxy_link();
}

} // namespace transport
