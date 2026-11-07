#include "vesc_update.hpp"
#include "bike/can_bridge.hpp"
#include "bike/vesc_upload.hpp"
#include "ble_proxy.hpp"
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "external_state.hpp"
#include "freertos/task.h"
#include "hardware_metadata.h"
#include "ota_image.hpp"
#include "packet_queue.hpp"
#include "wired_transport.hpp"
#include <atomic>

namespace vesc_update
{
namespace
{

struct Request {
  Owner owner;
  uint16_t size = 0;
  std::array<uint8_t, bike::max_payload> bytes{};
};

QueueHandle_t queue;
StaticQueue_t control;
std::atomic<bool> allowed{false}, continuing{false}, active{false}, cancelling{false};
std::atomic<bool> cancellable{false}, native_can{false};
std::atomic<int> progress{0};
std::atomic<const char *> message{"Ready for VESC Tool"};

bool valid(Owner owner) {
  return owner.native_can ? transport::wired_update_valid(owner.session)
                          : ble_proxy::update_owner_valid(owner.epoch, owner.session);
}

void reply(Owner owner, std::span<const uint8_t> payload) {
  if (!valid(owner))
    return;
  if (owner.native_can)
    transport::wired_update_reply(owner.peer, owner.session, payload);
  else
    ble_proxy::update_reply(owner.epoch, owner.session, payload);
}

uint32_t read32(std::span<const uint8_t> p, size_t at) {
  return uint32_t(p[at]) << 24 | uint32_t(p[at + 1]) << 16 | uint32_t(p[at + 2]) << 8 | p[at + 3];
}

void worker(void *) {
  auto request = make_external_state<Request>();
  ota_image::Image image;
  bike::VescUpload upload(image);
  Owner owner;
  image.set_commit_gate(
      [](void *context) { return !cancelling.load() && continuing.load() && valid(*static_cast<Owner *>(context)); },
      &owner);
  uint64_t last = 0, reboot_at = 0;
  std::array<uint8_t, 256> output{};
  for (;;) {
    const auto now = uint64_t(esp_timer_get_time() / 1000);
    if (reboot_at && now >= reboot_at)
      esp_restart();
    if (!reboot_at && (cancelling.exchange(false) ||
                       (upload.active() && (!continuing.load() || !valid(owner) || now - last > 60000)))) {
      upload.abort();
      cancellable.store(false);
      active.store(false);
      progress.store(0);
      message.store("Upload cancelled; current firmware retained");
    }
    if (xQueueReceive(queue, request.get(), pdMS_TO_TICKS(50)) != pdTRUE)
      continue;
    const auto p = std::span(request->bytes).first(request->size);
    if (!valid(request->owner) || p.empty())
      continue;
    const auto opcode = p[0];
    if (opcode == 0 && p.size() == 1) {
      std::array<uint8_t, 12> uuid{};
      if (esp_efuse_mac_get_default(uuid.data()) != ESP_OK)
        continue;
      const auto n = bike::can_bridge_firmware(output, uuid, HW_TYPE, esp_app_get_description()->version);
      reply(request->owner, std::span(output).first(n));
      continue;
    }
    if (opcode == 157 && p.size() == 1) {
      const auto n = bike::can_bridge_firmware_info(output, WFD_SOURCE_REVISION);
      reply(request->owner, std::span(output).first(n));
      continue;
    }
    if (opcode == 30)
      continue;
    const bool own = upload.active() && request->owner == owner;
    bool ok = !reboot_at && (upload.active() ? own && continuing.load() : allowed.load());
    if (opcode == 2) {
      if (ok && p.size() == 5) {
        cancellable.store(false);
        native_can.store(request->owner.native_can);
        message.store("Preparing firmware storage...");
        progress.store(0);
        active.store(true);
        ok = upload.begin(read32(p, 1));
        if (!ok)
          message.store("Unable to prepare update; current firmware retained");
      }
      else
        ok = false;
      active.store(upload.active());
      cancellable.store(upload.active());
      if (ok) {
        owner = request->owner;
        last = now;
        active.store(true);
        progress.store(0);
        message.store("Receiving firmware from VESC Tool");
      }
      const uint8_t ack[] = {2, uint8_t(ok)};
      reply(request->owner, ack);
    }
    else if (opcode == 3) {
      const auto offset = p.size() >= 5 ? read32(p, 1) : 0;
      ok = ok && own && p.size() > 5 && upload.write(offset, p.subspan(5));
      if (ok) {
        last = now;
        progress.store(int(upload.received() * 100 / upload.total()));
      }
      const uint8_t ack[] = {
          3, uint8_t(ok), uint8_t(offset >> 24), uint8_t(offset >> 16), uint8_t(offset >> 8), uint8_t(offset)};
      reply(request->owner, ack);
    }
    else if (opcode == 1 && p.size() == 1) {
      if (ok && own) {
        cancellable.store(false);
        message.store("Verifying received firmware...");
      }
      ok = ok && own && upload.finish();
      if (ok) {
        progress.store(100);
        message.store("Verified firmware; restarting");
        reboot_at = uint64_t(esp_timer_get_time() / 1000) + 500;
      }
      else {
        if (own) {
          upload.abort();
          active.store(false);
        }
        if (own || !upload.active())
          message.store("Firmware rejected; current firmware retained");
        const uint8_t text[] = {21, 'U', 'p', 'd', 'a', 't', 'e', ' ', 'r', 'e', 'j', 'e', 'c', 't', 'e', 'd'};
        reply(request->owner, text);
      }
    }
    else if (opcode == 29 && p.size() == 1 && ok) {
      cancellable.store(false);
      if (own)
        upload.abort();
      message.store("Restarting display");
      reboot_at = now + 250;
    }
  }
}

} // namespace

void start() {
  queue = create_packet_queue<Request>(4, control);
  configASSERT(queue && xTaskCreate(worker, "vesc_update", 8192, nullptr, 4, nullptr) == pdPASS);
}

void set_allowed(bool begin, bool continue_update) {
  allowed.store(begin);
  continuing.store(continue_update);
}

bool handles(std::span<const uint8_t> p) {
  return !p.empty() && (p[0] == 0 || p[0] == 157 || p[0] == 1 || p[0] == 2 || p[0] == 3 || p[0] == 29 || p[0] == 30);
}

void submit(Owner owner, std::span<const uint8_t> p) {
  if (!queue || p.empty() || p.size() > bike::max_payload)
    return;
  Request r;
  r.owner = owner;
  r.size = p.size();
  std::copy(p.begin(), p.end(), r.bytes.begin());
  xQueueSend(queue, &r, 0); // Missing ACK applies bounded client retry/backpressure.
}

void cancel() {
  cancelling.store(true);
}

Snapshot snapshot() {
  return {active.load(), progress.load(), message.load(), cancellable.load(), native_can.load()};
}

bool busy() {
  return active.load();
}

} // namespace vesc_update
