#include "bike/ble_name.hpp"
#include "bike/integration.hpp"
#include "bike/protocol.hpp"
#include "bike/telemetry.hpp"
#include "ble_proxy.hpp"
#include "ble_security.hpp"
#include "board.hpp"
#include "dashboard.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "network.hpp"
#include "nvs.h"
#include "nvs_flash.h"
#include "security.hpp"
#include "slint_backend.hpp"
#include "transport.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace
{

uint64_t now_ms() {
  return esp_timer_get_time() / 1000;
}

bike::Telemetry telemetry;

const bike::VehicleIntegration *vehicle = &bike::integration(bike::IntegrationId::Wfb);

bool send_payload(std::span<const uint8_t> payload) {
  if (ble_proxy::phone_owns_link())
    return false;
  return transport::send(payload);
}

slint::SharedString value(float v, const char *suffix, int precision = 0) {
  char s[48];
  std::snprintf(s, sizeof(s), "%.*f%s", precision, double(v), suffix);
  return s;
}

const char *status_text(transport::Status s) {
  switch (s) {
  case transport::Status::Disconnected:
    return "Disconnected";
  case transport::Status::Scanning:
    return "Looking for your bike";
  case transport::Status::Connecting:
    return "Connecting to bike...";
  case transport::Status::Discovering:
    return "Setting up vehicle link...";
  case transport::Status::Ready:
    return "Connected / waiting for telemetry";
  case transport::Status::Retry:
    return "Reconnecting / open Bike to choose";
  case transport::Status::Paused:
    return "Vehicle link paused for update";
  case transport::Status::SecurityRequired:
    return "Bike requires BLE authentication";
  case transport::Status::Error:
    return "Vehicle link unavailable / check wiring or restart";
  }
  return "Bluetooth";
}

bike::Orientation load_preferences(Dashboard &ui) {
  auto orientation = board::orientation;
  nvs_handle_t h;
  if (nvs_open("display", NVS_READONLY, &h) != ESP_OK)
    return orientation;
  uint8_t metric = 1, celsius = 1, light = 70, proxy = 0;
  uint16_t rotation = static_cast<int>(orientation);
  nvs_get_u8(h, "metric", &metric);
  nvs_get_u8(h, "celsius", &celsius);
  nvs_get_u8(h, "light", &light);
  nvs_get_u16(h, "rotation", &rotation);
  nvs_get_u8(h, "ble_proxy", &proxy);
  nvs_close(h);
  ui.set_metric(metric != 0);
  ui.set_celsius(celsius != 0);
  ui.set_brightness(std::clamp<int>(light, 10, 100));
  ui.set_ble_proxy_enabled(proxy != 0);
  return bike::valid_rotation(rotation) ? bike::orientation_from_degrees(rotation) : orientation;
}

} // namespace

