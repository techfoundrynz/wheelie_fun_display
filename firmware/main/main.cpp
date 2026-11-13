#include "bike/ble_name.hpp"
#include "bike/ble_signal.hpp"
#include "bike/integration.hpp"
#include "bike/lock_policy.hpp"
#include "bike/polling.hpp"
#include "bike/protocol.hpp"
#include "bike/stable_rows.hpp"
#include "bike/telemetry.hpp"
#include "bike/units.hpp"
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
#include "ota_image.hpp"
#include "render_profile.hpp"
#include "security.hpp"
#include "slint_backend.hpp"
#include "slint_rendering.hpp"
#include "transport.hpp"
#include "tuning.hpp"
#include "vesc_update.hpp"
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

slint::SharedString value(double v, const char *suffix, int precision = 0) {
  char s[48];
  std::snprintf(s, sizeof(s), "%.*f%s", precision, double(v), suffix);
  return s;
}

// Keep stable rows and only notify Slint when displayed content changes.
template <class Item> class StatRows {
  bike::StableRows<slint::VectorModel<Item>> rows;

public:

  explicit StatRows(slint::VectorModel<Item> &target) : rows(target) {
  }

  void add(const char *label, slint::SharedString text) {
    rows.add(Item{label, text});
  }
};

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
    return "Connected | waiting for telemetry";
  case transport::Status::Retry:
    return "Reconnecting";
  case transport::Status::Paused:
    return "Vehicle link paused for update";
  case transport::Status::SecurityRequired:
    return "Bike requires BLE authentication";
  case transport::Status::Error:
    return "Vehicle link unavailable | check wiring or restart";
  }
  return "Bluetooth";
}

