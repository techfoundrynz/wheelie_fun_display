#include "security.hpp"
#include "bike/lock_policy.hpp"
#include "bike/protocol.hpp"
#include "ble_proxy.hpp"
#include "ble_security.hpp"
#include "board.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "psa/crypto.h"
#include "slint_backend.hpp"
#include "transport.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

namespace security
{
namespace
{

using Record = bike::LockRecord;

Record record, candidate;
bool input_pattern = false;
Dashboard *ui;
const bike::Telemetry *telemetry;
enum class Mode {
  None,
  New,
  Confirm,
  Change,
  Remove,
  Unlock
};
Mode mode = Mode::None;
bool configured = false, locked = false, corrupt = false, asleep = false, bike_confirmed = false;
uint64_t activity = 0;
uint32_t candidate_passkey = 0;
bool local_authenticated = false;
bike::PinAttempts attempts;
bike::LockRequest request;
std::string digits;
bool submit_requested = false;
int mode_requested = -1;

uint32_t numeric_pin() {
  uint32_t number = 0;
  for (char digit : digits)
    number = number * 10 + unsigned(digit - '0');
  return number;
}

uint64_t now_ms() {
  return esp_timer_get_time() / 1000;
}

void clear_digits() {
  std::fill(digits.begin(), digits.end(), '\0');
  digits.clear();
  ui->set_pin_value("");
}

void message(const char *text) {
  ui->set_pin_message(text);
}

bool save(const Record &value) {
  nvs_handle_t h;
  if (nvs_open("security", NVS_READWRITE, &h) != ESP_OK)
    return false;
  auto error = nvs_set_blob(h, "pin", &value, sizeof(value));
  if (error == ESP_OK)
    error = nvs_commit(h);
  nvs_close(h);
  return error == ESP_OK;
}

std::optional<std::array<uint8_t, 32>> hash(const Record &value, const std::string &pin) {
  std::array<uint8_t, 22> input{};
  std::copy(value.salt.begin(), value.salt.end(), input.begin());
  std::copy(pin.begin(), pin.end(), input.begin() + value.salt.size());
  std::array<uint8_t, 32> digest{};
  size_t digest_size = 0;
  if (psa_crypto_init() != PSA_SUCCESS) {
    std::fill(input.begin(), input.end(), 0);
    return std::nullopt;
  }
  const auto status =
      psa_hash_compute(PSA_ALG_SHA_256, input.data(), input.size(), digest.data(), digest.size(), &digest_size);
  std::fill(input.begin(), input.end(), 0);
  if (status != PSA_SUCCESS || digest_size != digest.size()) {
    std::fill(digest.begin(), digest.end(), 0);
    return std::nullopt;
  }
  return digest;
}

std::optional<bool> matches(const Record &value) {
  auto digest = hash(value, digits);
  if (!digest)
    return std::nullopt;
  uint8_t difference = 0;
  for (size_t i = 0; i < digest->size(); ++i)
    difference |= (*digest)[i] ^ value.digest[i];
  std::fill(digest->begin(), digest->end(), 0);
  return difference == 0;
}

bool entry_pattern() {
  return input_pattern;
}

void project_proxy_pin(uint32_t pin) {
  char code[7];
  std::snprintf(code, sizeof(code), "%06u", unsigned(pin));
  ui->set_proxy_pairing_code(code);
}

void open(Mode next) {
  cancel_slint_input();
  mode = next;
  clear_digits();
  activity = now_ms();
  ui->set_pin_open(true);
  const bool pattern = entry_pattern();
  ui->set_pin_pattern(pattern);
  ui->set_pin_heading(next == Mode::Remove ? "REMOVE LOCK"
                      : pattern            ? (next == Mode::New       ? "CREATE PATTERN"
                                              : next == Mode::Confirm ? "CONFIRM PATTERN"
                                                                      : "ENTER PATTERN")
                                           : (next == Mode::New       ? "CREATE PIN"
                                              : next == Mode::Confirm ? "CONFIRM PIN"
                                                                      : "ENTER PIN"));
  if (next == Mode::Unlock)
    ui->set_proxy_pairing_code("");
  message(next == Mode::Remove    ? (pattern ? "Draw your current pattern to remove the screen lock."
                                             : "Enter your current PIN to remove the screen lock.")
          : pattern               ? "Connect six dots, then lift your finger."
          : next == Mode::New     ? "Choose a six-digit PIN"
          : next == Mode::Confirm ? "Enter the new PIN again"
                                  : "Enter your six-digit PIN");
}

void close() {
  mode = Mode::None;
  clear_digits();
  candidate = {};
  candidate_passkey = 0;
  ui->set_pin_open(false);
}

void finish_unlock() {
  if (record.armed) {
    auto next = record;
    next.armed = 0;
    if (!save(next)) {
      message("Could not save unlock. Try again.");
      return;
    }
    record = next;
  }
  locked = false;
  bike_confirmed = false;
  ui->set_screen_locked(false);
  ble_proxy::set_paused(false);
  close();
  ESP_LOGI("security", "Screen unlocked: stack_free=%u", unsigned(uxTaskGetStackHighWaterMark(nullptr)));
}

bool send_lock(bool desired) {
  const auto link = transport::snapshot();
  const auto now = now_ms();
  const auto &vehicle = bike::integration(link.integration);
  if (link.status == transport::Status::SecurityRequired && local_authenticated) {
    ble_security::retry();
    message("Pair Bluetooth, then unlock the screen again.");
    return false;
  }
  if (!vehicle.features().lock) {
    message("Bike locking is not available for this integration.");
    return false;
  }
  if (request.pending || link.status != transport::Status::Ready || !bike::can_lock(*telemetry, now) ||
      !ble_proxy::begin_maintenance()) {
    message("Connect the bike, stop riding and disconnect the phone.");
    return false;
  }
  std::array<uint8_t, 4> command{};
  if (!vehicle.lock(desired, command))
    return false;
  const bool sent = transport::send(command);
  if (!sent) {
    if (!locked)
      ble_proxy::set_paused(false);
    message("Command not queued. Check the bike connection.");
    return false;
  }
  request = {true, desired, link.session, now};
  message(desired ? "Lock requested; waiting for bike telemetry" : "Unlock requested; waiting for bike telemetry");
  return true;
}

void lock_bike() {
  if (!bike::integration(transport::snapshot().integration).features().lock)
    return;
  if (!configured || locked || mode != Mode::None)
    return;
  if (!bike::can_lock(*telemetry, now_ms()) || transport::snapshot().status != transport::Status::Ready ||
      !ble_proxy::begin_maintenance()) {
    ui->set_bike_lock_status("Stop the connected bike and disconnect the phone first.");
    return;
  }
  auto next = record;
  next.armed = 1;
  if (!save(next)) {
    ble_proxy::set_paused(false);
    ui->set_bike_lock_status("Could not save lock. Bike lock was not sent.");
    return;
  }
  record = next;
  locked = true;
  local_authenticated = false;
  bike_confirmed = false;
  ui->set_screen_locked(true);
  open(Mode::Unlock);
  send_lock(true);
}

void submit() {
  const auto now = now_ms();
  if (mode == Mode::None || corrupt || request.pending || !attempts.allowed(now))
    return;
  if (entry_pattern() ? !bike::valid_pattern(digits) : !bike::valid_pin(digits)) {
    clear_digits();
    message(entry_pattern() ? "Connect six different dots." : "Enter six digits.");
    return;
  }
  if (mode == Mode::New) {
    candidate = record;
    candidate.version = 2;
    candidate.pattern = input_pattern;
    esp_fill_random(candidate.salt.data(), candidate.salt.size());
    auto digest = hash(candidate, digits);
    if (!digest) {
      clear_digits();
      message("PIN verification unavailable. Please retry.");
      return;
    }
    candidate.digest = *digest;
    std::fill(digest->begin(), digest->end(), 0);
    candidate_passkey = numeric_pin();
    open(Mode::Confirm);
    return;
  }
  if (mode == Mode::Confirm) {
    const auto correct = matches(candidate);
    clear_digits();
    if (!correct.has_value()) {
      message("PIN verification unavailable. Please retry.");
      return;
    }
    if (!*correct) {
      candidate = {};
      open(Mode::New);
      message(entry_pattern() ? "Patterns did not match. Draw a new pattern."
                              : "PINs did not match. Choose a PIN again.");
      return;
    }
    if (!save(candidate)) {
      message("Could not save screen lock. Try again.");
      return;
    }
    record = candidate;
    configured = true;
    ble_security::set_bike_pin(true, true, candidate_passkey, record.digest);
    project_proxy_pin(candidate_passkey);
    candidate_passkey = 0;
    ui->set_unlock_method(input_pattern);
    ui->set_pin_configured(true);
    ui->set_bike_lock_status(record.pattern ? "Pattern saved. Six dot numbers form the phone BLE code."
                                            : "PIN saved. Keep your code somewhere safe.");
    close();
    return;
  }
  const auto entered_pin = numeric_pin();
  const auto correct = matches(record);
  clear_digits();
  if (!correct.has_value()) {
    message("PIN verification unavailable. Please retry.");
    return;
  }
  attempts.result(*correct, now);
  auto updated = record;
  updated.failures = std::min(attempts.failures, 5u);
  if (!save(updated)) {
    message("Could not save lock attempt. Try again.");
    return;
  }
  record = updated;
  if (!*correct) {
    message(attempts.allowed(now) ? (entry_pattern() ? "Incorrect pattern" : "Incorrect PIN")
                                  : "Too many attempts. Wait 30 seconds.");
    return;
  }
  local_authenticated = true;
  ble_security::set_bike_pin(true, true, entered_pin, record.digest);
  project_proxy_pin(entered_pin);
  if (mode == Mode::Change) {
    open(Mode::New);
  }
  else if (mode == Mode::Remove) {
    nvs_handle_t h;
    if (nvs_open("security", NVS_READWRITE, &h) != ESP_OK) {
      message("Could not remove screen lock. Try again.");
      return;
    }
    const auto erased = nvs_erase_key(h, "pin");
    const auto committed = erased == ESP_OK ? nvs_commit(h) : erased;
    nvs_close(h);
    if (committed != ESP_OK) {
      message("Could not remove screen lock. Try again.");
      return;
    }
    record = {};
    configured = false;
    local_authenticated = false;
    bike_confirmed = false;
    ui->set_lock_timeout(record.timeout);
    ble_security::set_bike_pin(false, true, 0, {});
    ui->set_pin_configured(false);
    ui->set_proxy_pairing_code("");
    ui->set_unlock_method(input_pattern);
    ui->set_bike_lock_status("Screen lock removed.");
    close();
  }
  else if (mode == Mode::Unlock) {
    const auto now = now_ms();
    const bool disabled = telemetry->bike_valid && now - telemetry->bike_ms < 1500 && telemetry->state == 15;
    if (record.armed || disabled)
      send_lock(false);
    else
      finish_unlock();
  }
}

void change_entry_mode(int method) {
  if (mode == Mode::None || request.pending || corrupt || method < 0 || method > 1 || !attempts.allowed(now_ms()))
    return;
  nvs_handle_t h;
  if (nvs_open("security", NVS_READWRITE, &h) != ESP_OK) {
    message("Could not save entry mode. Try again.");
    return;
  }
  auto error = nvs_set_u8(h, "entry_mode", uint8_t(method));
  if (error == ESP_OK)
    error = nvs_commit(h);
  nvs_close(h);
  if (error != ESP_OK) {
    message("Could not save entry mode. Try again.");
    return;
  }
  input_pattern = method == 1;
  ESP_LOGI("security", "Entry mode saved: %s", input_pattern ? "pattern" : "PIN");
  ui->set_unlock_method(method);
  open(mode);
}

} // namespace

void init(Dashboard &dashboard, const bike::Telemetry &data) {
  ui = &dashboard;
  telemetry = &data;
  // Allocate the hardware SHA lock before the first UI frame.
  // A fixed, non-secret input also validates the crypto backend without touching NVS.
  const auto crypto_ready = hash(Record{}, "000000").has_value();
  nvs_handle_t h;
  auto error = nvs_open("security", NVS_READONLY, &h);
  if (error == ESP_OK) {
    std::array<uint8_t, sizeof(Record)> bytes{};
    size_t length = bytes.size();
    error = nvs_get_blob(h, "pin", bytes.data(), &length);
    configured = error == ESP_OK;
    corrupt = (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) ||
              (configured && !bike::decode_lock_record(std::span(bytes.data(), length), record));
    uint8_t preferred = record.pattern;
    if (nvs_get_u8(h, "entry_mode", &preferred) != ESP_OK || preferred > 1)
      preferred = record.pattern;
    input_pattern = preferred == 1;
    nvs_close(h);
  }
  else if (error != ESP_ERR_NVS_NOT_FOUND)
    corrupt = true;
  ble_security::set_bike_pin(configured || corrupt, !configured && !corrupt, 0, record.digest);
  ui->set_unlock_method(input_pattern);
  ESP_LOGI("security", "Entry mode: %s", input_pattern ? "pattern" : "PIN");
  ui->set_pin_configured(configured);
  ui->set_bike_lock_status(configured ? "Screen lock configured. Lock only while the bike is stopped."
                                      : "Set a screen lock to enable bike locking.");
  ui->set_lock_timeout(record.timeout);
  attempts.failures = record.failures;
  if (record.failures >= 5)
    attempts.retry_at = now_ms() + 30000;
  ui->on_unlock_method_requested([](int method) {
    if (!submit_requested && mode != Mode::None && method >= 0 && method <= 1)
      mode_requested = method;
  });
  ui->on_pin_setup([] {
    if (!locked && mode == Mode::None) {
      open(configured ? Mode::Change : Mode::New);
    }
  });
  ui->on_pin_remove([] {
    if (!locked && configured && mode == Mode::None)
      open(Mode::Remove);
  });
  ui->on_bike_lock(lock_bike);
  ui->on_screen_lock([] {
    if (!configured || locked || mode != Mode::None)
      return;
    locked = true;
    local_authenticated = false;
    ble_proxy::set_paused(true);
    ui->set_screen_locked(true);
    open(Mode::Unlock);
  });
  ui->on_pin_submit([] {
    if (mode != Mode::None && !submit_requested)
      submit_requested = true;
  });
  ui->on_pin_clear([] {
    if (!submit_requested)
      clear_digits();
  });
  ui->on_pin_cancel([] {
    if (!locked) {
      submit_requested = false;
      mode_requested = -1;
      close();
    }
  });
  ui->on_pin_digit([](int digit) {
    activity = now_ms();
    if (mode != Mode::None && digit >= 0 && digit <= 9 && digits.size() < 6 && !request.pending &&
        attempts.allowed(activity)) {
      if (entry_pattern())
        bike::append_pattern(digits, digit);
      else
        digits.push_back('0' + digit);
      ui->set_pin_value(digits.c_str());
    }
  });
  ui->on_pin_backspace([] {
    if (submit_requested)
      return;
    if (!digits.empty()) {
      digits.back() = '\0';
      digits.pop_back();
    }
    ui->set_pin_value(digits.c_str());
  });
  ui->on_lock_retry([] {
    if (locked && record.armed && !bike_confirmed)
      send_lock(true);
  });
  ui->on_lock_timeout_changed([](int value) {
    if (locked || !configured || (value != 15 && value != 30 && value != 60 && value != 120))
      return;
    auto next = record;
    next.timeout = value;
    if (save(next)) {
      record = next;
      ui->set_lock_timeout(value);
    }
    else
      ui->set_bike_lock_status("Could not save timeout");
  });
  if (configured || corrupt) {
    locked = true;
    local_authenticated = false;
    ble_proxy::set_paused(true);
    ui->set_screen_locked(true);
    open(Mode::Unlock);
    if (corrupt)
      message("Lock storage could not be read. Service required.");
  }
  if (!crypto_ready)
    message("PIN verification unavailable. Please retry.");
  activity = now_ms();
}

void tick(uint64_t now) {
  // Crypto and flash operations run outside the nested Slint pointer callbacks.
  // Keep submitted input intact until this point, including pointer-exit cancellation.
  if (submit_requested) {
    submit_requested = false;
    mode_requested = -1;
    submit();
  }
  if (mode_requested >= 0) {
    const int requested = mode_requested;
    mode_requested = -1;
    change_entry_mode(requested);
  }
  const auto link = transport::snapshot();
  if (configured && !locked && mode == Mode::None && link.status == transport::Status::Ready && telemetry->bike_valid &&
      now - telemetry->bike_ms < 1500 && telemetry->state == 15) {
    // Also honor a disable observed after reconnect or requested by another client.
    auto next = record;
    next.armed = 1;
    const bool saved = save(next);
    record = next;
    locked = true;
    bike_confirmed = true;
    ble_proxy::set_paused(true);
    ui->set_screen_locked(true);
    open(Mode::Unlock);
    message(saved ? "Bike reports disabled. Enter your PIN or pattern." : "Bike disabled; could not save lock state.");
  }
  if (request.pending) {
    if (link.status != transport::Status::Ready || link.session != request.session || now - request.since > 5000) {
      request.pending = false;
      message("Bike lock state unconfirmed. Reconnect and retry.");
    }
    else if (request.confirmed(*telemetry, link.session, now)) {
      const bool desired = request.desired;
      request.pending = false;
      if (desired) {
        bike_confirmed = true;
        message("Bike reports disabled. Enter your PIN or pattern.");
      }
      else
        finish_unlock();
    }
  }
  if (locked && record.armed && !request.pending && bike_confirmed &&
      (link.status != transport::Status::Ready || !telemetry->bike_valid || now - telemetry->bike_ms >= 1500 ||
       telemetry->state != 15)) {
    bike_confirmed = false;
    message("Bike lock state unconfirmed. Unlock the screen or retry lock.");
  }
  ui->set_pin_ready(!corrupt && !request.pending && attempts.allowed(now));
  static uint64_t shown_cooldown = 0;
  if (!corrupt && !attempts.allowed(now)) {
    const auto remaining = (attempts.retry_at - now + 999) / 1000;
    if (remaining != shown_cooldown) {
      shown_cooldown = remaining;
      char text[64];
      std::snprintf(text, sizeof(text), "Too many attempts. Try again in %u s.", unsigned(remaining));
      message(text);
    }
  }
  else if (!corrupt && shown_cooldown) {
    shown_cooldown = 0;
    message(entry_pattern() ? "Connect six dots, then lift your finger." : "Enter your six-digit PIN");
  }
  ui->set_lock_retry_available(locked && record.armed && !bike_confirmed && !request.pending);
  if (take_slint_wake()) {
    asleep = false;
    activity = now;
    clear_digits();
    set_slint_sleeping(false);
    ui->set_screen_asleep(false);
    board::brightness(ui->get_brightness());
  }
  const auto last = std::max(activity, slint_last_activity_ms());
  if (locked && !asleep && now - last >= uint64_t(record.timeout) * 1000) {
    asleep = true;
    local_authenticated = false;
    clear_digits();
    set_slint_sleeping(true);
    ui->set_screen_asleep(true);
    board::brightness(0);
  }
}

bool pairing_permitted() {
  return (!locked && mode == Mode::None) || (mode == Mode::Unlock && local_authenticated);
}

bool blocked() {
  return locked || mode != Mode::None || ble_security::prompt().open;
}

} // namespace security