extern "C" void app_main() {
  auto err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  auto display = board::init();
  init_slint_backend(display);
  auto ui = Dashboard::create();
  ui->set_display_safe_inset(DISPLAY_SAFE_INSET);
  ui->set_initial_width(board::width);
  ui->set_initial_height(board::height);
  auto orientation = load_preferences(*ui);
  ui->set_rotation(static_cast<int>(orientation));
  ui->set_initial_width(bike::display_width(orientation));
  ui->set_initial_height(bike::display_height(orientation));
  set_slint_orientation(orientation);
  bool rotate_pending = false;
  board::brightness(ui->get_brightness());
  transport::start();
  ui->set_proxy_name(ble_proxy::name().c_str());
  ui->set_can_available(transport::available(transport::Kind::Can));
  ui->set_uart_available(transport::available(transport::Kind::Uart));
  ble_proxy::set_enabled(ui->get_ble_proxy_enabled());
  network::start();
  ui->set_firmware_version(esp_app_get_description()->version);
  ui->set_device_label(board::label);
  auto wifi_items = std::make_shared<slint::VectorModel<WifiItem>>();
  auto about_items = std::make_shared<slint::VectorModel<AboutItem>>();
  ui->set_about_items(about_items);
  uint64_t next_about = 0;
  int old_wifi_count = -1;
  std::array<network::AccessPoint, 12> old_wifi{};
  ui->set_wifi_networks(wifi_items);
  std::string editor, editing_ssid;
  int edit_stage = 0;
  auto project_editor = [&] {
    ui->set_editor_value(editor.c_str());
    ui->set_editor_masked(std::string(editor.size(), '*').c_str());
    ui->set_editor_secret(edit_stage == 2);
    ui->set_editor_heading(edit_stage == 1   ? "Network name (SSID)"
                           : edit_stage == 3 ? "BLE name / up to 29 bytes"
                                             : "Password / blank for open network");
    ui->set_editing(edit_stage != 0);
  };
  auto clear_editor = [&] {
    std::fill(editor.begin(), editor.end(), '\0');
    editor.clear();
    edit_stage = 0;
    project_editor();
  };
  ui->on_edit_proxy_name([&] {
    if (security::blocked())
      return;
    editor = ble_proxy::name();
    edit_stage = 3;
    project_editor();
  });
  ui->on_wifi_scan([] { network::scan(); });
  ui->on_wifi_connect_saved([] { network::connect_saved(); });
  ui->on_wifi_forget([] { network::forget(); });
  ui->on_wifi_manual([&] {
    editor.clear();
    edit_stage = 1;
    project_editor();
  });
  ui->on_wifi_select([&](int i) {
    const auto wifi = network::snapshot();
    if (i < 0 || i >= wifi.count)
      return;
    editing_ssid = wifi.networks[i].ssid;
    if (!wifi.networks[i].secured) {
      network::connect(editing_ssid, "");
      return;
    }
    editor.clear();
    edit_stage = 2;
    project_editor();
  });
  ui->on_editor_type([&](slint::SharedString text) {
    const size_t limit = edit_stage == 1 ? 32 : edit_stage == 3 ? 29 : 64;
    if (edit_stage && editor.size() + text.size() <= limit)
      editor.append(text.data(), text.size());
    project_editor();
  });
  ui->on_editor_backspace([&] {
    if (!editor.empty()) {
      size_t p = editor.size() - 1;
      while (p && (uint8_t(editor[p]) & 0xc0) == 0x80)
        --p;
      editor.erase(p);
    }
    project_editor();
  });
  ui->on_editor_cancel(clear_editor);
  ui->on_editor_done([&] {
    if (edit_stage == 1) {
      if (editor.empty())
        return;
      editing_ssid = editor;
      editor.clear();
      edit_stage = 2;
      project_editor();
    }
    else if (edit_stage == 2) {
      network::connect(editing_ssid, editor);
      clear_editor();
    }
    else if (edit_stage == 3 && ble_proxy::set_name(editor)) {
      ui->set_proxy_name(editor.c_str());
      clear_editor();
    }
  });
  std::string pairing_digits;
  uint32_t pairing_ticket = 0;
  auto clear_pairing = [&] {
    std::fill(pairing_digits.begin(), pairing_digits.end(), '\0');
    pairing_digits.clear();
    ui->set_pairing_mask("");
  };
  ui->on_pairing_type([&](int digit) {
    const auto prompt = ble_security::prompt();
    if (!security::pairing_permitted() || !prompt.waiting || prompt.ticket != pairing_ticket || digit < 0 ||
        digit > 9 || pairing_digits.size() >= 6)
      return;
    pairing_digits.push_back('0' + digit);
    ui->set_pairing_mask(std::string(pairing_digits.size(), '*').c_str());
  });
  ui->on_pairing_backspace([&] {
    if (!pairing_digits.empty()) {
      pairing_digits.back() = '\0';
      pairing_digits.pop_back();
    }
    ui->set_pairing_mask(std::string(pairing_digits.size(), '*').c_str());
  });
  ui->on_pairing_submit([&] {
    if (!security::pairing_permitted() || pairing_digits.size() != 6)
      return;
    uint32_t pin = 0;
    for (char digit : pairing_digits)
      pin = pin * 10 + unsigned(digit - '0');
    ble_security::submit(pairing_ticket, pin);
    clear_pairing();
  });
  ui->on_pairing_cancel([&] {
    ble_security::cancel(pairing_ticket);
    clear_pairing();
  });
  ui->on_pairing_retry([&] {
    if (security::pairing_permitted())
      ble_security::retry();
    clear_pairing();
  });
  auto maintenance_allowed = [&] {
    return !security::blocked() && !ble_proxy::phone_owns_link() &&
           (transport::snapshot().status != transport::Status::Ready || telemetry.can_configure(now_ms()));
  };
  ui->on_update_check([] { network::check_release(); });
  ui->on_update_cancel([] { network::cancel(); });
  ui->on_update_install([&] {
    if (maintenance_allowed())
      network::install();
  });
  ui->on_update_reboot([&] {
    if (maintenance_allowed())
      network::reboot();
  });
  auto peers = std::make_shared<slint::VectorModel<Peer>>();
  ui->set_peers(peers);
  int pending_kind = 0, pending_value = 0;
  uint32_t pending_session = 0;
  uint64_t pending_since = 0;
  std::array<transport::Device, 6> displayed_peers{};
  int displayed_count = 0;
  ui->on_connect_peer([&](int index) {
    if (!security::blocked() && index >= 0 && index < displayed_count)
      transport::connect(displayed_peers[index].address, ui->get_integration_choice());
  });
  ui->on_transport_requested([&](int value) {
    if (value < 0 || value > 2 || security::blocked() || network::snapshot().busy || ble_proxy::phone_owns_link())
      return;
    if (transport::select(transport::Kind(value))) {
      pending_kind = 0;
      ui->set_confirming(false);
      telemetry = {};
    }
  });
  ui->on_connect_wired([&] {
    if (security::blocked() || transport::selected() == transport::Kind::Ble)
      return;
    const int choice = ui->get_integration_choice();
    transport::connect("", choice);
  });
  ui->on_discover_controller([&] {
    if (!security::blocked() && !network::snapshot().busy && transport::rediscover()) {
      pending_kind = 0;
      ui->set_confirming(false);
      telemetry = {};
    }
  });
  ui->on_forget_peer([&] {
    if (security::blocked())
      return;
    transport::forget();
    telemetry = {};
  });
  ui->on_orientation_requested([&](int degrees) {
    if (!bike::valid_rotation(degrees))
      return;
    orientation = bike::orientation_from_degrees(degrees);
    rotate_pending = true;
  });
  ui->on_preferences_changed([&] {
    board::brightness(ui->get_brightness());
    nvs_handle_t h;
    if (nvs_open("display", NVS_READWRITE, &h) == ESP_OK) {
      nvs_set_u8(h, "metric", ui->get_metric());
      nvs_set_u8(h, "celsius", ui->get_celsius());
      nvs_set_u8(h, "light", ui->get_brightness());
      nvs_commit(h);
      nvs_close(h);
    }
  });
  ui->on_ble_proxy_changed([&](bool enabled) {
    if (security::blocked())
      return;
    ble_proxy::set_enabled(enabled);
    nvs_handle_t h;
    if (nvs_open("display", NVS_READWRITE, &h) == ESP_OK) {
      nvs_set_u8(h, "ble_proxy", enabled);
      nvs_commit(h);
      nvs_close(h);
    }
  });
  auto prepare = [&](int kind, int v) {
    if ((kind == 1 && !vehicle->features().ride_presets) || (kind == 2 && !vehicle->features().speed_limit))
      return;
    if (security::blocked() || ble_proxy::phone_owns_link() || !telemetry.can_configure(now_ms()) ||
        transport::snapshot().status != transport::Status::Ready)
      return;
    pending_kind = kind;
    pending_value = v;
    pending_session = transport::snapshot().session;
    pending_since = now_ms();
    if (kind == 1) {
      const char *names[] = {"Eco", "Cruise", "Sport"};
      ui->set_confirm_text(slint::SharedString("Ride response: ") + names[std::clamp(v, 0, 2)]);
    }
    else
      ui->set_confirm_text(slint::SharedString("Speed limit: ") +
                           value(ui->get_metric() ? v * 1.609344f : v, ui->get_metric() ? " km/h" : " mph"));
    ui->set_confirming(true);
  };
  ui->on_apply_mode([&](int v) {
    if (v >= 0 && v <= 2)
      prepare(1, v);
  });
  ui->on_apply_limit([&](int v) {
    if (v >= 5 && v <= 35)
      prepare(2, v);
  });
  ui->on_cancel_change([&] {
    pending_kind = 0;
    ui->set_confirming(false);
  });
  ui->on_confirm_change([&] {
    const auto link = transport::snapshot();
    bool sent = false;
    if (!security::blocked() && !ble_proxy::phone_owns_link() && link.status == transport::Status::Ready &&
        link.session == pending_session && now_ms() - pending_since < 30000 && telemetry.can_configure(now_ms())) {
      if (pending_kind == 1) {
        std::array<uint8_t, 6> cmd{};
        if (vehicle->ride(static_cast<bike::RideMode>(pending_value), cmd))
          sent = send_payload(cmd);
      }
      if (pending_kind == 2) {
        std::array<uint8_t, 4> cmd{};
        if (vehicle->speed_limit(pending_value, cmd))
          sent = send_payload(cmd);
      }
    }
    // No acknowledgement/readback is documented: never claim the bike applied it.
    ui->set_feedback(sent ? "Command queued. Bike application is unverified."
                          : "Change not sent. Check connection and stop the bike.");
    pending_kind = 0;
    ui->set_confirming(false);
  });
  uint32_t session = 0;
  uint64_t next_poll = 0, next_ui = 0, ready_since = 0;
  int old_count = -1;
  std::array<transport::Device, 6> old_peers{};
  const auto ui_started = now_ms();
  security::init(*ui, telemetry);
  configASSERT(heap_caps_check_integrity_all(true));
  ESP_LOGI("display", "UI initialized: stack_free=%u, internal_free=%u", unsigned(uxTaskGetStackHighWaterMark(nullptr)),
           unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  ui->show();
  configASSERT(heap_caps_check_integrity_all(true));
  ESP_LOGI("display", "UI shown: stack_free=%u", unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  for (;;) {
    const int64_t iteration_started = esp_timer_get_time();
    if (rotate_pending) {
      // Defer until the input dispatch and previous frame's DMA are done.
      rotate_pending = false;
      pending_kind = 0;
      ui->set_confirming(false);
      ui->set_initial_width(bike::display_width(orientation));
      ui->set_initial_height(bike::display_height(orientation));
      set_slint_orientation(orientation);
      ui->set_rotation(static_cast<int>(orientation));
      nvs_handle_t h;
      if (nvs_open("display", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u16(h, "rotation", static_cast<int>(orientation));
        nvs_commit(h);
        nvs_close(h);
      }
    }
    const auto now = now_ms();
    const auto link = transport::snapshot();
    vehicle = &bike::integration(link.integration);
    const bool ready = link.status == transport::Status::Ready;
    if (session != link.session || !ready) {
      telemetry = {};
      session = link.session;
      next_poll = 0;
      if (pending_kind) {
        pending_kind = 0;
        ui->set_confirming(false);
      }
    }
    if (!ready)
      ready_since = 0;
    else if (!ready_since)
      ready_since = now;
    transport::Packet received;
    while (transport::receive(received)) {
      if (received.discontinuity) {
        telemetry = {};
        continue;
      }
      if (ready && received.session == session)
        vehicle->decode(std::span<const uint8_t>(received.bytes.data(), received.size), now, telemetry);
    }
    security::tick(now);
    const auto pairing = ble_security::prompt();
    if (pairing.ticket != pairing_ticket) {
      pairing_ticket = pairing.ticket;
      clear_pairing();
      if (pairing.open)
        cancel_slint_input();
    }
    ui->set_pairing_open(pairing.open);
    ui->set_pairing_failed(pairing.failed);
    ui->set_pairing_ready(pairing.waiting || pairing.failed);
    ui->set_pairing_permitted(security::pairing_permitted());
    if (security::blocked() && pending_kind) {
      pending_kind = 0;
      ui->set_confirming(false);
    }
    if (ready && !ble_proxy::phone_owns_link() && now >= next_poll) {
      const uint8_t setup[] = {47};
      send_payload(setup);
      if (!vehicle->poll().empty())
        send_payload(vehicle->poll());
      next_poll = now + 200; // 5 Hz; independent of untrusted board loopTime
    }
    if (pending_kind && now - pending_since > 30000) {
      pending_kind = 0;
      ui->set_confirming(false);
    }
    if (now >= next_ui) {
      const bool fresh = ready && telemetry.fresh(now);
      const bool bike_fresh = telemetry.bike_valid && now - telemetry.bike_ms < 1500;
      ui->set_connected(ready);
      ui->set_link_present(link.link_present);
      ui->set_transport_kind(int(transport::selected()));
      ui->set_controller_status(link.controller_info);
      ui->set_active_integration(int(vehicle->id()));
      if (link.link_present)
        ui->set_integration_choice(link.selection == bike::IntegrationId::Auto ? -1 : int(link.selection));
      ui->set_vehicle_brand(vehicle->name());
      ui->set_supports_presets(vehicle->features().ride_presets);
      ui->set_supports_limit(vehicle->features().speed_limit);
      ui->set_supports_lock(vehicle->features().lock);
      ui->set_fresh(fresh);
      const auto wifi = network::snapshot();
      ui->set_wifi_status(wifi.status.c_str());
      ui->set_wifi_saved(wifi.ssid.c_str());
      ui->set_wifi_connected(wifi.connected);
      ui->set_network_busy(wifi.busy);
      ui->set_can_change_transport(!security::blocked() && !wifi.busy && !ble_proxy::phone_owns_link());
      ui->set_release_version(wifi.version.c_str());
      ui->set_update_available(wifi.available);
      ui->set_updating(wifi.updating);
      ui->set_update_progress(wifi.progress);
      ui->set_reboot_ready(wifi.reboot_ready);
      ui->set_can_maintain(maintenance_allowed());
      const auto proxy = ble_proxy::snapshot();
      ui->set_ble_proxy_status(!proxy.enabled      ? "Off"
                               : proxy.connected   ? "Phone connected / VESC link reserved for phone"
                               : proxy.advertising ? slint::SharedString("Connect the phone to ") + ui->get_proxy_name()
                                                   : "Waiting for VESC connection");
      if (ui->get_page() == 6 && now >= next_about) {
        const auto *app = esp_app_get_description();
        const auto *partition = esp_ota_get_running_partition();
        esp_chip_info_t chip{};
        esp_chip_info(&chip);
        uint32_t flash_bytes = 0;
        esp_flash_get_size(nullptr, &flash_bytes);
        char hash[17]{};
        for (int i = 0; i < 8; ++i)
          std::snprintf(hash + 2 * i, 3, "%02x", app->app_elf_sha256[i]);
        about_items->clear();
        auto stat = [&](const char *label, slint::SharedString text) {
          about_items->push_back(AboutItem{label, text});
        };
        stat("Version", app->version);
        stat("Hardware", HW_TYPE);
        stat("Build", slint::SharedString(app->date) + " " + app->time);
        stat("Application hash", hash);
        stat("ESP-IDF", app->idf_ver);
        stat("CPU", slint::SharedString(CONFIG_IDF_TARGET) + " / " + value(chip.cores, " cores"));
        stat("Display",
             value(bike::display_width(orientation), " x ") + value(bike::display_height(orientation), " pixels"));
        stat("Panel", value(board::panel_width, " x ") + value(board::panel_height, " pixels native"));
        stat("Panel clock", value(board::display_clock_hz / 1000000.f, " MHz ") + board::display_bus_name);
        stat("Flash", value(flash_bytes / 1048576.f, " MiB"));
        stat("PSRAM", value(esp_psram_get_size() / 1048576.f, " MiB"));
        stat("Internal free", value(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024.f, " KiB", 1));
        stat("Internal minimum", value(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024.f, " KiB", 1));
        stat("Internal largest", value(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024.f, " KiB", 1));
        stat("PSRAM free", value(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024.f, " KiB"));
        stat("UI stack free", value(uxTaskGetStackHighWaterMark(nullptr), " bytes"));
        stat("Uptime", value(now / 1000.f, " seconds"));
        stat("Reset reason", value(esp_reset_reason(), ""));
        stat("Boot partition", partition ? partition->label : "Unknown");
        stat("Bluetooth", status_text(link.status));
        stat("Wi-Fi", wifi.connected ? wifi.ssid.c_str() : "Disconnected");
        stat("IP address", wifi.ip.c_str());
        stat("Vehicle transport", transport::name(transport::selected()));
        const auto rendering = slint_render_stats();
        stat("Rendered frames", value(rendering.frames, ""));
        stat("Frame work (EMA)", value(rendering.frame_us / 1000.f, " ms", 1));
        stat("DMA wait (EMA)", value(rendering.wait_us / 1000.f, " ms", 1));
        stat("Longest frame", value(rendering.max_frame_us / 1000.f, " ms", 1));
        stat("Panel chunks (EMA)", value(rendering.chunks, ""));
        next_about = now + 2000;
      }
      bool wifi_changed = old_wifi_count != wifi.count;
      for (int i = 0; i < wifi.count; ++i)
        wifi_changed |= old_wifi[i].ssid != wifi.networks[i].ssid || old_wifi[i].rssi != wifi.networks[i].rssi ||
                        old_wifi[i].secured != wifi.networks[i].secured;
      if (wifi_changed) {
        wifi_items->clear();
        for (int i = 0; i < wifi.count; ++i)
          wifi_items->push_back(
              WifiItem{wifi.networks[i].ssid.c_str(),
                       value(wifi.networks[i].rssi, wifi.networks[i].secured ? " dBm / locked" : " dBm / open")});
        old_wifi_count = wifi.count;
        old_wifi = wifi.networks;
      }
      ui->set_can_configure(!security::blocked() && ready && !ble_proxy::phone_owns_link() &&
                            telemetry.can_configure(now));
      ui->set_connection(link.link_present && ble_proxy::phone_owns_link() ? "Phone app active"
                         : !ready && link.status == transport::Status::Discovering && link.controller_info[0]
                             ? slint::SharedString(link.controller_info)
                         : fresh ? bike_fresh ? slint::SharedString(vehicle->name()) + " / live" : "VESC / live"
                         : ready && now - ready_since > 1500 ? "Connected / telemetry stale"
                                                             : status_text(link.status));
      ui->set_speed(fresh ? value(std::abs(telemetry.speed_mps) * (ui->get_metric() ? 3.6f : 2.2369363f), "", 1)
                          : "--");
      ui->set_speed_progress(fresh ? std::abs(telemetry.speed_mps) / 15.6464f : 0);
      ui->set_battery(fresh ? value(telemetry.battery_percent, "%") : "--");
      ui->set_battery_level(fresh ? telemetry.battery_percent : 0);
      ui->set_voltage(fresh ? value(telemetry.voltage, " V", 1) : "--");
      ui->set_power(fresh ? value(telemetry.battery_current * telemetry.voltage, " W") : "--");
      ui->set_mosfet(fresh ? value(ui->get_celsius() ? telemetry.mosfet_c : telemetry.mosfet_c * 1.8f + 32.f,
                                   ui->get_celsius() ? " \u00B0C" : " \u00B0F")
                           : "--");
      ui->set_motor(fresh ? value(ui->get_celsius() ? telemetry.motor_c : telemetry.motor_c * 1.8f + 32.f,
                                  ui->get_celsius() ? " \u00B0C" : " \u00B0F")
                          : "--");
      ui->set_pitch(bike_fresh ? value(telemetry.pitch_deg, "\u00B0", 1) : "--");
      ui->set_hot(fresh && (telemetry.mosfet_c >= 75 || telemetry.motor_c >= 75));
      ui->set_fault_text(fresh && telemetry.fault
                             ? slint::SharedString("Controller fault ") + value(telemetry.fault, "")
                         : fresh && telemetry.mosfet_c >= 75 && telemetry.motor_c >= 75 ? "Controller + motor hot"
                         : fresh && telemetry.mosfet_c >= 75                            ? "Controller hot"
                         : fresh && telemetry.motor_c >= 75                             ? "Motor hot"
                                                                                        : "");
      bool changed = link.count != old_count;
      for (int i = 0; i < link.count; ++i)
        changed |= std::strcmp(old_peers[i].name, link.devices[i].name) ||
                   std::strcmp(old_peers[i].address, link.devices[i].address) ||
                   old_peers[i].rssi != link.devices[i].rssi;
      if (changed) {
        peers->clear();
        displayed_peers = link.devices;
        displayed_count = link.count;
        std::stable_sort(displayed_peers.begin(), displayed_peers.begin() + displayed_count,
                         [](const auto &a, const auto &b) {
                           return bike::preferred_bike_name(a.name) > bike::preferred_bike_name(b.name);
                         });
        for (int i = 0; i < link.count; ++i)
          peers->push_back(
              Peer{displayed_peers[i].name, displayed_peers[i].address, value(displayed_peers[i].rssi, " dBm")});
        old_count = link.count;
        old_peers = link.devices;
      }
      next_ui = now + 100;
    }
    render_slint_frame();
    // Only accept a pending OTA image after initialization and ten seconds
    // of a functioning UI loop. Early reset/panic leaves rollback available.
    static bool accepted = false;
    if (!accepted && now - ui_started > 10000) {
      esp_ota_mark_app_valid_cancel_rollback();
      accepted = true;
      const auto rendering = slint_render_stats();
      ESP_LOGI("display",
               "UI ready: %s=%u MHz, internal_free=%u, largest=%u, frames=%llu, work_ema_us=%u, wait_ema_us=%u, "
               "ui_stack_free=%u",
               board::display_bus_name, unsigned(board::display_clock_hz / 1000000),
               unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
               unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
               static_cast<unsigned long long>(rendering.frames), unsigned(rendering.frame_us),
               unsigned(rendering.wait_us), unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    }
    vTaskDelay(pdMS_TO_TICKS(slint_loop_delay_ms(esp_timer_get_time() - iteration_started)));
  }
}