bike::Orientation load_preferences(Dashboard &ui) {
  auto orientation = board::orientation;
  nvs_handle_t h;
  if (nvs_open("display", NVS_READONLY, &h) != ESP_OK)
    return orientation;
  uint8_t metric = 1, celsius = 1, light = 70, proxy = 0, footer_mode = 0;
  uint16_t rotation = static_cast<int>(orientation);
  nvs_get_u8(h, "metric", &metric);
  nvs_get_u8(h, "celsius", &celsius);
  nvs_get_u8(h, "light", &light);
  nvs_get_u16(h, "rotation", &rotation);
  nvs_get_u8(h, "ble_proxy", &proxy);
  const auto footer_result = nvs_get_u8(h, "footer_mode", &footer_mode);
  if (footer_result == ESP_ERR_NVS_NOT_FOUND) {
    uint8_t legacy_auto_hide = 0;
    if (nvs_get_u8(h, "hide_footer", &legacy_auto_hide) == ESP_OK && legacy_auto_hide != 0)
      footer_mode = static_cast<uint8_t>(FooterVisibility::Auto);
  }
  else if (footer_result != ESP_OK || footer_mode > static_cast<uint8_t>(FooterVisibility::Auto)) {
    footer_mode = static_cast<uint8_t>(FooterVisibility::Show);
  }
  nvs_close(h);
  ui.set_metric(metric != 0);
  ui.set_celsius(celsius != 0);
  ui.set_brightness(std::clamp<int>(light, 10, 100));
  ui.set_ble_proxy_enabled(proxy != 0);
  ui.set_footer_visibility(static_cast<FooterVisibility>(footer_mode));
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
  ui->set_display_safe_inset_x(DISPLAY_SAFE_INSET_X);
  ui->set_display_safe_inset_y(DISPLAY_SAFE_INSET_Y);
  ui->set_initial_width(board::width);
  ui->set_initial_height(board::height);
  auto orientation = load_preferences(*ui);
  ui->set_rotation(static_cast<int>(orientation));
  ui->set_initial_width(bike::display_width(orientation));
  ui->set_initial_height(bike::display_height(orientation));
  set_slint_orientation(orientation);
  if (ui->get_native_page_snapshots()) {
    slint_rendering::configure_page_slides(
        [ui] {
          slint_rendering::PageSlideState state;
          state.active = ui->get_native_slide_active();
          state.incoming = ui->get_displayed_page();
          state.content_active = ui->get_native_page_content_active();
          state.available = ui->get_native_slides_allowed();
          state.viewport = {int(ui->get_native_page_x() + .5f), int(ui->get_native_page_y() + .5f),
                            int(ui->get_native_page_width() + .5f), int(ui->get_native_page_height() + .5f)};
          if (state.active) {
            state.incoming = ui->get_native_slide_incoming();
            state.outgoing = ui->get_native_slide_outgoing();
            state.direction = ui->get_native_slide_direction();
            state.progress = ui->get_native_slide_progress();
          }
          return state;
        },
        [ui](int page) { ui->set_native_capture_page(page); });
  }
  bool rotate_pending = false;
  board::brightness(ui->get_brightness());
  vesc_update::start();
  transport::start();
  const auto initial_link = transport::snapshot();
  const auto &initial_presentation =
      bike::presentation_integration(initial_link.integration, bike::integration_id(ui->get_integration_choice()),
                                     initial_link.status == transport::Status::Ready);
  ble_proxy::set_wfb_name_override(initial_presentation.id() == bike::IntegrationId::Wfb);
  ui->set_proxy_name_locked(ble_proxy::name_locked());
  ui->set_proxy_name(ble_proxy::runtime_name().c_str());
  ui->set_can_available(transport::available(transport::Kind::Can));
  ui->set_uart_available(transport::available(transport::Kind::Uart));
  ble_proxy::set_enabled(ui->get_ble_proxy_enabled());
  network::start();
  ui->set_firmware_version(esp_app_get_description()->version);
  ui->set_device_label(board::label);
  auto wifi_items = std::make_shared<slint::VectorModel<WifiItem>>();
  auto about_items = std::make_shared<slint::VectorModel<AboutItem>>();
  ui->set_about_items(about_items);
  auto vesc_stats = std::make_shared<slint::VectorModel<StatItem>>();
  auto bms_stats = std::make_shared<slint::VectorModel<StatItem>>();
  auto bms_cells = std::make_shared<slint::VectorModel<StatItem>>();
  auto bms_temperatures = std::make_shared<slint::VectorModel<StatItem>>();
  ui->set_vesc_stats(vesc_stats);
  ui->set_bms_stats(bms_stats);
  ui->set_bms_cells(bms_cells);
  ui->set_bms_temperatures(bms_temperatures);
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
                           : edit_stage == 3 ? "BLE name | up to 29 bytes"
                                             : "Password | blank for open network");
    ui->set_editing(edit_stage != 0);
  };
  auto clear_editor = [&] {
    std::fill(editor.begin(), editor.end(), '\0');
    editor.clear();
    edit_stage = 0;
    project_editor();
  };
  ui->on_edit_proxy_name([&] {
    if (ble_proxy::name_locked() || security::blocked())
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
    else if (edit_stage == 3 && ble_proxy::name_locked()) {
      clear_editor();
    }
    else if (edit_stage == 3 && ble_proxy::set_name(editor)) {
      ui->set_proxy_name(ble_proxy::runtime_name().c_str());
      clear_editor();
    }
  });
  std::string pairing_digits;
  uint32_t pairing_ticket = 0;
  auto clear_pairing = [&] {
    std::fill(pairing_digits.begin(), pairing_digits.end(), '\0');
    pairing_digits.clear();
    ui->set_pairing_value("");
  };
  ui->on_pairing_clear(clear_pairing);
  ui->on_pairing_type([&](int digit) {
    const auto prompt = ble_security::prompt();
    if (!security::pairing_permitted() || !prompt.waiting || prompt.ticket != pairing_ticket || digit < 0 ||
        digit > 9 || pairing_digits.size() >= 6)
      return;
    pairing_digits.push_back('0' + digit);
    ui->set_pairing_value(pairing_digits.c_str());
  });
  ui->on_pairing_backspace([&] {
    if (!pairing_digits.empty()) {
      pairing_digits.back() = '\0';
      pairing_digits.pop_back();
    }
    ui->set_pairing_value(pairing_digits.c_str());
  });
  ui->on_pairing_submit([&] {
    if (!security::pairing_permitted() || !bike::valid_ble_pin(pairing_digits))
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
    return !security::blocked() && !ble_proxy::update_mode() && !vesc_update::busy() && !ble_proxy::phone_owns_link() &&
           (transport::snapshot().status != transport::Status::Ready || telemetry.can_configure(now_ms()));
  };
  ui->on_update_check([] { network::check_release(); });
  ui->on_vesc_update_mode_requested([&](bool enabled) {
    if (security::blocked() || network::snapshot().busy || vesc_update::busy())
      return;
    if (enabled && (!maintenance_allowed() || ota_image::busy()))
      return;
    if (ble_proxy::set_update_mode(enabled))
      tuning::cancel();
  });
  ui->on_vesc_update_cancel([] { vesc_update::cancel(); });
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
  std::array<transport::Device, 6> displayed_peers{};
  int displayed_count = 0;
  ui->on_connect_peer([&](int index) {
    if (!security::blocked() && !vesc_update::busy() && !ble_proxy::update_mode() && index >= 0 &&
        index < displayed_count)
      transport::connect(displayed_peers[index].address, ui->get_integration_choice());
  });
  ui->on_transport_requested([&](int value) {
    if (value < 0 || value > 2 || security::blocked() || ble_proxy::update_mode() || vesc_update::busy() ||
        network::snapshot().busy || ble_proxy::phone_owns_link())
      return;
    if (transport::select(transport::Kind(value))) {
      tuning::cancel();
      telemetry = {};
    }
  });
  ui->on_connect_wired([&] {
    if (security::blocked() || vesc_update::busy() || ble_proxy::update_mode() ||
        transport::selected() == transport::Kind::Ble)
      return;
    const int choice = ui->get_integration_choice();
    transport::connect("", choice);
  });
  ui->on_discover_controller([&] {
    if (!security::blocked() && !vesc_update::busy() && !ble_proxy::update_mode() && !network::snapshot().busy &&
        transport::rediscover()) {
      tuning::cancel();
      telemetry = {};
    }
  });
  ui->on_forget_peer([&] {
    if (security::blocked() || vesc_update::busy() || ble_proxy::update_mode())
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
      nvs_set_u8(h, "footer_mode", static_cast<uint8_t>(ui->get_footer_visibility()));
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
  tuning::init(*ui, telemetry);
  uint32_t session = 0;
  uint32_t setup_received = 0, setup_rejected = 0, bike_received = 0;
  uint64_t next_telemetry_log = 0;
  bool logged_setup_stale = false, logged_bike_stale = false;
  uint64_t next_setup_poll = 0, next_vehicle_poll = 0, next_bms_poll = 0, next_stats = 0, next_ui = 0, ready_since = 0;
  int old_count = -1;
  std::array<transport::Device, 6> old_peers{};
  const auto ui_started = now_ms();
  security::init(*ui, telemetry);
  configASSERT(heap_caps_check_integrity_all(true));
  ESP_LOGI("display", "UI initialized: stack_free=%u, internal_free=%u, reset_reason=%u",
           unsigned(uxTaskGetStackHighWaterMark(nullptr)), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           unsigned(esp_reset_reason()));
  set_slint_contact_observer([ui](bool down) { ui->invoke_screen_contact(down); });
  ui->show();
  configASSERT(heap_caps_check_integrity_all(true));
  ESP_LOGI("display", "UI shown: stack_free=%u", unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  for (;;) {
    const int64_t iteration_started = esp_timer_get_time();
    if (rotate_pending) {
      // Defer until the input dispatch and previous frame's DMA are done.
      rotate_pending = false;
      tuning::cancel();
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
#if WFD_RENDER_PROFILE
    render_profile::step(ui, orientation, now, ui_started);
    render_profile::current_page = ui->get_page();
#endif
    const auto link = transport::snapshot();
    vehicle = &bike::integration(link.integration);
    const bool ready = link.status == transport::Status::Ready;
    if (session != link.session || !ready) {
      telemetry = {};
      session = link.session;
      next_setup_poll = next_vehicle_poll = 0;
      next_bms_poll = 0;
      next_stats = 0;
    }
    if (!ready)
      ready_since = 0;
    else if (!ready_since)
      ready_since = now;
    transport::Packet received;
    while (transport::receive(received)) {
      if (received.discontinuity) {
        telemetry = {};
        tuning::reset();
        continue;
      }
      if (ready && received.session == session) {
        const auto payload = std::span<const uint8_t>(received.bytes.data(), received.size);
        tuning::receive(payload);
        const bool decoded = vehicle->decode(payload, now, telemetry);
        if (!payload.empty() && payload[0] == 47) {
          ++setup_received;
          setup_rejected += !decoded;
        }
        else if (!payload.empty() && payload[0] == 36 && decoded)
          ++bike_received;
        (void)decoded;
      }
    }
    security::tick(now);
    vesc_update::set_allowed(!security::blocked() && !network::snapshot().busy &&
                                 (ble_proxy::update_mode() || !ready || telemetry.can_configure(now)),
                             !security::blocked());
    tuning::tick(now);
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
    if (ready && now >= next_setup_poll) {
      const uint8_t setup[] = {47};
      if (transport::poll(setup))
        next_setup_poll = now + bike::telemetry_poll_interval_ms;
    }
    if (ready && now >= next_vehicle_poll && !vehicle->poll().empty()) {
      if (transport::poll(vehicle->poll()))
        next_vehicle_poll = now + bike::telemetry_poll_interval_ms;
    }
    if (ready && (ui->get_page() == 1 || next_bms_poll == 0) && now >= next_bms_poll &&
        (!ble_proxy::phone_owns_link() || telemetry.bms.received_ms != 0)) {
      const uint8_t bms_values[] = {96}; // Read cached VESC BMS values; never change BMS configuration.
      if (transport::poll(bms_values))
        next_bms_poll = now + bike::bms_poll_interval_ms;
    }
    if (now >= next_ui) {
      const bool fresh = ready && telemetry.fresh(now);
      const bool bike_fresh = telemetry.bike_valid && now - telemetry.bike_ms < 1500;
      ui->set_connected(ready);
      ui->set_connection_pending(link.status != transport::Status::Disconnected &&
                                 link.status != transport::Status::Error && !ready);
      ui->set_link_present(link.link_present);
      ui->set_transport_kind(int(transport::selected()));
      ui->set_controller_status(link.controller_info);
      ui->set_active_integration(int(vehicle->id()));
      if (link.link_present)
        ui->set_integration_choice(link.selection == bike::IntegrationId::Auto ? -1 : int(link.selection));
      const auto &presentation =
          bike::presentation_integration(link.integration, bike::integration_id(ui->get_integration_choice()), ready);
      ble_proxy::set_wfb_name_override(presentation.id() == bike::IntegrationId::Wfb);
      ui->set_proxy_name_locked(ble_proxy::name_locked());
      ui->set_proxy_name(ble_proxy::runtime_name().c_str());
      if (ble_proxy::name_locked() && edit_stage == 3)
        clear_editor();
      ui->set_vehicle_brand(presentation.name());
      ui->set_supports_tuning(presentation.features().tuning);
      ui->set_supports_presets(presentation.features().ride_presets);
      ui->set_supports_limit(presentation.features().speed_limit);
      ui->set_supports_lock(presentation.features().lock);
      ui->set_fresh(fresh);
      const bool integration_stale = ready && now - ready_since > 1500 && !vehicle->poll().empty() && !bike_fresh;
      ui->set_data_stale(integration_stale || (ready && now - ready_since > 1500 && !fresh));
      const bool setup_stale = ready && now - ready_since > 1500 && !fresh;
      if (ble_proxy::phone_owns_link() &&
          (now >= next_telemetry_log || logged_setup_stale != setup_stale || logged_bike_stale != integration_stale)) {
        const auto diagnostic_now = now_ms();
        ESP_LOGI("telemetry",
                 "setup_age_ms=%lld bike_age_ms=%lld setup_rx=%u setup_rejected=%u bike_rx=%u "
                 "rssi_valid=%d rssi_dbm=%d rssi_bars=%d rssi_age_ms=%lld",
                 telemetry.setup_valid ? static_cast<long long>(diagnostic_now - telemetry.setup_ms) : -1LL,
                 telemetry.bike_valid ? static_cast<long long>(diagnostic_now - telemetry.bike_ms) : -1LL,
                 unsigned(setup_received), unsigned(setup_rejected), unsigned(bike_received), link.rssi_valid,
                 link.rssi, ui->get_bike_ble_signal(),
                 link.rssi_sample_ms && diagnostic_now >= link.rssi_sample_ms
                     ? static_cast<long long>(diagnostic_now - link.rssi_sample_ms)
                     : -1LL);
        next_telemetry_log = now + 5000;
      }
      logged_setup_stale = setup_stale;
      logged_bike_stale = integration_stale;
      const bool upstream_ble = ready && transport::selected() == transport::Kind::Ble && link.rssi_valid;
      ui->set_bike_ble_signal(upstream_ble ? bike::ble_signal_level(link.rssi) : -1);
      ui->set_bike_ble_rssi(upstream_ble ? link.rssi : 0);
      const auto wifi = network::snapshot();
      ui->set_wifi_status(wifi.status.c_str());
      ui->set_wifi_saved(wifi.ssid.c_str());
      ui->set_wifi_connected(wifi.connected);
      ui->set_network_busy(wifi.busy);
      ui->set_can_change_transport(!security::blocked() && !wifi.busy && !ble_proxy::phone_owns_link() &&
                                   !vesc_update::busy() && !ble_proxy::update_mode());
      ui->set_release_version(wifi.version.c_str());
      ui->set_update_available(wifi.available);
      ui->set_updating(wifi.updating);
      ui->set_update_cancellable(wifi.cancellable);
      ui->set_update_progress(wifi.progress);
      ui->set_reboot_ready(wifi.reboot_ready);
      ui->set_can_maintain(maintenance_allowed());
      const auto vesc_ota = vesc_update::snapshot();
      ui->set_vesc_update_mode(ble_proxy::update_mode());
      ui->set_vesc_update_active(vesc_ota.active);
      ui->set_vesc_update_cancellable(vesc_ota.cancellable);
      ui->set_vesc_update_transport(vesc_ota.native_can ? "CAN" : "BLE");
      ui->set_vesc_update_progress(vesc_ota.progress);
      ui->set_vesc_update_status(vesc_ota.status);
      const auto proxy = ble_proxy::snapshot();
      ui->set_phone_connected(proxy.authorized);
      ui->set_ble_proxy_status(!proxy.enabled      ? "Off"
                               : proxy.authorized  ? "Phone connected | VESC link reserved for phone"
                               : proxy.connected   ? "Pairing phone..."
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
        StatRows about(*about_items);
        auto stat = [&](const char *label, slint::SharedString text) { about.add(label, text); };
        stat("Version", app->version);
        stat("Hardware", HW_TYPE);
        stat("Build", slint::SharedString(app->date) + " " + app->time);
        stat("Application hash", hash);
        stat("ESP-IDF", app->idf_ver);
        stat("CPU", slint::SharedString(CONFIG_IDF_TARGET) + " | " + value(chip.cores, " cores"));
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
                       value(wifi.networks[i].rssi, wifi.networks[i].secured ? " dBm | locked" : " dBm | open")});
        old_wifi_count = wifi.count;
        old_wifi = wifi.networks;
      }
      ui->set_can_configure(!security::blocked() && ready && !ble_proxy::phone_owns_link() &&
                            telemetry.can_configure(now));
      ui->set_connection(!ready && link.status == transport::Status::Discovering && link.controller_info[0]
                             ? slint::SharedString(link.controller_info)
                         : integration_stale                 ? "Connected | bike telemetry stale"
                         : fresh                             ? "Connected"
                         : ready && now - ready_since > 1500 ? "Connected | telemetry stale"
                                                             : status_text(link.status));
      ui->set_speed(fresh ? value(std::abs(telemetry.speed_mps) * (ui->get_metric() ? 3.6f : 2.2369363f), "", 1)
                          : "--");
      ui->set_speed_progress(fresh ? std::abs(telemetry.speed_mps) / 15.6464f : 0);
      ui->set_battery(fresh && telemetry.battery_valid ? value(telemetry.battery_percent, "%") : "--");
      ui->set_battery_level(fresh && telemetry.battery_valid ? telemetry.battery_percent : 0);
      ui->set_voltage(fresh ? value(telemetry.voltage, " V", 1) : "--");
      const auto distance_unit = ui->get_metric() ? " km" : " mi";
      ui->set_trip(fresh ? value(bike::distance_in_units(telemetry.distance_abs_m, ui->get_metric()), distance_unit, 2)
                         : "--");
      ui->set_odometer(fresh && telemetry.odometer_valid
                           ? value(bike::distance_in_units(telemetry.odometer_m, ui->get_metric()), distance_unit, 1)
                           : "--");
      ui->set_power(fresh ? value(telemetry.battery_current * telemetry.voltage, " W") : "--");
      ui->set_motor_current(fresh ? value(telemetry.motor_current, " A", 1) : "--");
      ui->set_battery_current(fresh ? value(telemetry.battery_current, " A", 1) : "--");
      ui->set_roll(bike_fresh ? value(telemetry.roll_deg, "\u00B0", 1) : "--");
      ui->set_mosfet(fresh ? value(ui->get_celsius() ? telemetry.mosfet_c : telemetry.mosfet_c * 1.8f + 32.f,
                                   ui->get_celsius() ? " \u00B0C" : " \u00B0F")
                           : "--");
      ui->set_motor(fresh ? value(ui->get_celsius() ? telemetry.motor_c : telemetry.motor_c * 1.8f + 32.f,
                                  ui->get_celsius() ? " \u00B0C" : " \u00B0F")
                          : "--");
      ui->set_pitch(bike_fresh ? value(telemetry.pitch_deg, "\u00B0", 1) : "--");
      if (ui->get_page() == 1 && now >= next_stats) {
        next_stats = now + 500;
        StatRows controller(*vesc_stats), pack(*bms_stats), cells(*bms_cells), temperatures(*bms_temperatures);
        const auto live = [&](double number, const char *suffix, int precision = 1) {
          return fresh ? value(number, suffix, precision) : slint::SharedString("--");
        };
        const auto temperature = [&](float degrees) {
          return value(ui->get_celsius() ? degrees : degrees * 1.8f + 32.f, ui->get_celsius() ? " \u00B0C" : " \u00B0F",
                       1);
        };
        controller.add("Duty cycle", live(telemetry.duty_cycle * 100, "%"));
        controller.add("Electrical RPM", live(telemetry.erpm, " eRPM", 0));
        controller.add("Consumed charge", live(telemetry.amp_hours, " Ah", 3));
        controller.add("Regenerated charge", live(telemetry.amp_hours_charged, " Ah", 3));
        controller.add("Consumed energy", live(telemetry.watt_hours, " Wh"));
        controller.add("Regenerated energy", live(telemetry.watt_hours_charged, " Wh"));
        controller.add("Signed distance",
                       fresh ? value(bike::distance_in_units(telemetry.distance_m, ui->get_metric()), distance_unit, 2)
                             : "--");
        controller.add("PID position", live(telemetry.pid_position_deg, "\u00B0"));
        controller.add("Controller ID",
                       fresh && telemetry.controller_id_valid ? value(telemetry.controller_id, "") : "--");
        controller.add("Controllers",
                       fresh && telemetry.controller_count_valid ? value(telemetry.controller_count, "") : "--");
        controller.add("Remaining energy",
                       fresh && telemetry.remaining_wh_valid ? value(telemetry.remaining_wh, " Wh") : "--");
        controller.add("Controller uptime",
                       fresh && telemetry.uptime_valid ? value(telemetry.uptime_ms / 1000., " s", 0) : "--");
        controller.add("Fault code", live(telemetry.fault, "", 0));
        const auto &bms = telemetry.bms;
        const bool bms_fresh = ready && bms.available && bms.fresh(now);
        ui->set_bms_state(bms_fresh ? "Recent controller reply | cached BMS values" : "No recent VESC BMS data.");
        if (bms_fresh) {
          pack.add("Cells", value(bms.cell_count, ""));
          pack.add("Temperature sensors", value(bms.temp_count, ""));
          pack.add("Balancing cells",
                   value(std::count(bms.balancing.begin(), bms.balancing.begin() + bms.cell_count, true), ""));
          pack.add("Pack voltage", value(bms.pack_voltage, " V", 2));
          pack.add("Charge voltage", value(bms.charge_voltage, " V", 2));
          pack.add("Input current", value(bms.input_current, " A", 2));
          pack.add("IC input current", value(bms.input_current_ic, " A", 2));
          pack.add("Charge counter", value(bms.amp_hours, " Ah", 3));
          pack.add("Energy counter", value(bms.watt_hours, " Wh", 1));
          pack.add("State of charge", bms.soc_valid ? value(bms.soc * 100, "%", 1) : "--");
          pack.add("State of health", bms.soh_valid ? value(bms.soh * 100, "%", 1) : "--");
          pack.add("BMS CAN ID", bms.can_id_valid && bms.can_id != 255 ? value(bms.can_id, "") : "--");
          pack.add("IC temperature", temperature(bms.ic_c));
          pack.add("Humidity sensor", temperature(bms.humidity_sensor_c));
          pack.add("Humidity", value(bms.humidity_percent, "%", 1));
          pack.add("Maximum cell temp", temperature(bms.max_cell_c));
          if (bms.lifetime_valid) {
            pack.add("Lifetime charge in", value(bms.lifetime_ah_charged_total, " Ah", 2));
            pack.add("Lifetime energy in", value(bms.lifetime_wh_charged_total, " Wh", 1));
            pack.add("Lifetime charge out", value(bms.lifetime_ah_discharged_total, " Ah", 2));
            pack.add("Lifetime energy out", value(bms.lifetime_wh_discharged_total, " Wh", 1));
          }
          if (bms.pressure_valid)
            pack.add("Pressure", value(bms.pressure_pa, " Pa", 0));
          if (bms.status_valid)
            pack.add("Status", bms.status.data());
          if (bms.cell_count) {
            const auto end = bms.cell_volts.begin() + bms.cell_count;
            const auto [minimum, maximum] = std::minmax_element(bms.cell_volts.begin(), end);
            pack.add("Lowest cell", value(*minimum, " V", 3));
            pack.add("Highest cell", value(*maximum, " V", 3));
            pack.add("Cell spread", value((*maximum - *minimum) * 1000, " mV", 0));
          }
          for (size_t i = 0; i < bms.cell_count; ++i) {
            char label[32];
            std::snprintf(label, sizeof(label), "Cell %u%s", unsigned(i + 1), bms.balancing[i] ? " | balancing" : "");
            cells.add(label, value(bms.cell_volts[i], " V", 3));
          }
          for (size_t i = 0; i < bms.temp_count; ++i) {
            char label[24];
            std::snprintf(label, sizeof(label), "Sensor %u", unsigned(i + 1));
            temperatures.add(label, temperature(bms.temps_c[i]));
          }
        }
      }
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
