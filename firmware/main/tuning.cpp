#include "tuning.hpp"
#include "bike/configuration_write.hpp"
#include "bike/integration.hpp"
#include "bike/tuning.hpp"
#include "ble_proxy.hpp"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "miniz.h"
#include "security.hpp"
#include "transport.hpp"
#include "vesc_update.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace tuning
{
namespace
{

enum class Phase {
  Idle,
  Schema,
  Read,
  Ready,
  Confirm,
  Preflight,
  VerifyDelay,
  Verify,
  Error
};

constexpr size_t schema_limit = 64 * 1024;
constexpr uint64_t reply_timeout = 2000;

uint32_t read32(std::span<const uint8_t> p, size_t at) {
  return uint32_t(p[at]) << 24 | uint32_t(p[at + 1]) << 16 | uint32_t(p[at + 2]) << 8 | p[at + 3];
}

void write32(uint8_t *p, uint32_t value) {
  for (int i = 3; i >= 0; --i) {
    p[i] = uint8_t(value);
    value >>= 8;
  }
}

bool same_profile(std::span<const uint8_t> a, std::span<const uint8_t> b) {
  return a.size() > 1 && a.size() == b.size() && std::equal(a.begin() + 1, a.end(), b.begin() + 1);
}

struct Manager {
  Dashboard *ui = nullptr;
  const bike::Telemetry *telemetry = nullptr;
  std::unique_ptr<bike::TuningBackend> backend;
  bike::IntegrationId backend_integration = bike::IntegrationId::Auto;
  Phase phase = Phase::Idle;
  uint32_t session = 0;
  uint64_t deadline = 0, retry_at = 0, confirmation_at = 0;
  unsigned retries = 0;
  bool awaiting = false, phone = false, refresh_requested = false, metric = true, initialized = false;
  std::vector<uint8_t> compressed;
  size_t schema_offset = 0, requested_size = 0;
  std::array<uint8_t, 1024> baseline{}, candidate{};
  size_t baseline_size = 0, candidate_size = 0;
  std::shared_ptr<slint::VectorModel<TuneField>> fields = std::make_shared<slint::VectorModel<TuneField>>();

  bool matches(const transport::Snapshot &link) const {
    return backend && link.status == transport::Status::Ready && link.integration == backend->integration_id();
  }

  void project_labels() {
    auto model = [](std::span<const std::string_view> labels) {
      auto result = std::make_shared<slint::VectorModel<slint::SharedString>>();
      for (const auto label : labels)
        result->push_back(slint::SharedString(std::string(label).c_str()));
      return result;
    };
    ui->set_tuning_sections(model(backend ? backend->section_labels() : std::span<const std::string_view>{}));
    ui->set_tuning_presets(model(backend ? backend->preset_labels() : std::span<const std::string_view>{}));
    ui->set_tuning_preset_descriptions(
        model(backend ? backend->preset_descriptions() : std::span<const std::string_view>{}));
    const int count = backend ? int(backend->section_labels().size()) : 0;
    ui->set_tuning_section(count ? std::clamp(ui->get_tuning_section(), 0, count - 1) : 0);
  }

  bool can_write(uint64_t now) const {
    if (vesc_update::busy() || ble_proxy::update_mode())
      return false;
    const auto link = transport::snapshot();
    return bike::configuration_write_allowed(
        {link.status == transport::Status::Ready, matches(link), !security::blocked(), ble_proxy::phone_owns_link(),
         telemetry->can_configure(now), candidate_size != 0, link.session, session, now, confirmation_at});
  }

  void status(const char *text) {
    ui->set_tuning_status(text);
  }

  void error(const char *text) {
    phase = Phase::Error;
    awaiting = false;
    candidate_size = 0;
    ui->set_confirming(false);
    ui->set_tuning_ready(false);
    ui->set_tuning_busy(false);
    status(text);
  }

  void project(bool drafts = false) {
    const bool loaded = backend && backend->ready();
    const bool ready = phase == Phase::Ready && loaded && !phone;
    ui->set_tuning_ready(ready);
    ui->set_tuning_busy(phase == Phase::Schema || phase == Phase::Read || phase == Phase::Preflight ||
                        phase == Phase::Verify || phase == Phase::VerifyDelay);
    ui->set_tuning_dirty(loaded && backend->dirty());
    const int saved = loaded ? backend->preset_index(false) : -1;
    const auto labels = backend ? backend->preset_labels() : std::span<const std::string_view>{};
    const std::string response = saved >= 0 && size_t(saved) < labels.size()    ? std::string(labels[saved])
                                 : saved >= 0 && size_t(saved) == labels.size() ? "Custom"
                                                                                : "Not read from bike";
    ui->set_tuning_saved_response(response.c_str());
    const auto limit = loaded ? backend->limit_mph(false) : std::nullopt;
    ui->set_tuning_limit_min(backend ? backend->minimum_limit_mph() : 5);
    ui->set_tuning_limit_max(backend ? backend->maximum_limit_mph() : 35);
    ui->set_tuning_unlimited(backend && backend->supports_unlimited());
    if (limit) {
      char text[64];
      std::snprintf(text, sizeof(text), ui->get_metric() ? "%.0f km/h" : "%.0f mph",
                    ui->get_metric() ? *limit * 1.609344 : double(*limit));
      ui->set_tuning_saved_limit(*limit == 0 ? "No limit" : text);
    }
    else
      ui->set_tuning_saved_limit("Not read from bike");
    if (loaded && drafts) {
      ui->set_draft_mode(backend->preset_index(true));
      if (auto limit = backend->limit_mph(true))
        ui->set_draft_limit(*limit);
    }
    else if (!loaded)
      ui->set_draft_mode(-1);
    size_t row = 0;
    if (loaded && !backend->section_labels().empty()) {
      for (const auto &control : backend->controls(ui->get_tuning_section())) {
        TuneField item{};
        item.id = control.id;
        item.label = control.label.c_str();
        item.value = control.value.c_str();
        item.saved = control.saved.c_str();
        item.can_decrease = control.can_decrease;
        item.can_increase = control.can_increase;
        item.changed = control.changed;
        item.kind = control.kind;
        item.minimum = control.minimum;
        item.maximum = control.maximum;
        item.position = control.position;
        item.step = control.step;
        item.default_position = control.default_position;
        item.has_default = control.has_default;
        item.read_only = control.read_only;
        item.read_only_reason = control.read_only_reason.c_str();
        item.off_grid = control.off_grid;
        item.tick_every = control.tick_every;
        auto options = std::make_shared<slint::VectorModel<slint::SharedString>>();
        for (const auto &option : control.options)
          options->push_back(option.c_str());
        item.options = options;
        if (row < fields->row_count())
          fields->set_row_data(row, item);
        else
          fields->push_back(item);
        ++row;
      }
    }
    while (fields->row_count() > row)
      fields->erase(row);
  }

  void reset() {
    backend.reset();
    initialized = false;
    backend_integration = bike::IntegrationId::Auto;
    project_labels();
    compressed.clear();
    baseline_size = candidate_size = schema_offset = 0;
    phase = Phase::Idle;
    awaiting = false;
    retries = 0;
    refresh_requested = false;
    ui->set_confirming(false);
    status("Read the bike's configuration before tuning.");
    project();
  }

  void cancel() {
    // A committed write can only be verified, never cancelled retroactively.
    if (phase == Phase::VerifyDelay || phase == Phase::Verify) {
      ui->set_confirming(false);
      return;
    }
    if (phase == Phase::Confirm || phase == Phase::Preflight) {
      phase = (backend && backend->ready()) ? Phase::Ready : Phase::Idle;
      awaiting = false;
    }
    candidate_size = 0;
    ui->set_confirming(false);
    project();
  }

  void prepare() {
    const auto now = uint64_t(esp_timer_get_time() / 1000);
    if (phase != Phase::Ready || !(backend && backend->ready()) || !backend->dirty()) {
      ui->set_feedback("No tuning changes to save.");
      return;
    }
    candidate_size = backend->encode(candidate);
    confirmation_at = now;
    if (!can_write(now)) {
      candidate_size = 0;
      ui->set_feedback("Change not sent. Stop the bike and check the connection.");
      return;
    }
    std::string summary = "Save these settings to the bike:\n";
    for (const auto &control : backend->changed_controls())
      summary += control.label + ": " + control.saved + " to " + control.value + "\n";
    phase = Phase::Confirm;
    ui->set_confirm_text(summary.c_str());
    ui->set_confirming(true);
    project();
  }

  void choose_preset(int index) {
    if (phase != Phase::Ready || !backend || phone || security::blocked())
      return;
    if (!backend->select_preset(index)) {
      const auto reason = backend->error();
      error(reason.empty() ? "This bike schema does not support that response preset." : reason.data());
      return;
    }
    project(true);
    prepare();
  }

  void adjust(int id, int delta) {
    if (phase != Phase::Ready || !backend || phone || security::blocked())
      return;
    if (backend->adjust(id, delta))
      project(true);
  }

  void begin_read(bool force_schema = false) {
    if (!backend)
      return;
    if (force_schema) {
      backend->clear();
      compressed.clear();
      schema_offset = 0;
    }
    phase = backend->schema_ready() ? Phase::Read : Phase::Schema;
    awaiting = false;
    retries = 0;
    retry_at = 0;
    status(phase == Phase::Schema ? "Reading and verifying the bike's configuration schema…"
                                  : "Reading bike settings…");
    project();
  }

  bool parse_compressed() {
    if (compressed.size() < 6)
      return false;
    const size_t length = read32(compressed, 0);
    if (!length || length > bike::TuningBackend::max_schema_bytes)
      return false;
    auto *xml = static_cast<uint8_t *>(heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto *decompressor = static_cast<tinfl_decompressor *>(
        heap_caps_calloc(1, sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!xml || !decompressor) {
      heap_caps_free(xml);
      heap_caps_free(decompressor);
      return false;
    }
    tinfl_init(decompressor);
    size_t input = compressed.size() - 4, output = length;
    const auto result = tinfl_decompress(decompressor, compressed.data() + 4, &input, xml, xml, &output,
                                         TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    const bool valid = result == TINFL_STATUS_DONE && input == compressed.size() - 4 && output == length &&
                       backend->parse_schema({reinterpret_cast<char *>(xml), length});
    heap_caps_free(decompressor);
    heap_caps_free(xml);
    compressed.clear();
    return valid;
  }

  void receive(std::span<const uint8_t> payload) {
    const auto link = transport::snapshot();
    if (!matches(link) || link.session != session || phone || !awaiting || payload.empty())
      return;
    if (phase == Phase::Schema && payload[0] == 92) {
      if (payload.size() < 11 || payload[1] != backend->config_slot()) {
        error("Invalid bike schema reply.");
        return;
      }
      const size_t total = read32(payload, 2), offset = read32(payload, 6), count = payload.size() - 10;
      if (offset < schema_offset)
        return; // Late retry of an already consumed chunk.
      if (!total || total > schema_limit || offset != schema_offset || count > requested_size || offset > total ||
          count > total - offset || (!compressed.empty() && compressed.size() != total)) {
        error("Bike schema reply failed its size/offset checks.");
        return;
      }
      if (compressed.empty())
        compressed.resize(total);
      std::copy(payload.begin() + 10, payload.end(), compressed.begin() + offset);
      schema_offset += count;
      awaiting = false;
      retries = 0;
      if (schema_offset == total) {
        if (!parse_compressed()) {
          error("Unsupported or invalid vehicle configuration schema.");
          return;
        }
        begin_read();
      }
      return;
    }
    if ((phase == Phase::Read || phase == Phase::Preflight || phase == Phase::Verify) && payload[0] == 93) {
      if (payload.size() < 6 || payload[1] != backend->config_slot()) {
        error("Invalid bike configuration reply.");
        return;
      }
      awaiting = false;
      const bool preflight = phase == Phase::Preflight, verify = phase == Phase::Verify;
      if (preflight) {
        if (!same_profile(payload, {baseline.data(), baseline_size})) {
          if (!backend->decode(payload)) {
            error("Bike configuration changed to an unsupported schema.");
            return;
          }
          baseline_size = payload.size();
          std::copy(payload.begin(), payload.end(), baseline.begin());
          phase = Phase::Ready;
          candidate_size = 0;
          status("Bike settings changed since readback. Review the new values before saving.");
          project(true);
          return;
        }
        const auto now = uint64_t(esp_timer_get_time() / 1000);
        if (!can_write(now) || !transport::send({candidate.data(), candidate_size})) {
          error("Settings were not sent. Stop the bike and check the connection.");
          return;
        }
        phase = Phase::VerifyDelay;
        retry_at = now + 200;
        retries = 0;
        status("Settings sent. Waiting for configuration readback…");
        project();
        return;
      }
      const bool matched = !verify || same_profile(payload, {candidate.data(), candidate_size});
      if (!backend->decode(payload)) {
        error("Bike configuration signature or values were not supported.");
        return;
      }
      baseline_size = payload.size();
      std::copy(payload.begin(), payload.end(), baseline.begin());
      phase = Phase::Ready;
      candidate_size = 0;
      status(matched ? (verify ? "Saved profile verified by readback."
                               : "Bike profile read. Changes remain drafts until saved.")
                     : "Readback differs from the requested settings. Review the bike's reported values.");
      ui->set_feedback(matched && verify ? "Saved profile verified by readback."
                                         : "Review the bike profile before applying changes.");
      project(true);
    }
  }

  void tick(uint64_t now) {
    if (metric != ui->get_metric()) {
      metric = ui->get_metric();
      project();
    }
    const auto link = transport::snapshot();
    if (link.status != transport::Status::Ready) {
      if (initialized || backend || phase != Phase::Idle || session != link.session ||
          backend_integration != link.integration) {
        reset();
        session = link.session;
        backend_integration = link.integration;
      }
      return;
    }
    if (!initialized || link.session != session || backend_integration != link.integration) {
      reset();
      session = link.session;
      backend_integration = link.integration;
      backend = bike::make_tuning_backend(link.integration);
      initialized = true;
      project_labels();
      project();
    }
    if (!backend)
      return;
    const bool owned = ble_proxy::phone_owns_link();
    if (owned != phone) {
      phone = owned;
      cancel();
      awaiting = false;
      if ((backend && backend->ready()))
        backend->reset_draft();
      if (phone) {
        phase = Phase::Idle;
        status("Disconnect the phone to read or save bike settings.");
      }
      else
        begin_read();
      project(true);
    }
    if (phone || security::blocked()) {
      if (phase == Phase::Confirm || phase == Phase::Preflight) {
        cancel();
        status("Confirmation cancelled. Unlock and review the draft again.");
      }
      return;
    }
    if (phase == Phase::Confirm && now - confirmation_at >= 30000) {
      cancel();
      status("Confirmation expired. Review and apply again.");
    }
    if (refresh_requested && (phase == Phase::Ready || phase == Phase::Error || phase == Phase::Idle)) {
      refresh_requested = false;
      begin_read(phase == Phase::Error);
    }
    if (phase == Phase::Idle && ui->get_page() == 2)
      begin_read();
    if (phase == Phase::VerifyDelay && now >= retry_at)
      phase = Phase::Verify;
    if (phase != Phase::Schema && phase != Phase::Read && phase != Phase::Preflight && phase != Phase::Verify)
      return;
    if (phase == Phase::Preflight && !can_write(now)) {
      error("Settings were not sent. Stop the bike and try again.");
      return;
    }
    if (awaiting) {
      if (now < deadline)
        return;
      awaiting = false;
      if (++retries >= 3) {
        error(phase == Phase::Verify ? "Save readback timed out. Bike application is unverified."
                                     : "Bike settings read timed out. Tap Read bike settings to retry.");
        return;
      }
      retry_at = now + 100;
    }
    if (now < retry_at)
      return;
    std::array<uint8_t, 10> request{92, backend->config_slot()};
    size_t size = 2;
    if (phase == Phase::Schema) {
      requested_size = compressed.empty() ? 10 : std::min(size_t(384), compressed.size() - schema_offset);
      write32(request.data() + 2, uint32_t(requested_size));
      write32(request.data() + 6, uint32_t(schema_offset));
      size = 10;
    }
    else
      request[0] = 93;
    if (transport::send({request.data(), size})) {
      awaiting = true;
      deadline = now + reply_timeout;
    }
  }
};

Manager *manager = nullptr;

} // namespace

void init(Dashboard &ui, const bike::Telemetry &telemetry) {
  auto *memory = heap_caps_malloc(sizeof(Manager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  configASSERT(memory);
  manager = new (memory) Manager;
  manager->ui = &ui;
  manager->telemetry = &telemetry;
  ui.set_tuning_fields(manager->fields);
  manager->reset();
  ui.on_tuning_refresh([] { manager->refresh_requested = true; });
  ui.on_tuning_adjust([](int id, int delta) { manager->adjust(id, delta); });
  ui.on_tuning_set([](int id, float value) {
    if (manager->phase == Phase::Ready && manager->backend && !manager->phone && !security::blocked() &&
        manager->backend->set_control(id, value))
      manager->project(true);
  });
  ui.on_tuning_section_requested([](int section) {
    const int count = manager->backend ? int(manager->backend->section_labels().size()) : 0;
    if (section >= 0 && section < count) {
      manager->ui->set_tuning_section(section);
      manager->project();
    }
  });
  ui.on_tuning_save([] { manager->prepare(); });
  ui.on_tuning_reset([] {
    if (manager->phase == Phase::Ready) {
      manager->backend->reset_draft();
      manager->project(true);
    }
  });
  ui.on_apply_mode([](int mode) { manager->choose_preset(mode); });
  ui.on_apply_limit([](int mph) {
    if (manager->phase == Phase::Ready && manager->backend && !manager->phone && !security::blocked() &&
        manager->backend->set_limit_mph(mph)) {
      manager->project(true);
      manager->prepare();
    }
  });
  ui.on_cancel_change([] { manager->cancel(); });
  ui.on_confirm_change([] {
    if (manager->phase != Phase::Confirm)
      return;
    const auto now = uint64_t(esp_timer_get_time() / 1000);
    manager->ui->set_confirming(false);
    if (!manager->can_write(now)) {
      manager->error("Settings were not sent. Stop the bike and try again.");
      return;
    }
    manager->phase = Phase::Preflight;
    manager->awaiting = false;
    manager->retries = 0;
    manager->retry_at = 0;
    manager->status("Checking bike settings again before saving…");
    manager->project();
  });
}

void tick(uint64_t now) {
  if (manager)
    manager->tick(now);
}

void receive(std::span<const uint8_t> payload) {
  if (manager)
    manager->receive(payload);
}

void reset() {
  if (manager)
    manager->reset();
}

void cancel() {
  if (manager)
    manager->cancel();
}

} // namespace tuning
