#include "security.hpp"
#include "bike/lock_policy.hpp"
#include "bike/protocol.hpp"
#include "ble_proxy.hpp"
#include "ble_security.hpp"
#include "board.hpp"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "psa/crypto.h"
#include "slint_backend.hpp"
#include "transport.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

namespace security
{
namespace
{

struct Record {
  uint8_t version = 1;
  std::array<uint8_t, 16> salt{};
  std::array<uint8_t, 32> digest{};
  uint8_t armed = 0;
  uint8_t timeout = 30;
  uint8_t failures = 0;
};

Record record, candidate;
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
  ui->set_pin_mask("");
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

std::array<uint8_t, 32> hash(const Record &value, const std::string &pin) {
  std::array<uint8_t, 22> input{};
  std::copy(value.salt.begin(), value.salt.end(), input.begin());
  std::copy(pin.begin(), pin.end(), input.begin() + value.salt.size());
  std::array<uint8_t, 32> digest{};
  size_t digest_size = 0;
  ESP_ERROR_CHECK(psa_crypto_init() == PSA_SUCCESS ? ESP_OK : ESP_FAIL);
  const auto status =
      psa_hash_compute(PSA_ALG_SHA_256, input.data(), input.size(), digest.data(), digest.size(), &digest_size);
  ESP_ERROR_CHECK(status == PSA_SUCCESS && digest_size == digest.size() ? ESP_OK : ESP_FAIL);
  std::fill(input.begin(), input.end(), 0);
  return digest;
}

bool matches(const Record &value) {
  auto digest = hash(value, digits);
  uint8_t difference = 0;
  for (size_t i = 0; i < digest.size(); ++i)
    difference |= digest[i] ^ value.digest[i];
  std::fill(digest.begin(), digest.end(), 0);
  return difference == 0;
}

void open(Mode next) {
  cancel_slint_input();
  mode = next;
  clear_digits();
  activity = now_ms();
  ui->set_pin_open(true);
  ui->set_pin_heading(next == Mode::New ? "CREATE PIN" : next == Mode::Confirm ? "CONFIRM PIN" : "ENTER PIN");
  message(next == Mode::New       ? "Choose a six-digit PIN"
          : next == Mode::Confirm ? "Enter the new PIN again"
                                  : "Enter your six-digit PIN");
}

void close() {
  mode = Mode::None;
  clear_digits();
  candidate = {};
  ui->set_pin_open(false);
}

void finish_unlock() {
  if (record.armed) {
    auto next = record;
    next.armed = 0;
    if (!save(next)) {
      message("Could not save unlock. Enter PIN to retry.");
      return;
    }
    record = next;
  }
  locked = false;
  bike_confirmed = false;
  ui->set_screen_locked(false);
  ble_proxy::set_paused(false);
  close();
}

bool send_lock(bool desired) {
  const auto link = transport::snapshot();
  const auto now = now_ms();
  const auto &vehicle = bike::integration(link.integration);
  if (link.status == transport::Status::SecurityRequired && local_authenticated) {
    ble_security::retry();
    message("Pair Bluetooth, then enter your bike PIN again.");
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
  if (mode == Mode::None || corrupt || request.pending || !attempts.allowed(now) || !bike::valid_pin(digits))
    return;
  if (mode == Mode::New) {
    candidate = record;
    esp_fill_random(candidate.salt.data(), candidate.salt.size());
    candidate.digest = hash(candidate, digits);
    candidate_passkey = numeric_pin();
    open(Mode::Confirm);
    return;
  }
  if (mode == Mode::Confirm) {
    const bool correct = matches(candidate);
    clear_digits();
    if (!correct) {
      candidate = {};
      open(Mode::New);
      message("PINs did not match. Choose a PIN again.");
      return;
    }
    if (!save(candidate)) {
      message("Could not save PIN. Enter it again to retry.");
      return;
    }
    record = candidate;
    configured = true;
    ble_security::set_bike_pin(true, true, candidate_passkey, record.digest);
    candidate_passkey = 0;
    ui->set_pin_configured(true);
    ui->set_bike_lock_status("PIN saved. Keep your code somewhere safe.");
    close();
    return;
  }
  const auto entered_pin = numeric_pin();
  const bool correct = matches(record);
  clear_digits();
  attempts.result(correct, now);
  auto updated = record;
  updated.failures = std::min(attempts.failures, 5u);
  if (!save(updated)) {
    message("Could not save PIN attempt. Try again.");
    return;
  }
  record = updated;
  if (!correct) {
    message(attempts.allowed(now) ? "Incorrect PIN" : "Too many attempts. Wait 30 seconds.");
    return;
  }
  local_authenticated = true;
  ble_security::set_bike_pin(true, true, entered_pin, record.digest);
  if (mode == Mode::Change) {
    open(Mode::New);
  }
  else if (mode == Mode::Remove) {
    nvs_handle_t h;
    if (nvs_open("security", NVS_READWRITE, &h) != ESP_OK) {
      message("Could not remove PIN. Try again.");
      return;
    }
    const auto erased = nvs_erase_key(h, "pin");
    const auto committed = erased == ESP_OK ? nvs_commit(h) : erased;
    nvs_close(h);
    if (committed != ESP_OK) {
      message("Could not remove PIN. Try again.");
      return;
    }
    record = {};
    configured = false;
    ble_security::set_bike_pin(false, true, 0, {});
    ui->set_pin_configured(false);
    ui->set_bike_lock_status("PIN removed");
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

} // namespace

void init(Dashboard &dashboard, const bike::Telemetry &data) {
  ui = &dashboard;
  telemetry = &data;
  nvs_handle_t h;
  auto error = nvs_open("security", NVS_READONLY, &h);
  if (error == ESP_OK) {
    size_t length = sizeof(record);
    error = nvs_get_blob(h, "pin", &record, &length);
    configured = error == ESP_OK;
    corrupt = (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) ||
              (configured &&
               (length != sizeof(record) || record.version != 1 || record.armed > 1 || record.failures > 5 ||
                (record.timeout != 15 && record.timeout != 30 && record.timeout != 60 && record.timeout != 120)));
    nvs_close(h);
  }
  else if (error != ESP_ERR_NVS_NOT_FOUND)
    corrupt = true;
  ble_security::set_bike_pin(configured || corrupt, !configured && !corrupt, 0, record.digest);
  ui->set_pin_configured(configured);
  ui->set_bike_lock_status(configured ? "PIN configured. Lock only while the bike is stopped."
                                      : "Set a PIN to enable bike locking.");
  ui->set_lock_timeout(record.timeout);
  attempts.failures = record.failures;
  if (record.failures >= 5)
    attempts.retry_at = now_ms() + 30000;
  ui->on_pin_setup([] {
    if (!locked && mode == Mode::None)
      open(configured ? Mode::Change : Mode::New);
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
  ui->on_pin_submit(submit);
  ui->on_pin_cancel([] {
    if (!locked)
      close();
  });
  ui->on_pin_digit([](int digit) {
    activity = now_ms();
    if (mode != Mode::None && digit >= 0 && digit <= 9 && digits.size() < 6 && !request.pending &&
        attempts.allowed(activity)) {
      digits.push_back('0' + digit);
      ui->set_pin_mask(std::string(digits.size(), '*').c_str());
    }
  });
  ui->on_pin_backspace([] {
    if (!digits.empty()) {
      digits.back() = '\0';
      digits.pop_back();
    }
    ui->set_pin_mask(std::string(digits.size(), '*').c_str());
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
      message("PIN storage could not be read. Service required.");
  }
  activity = now_ms();
}

void tick(uint64_t now) {
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
    message(saved ? "Bike reports disabled. Enter PIN to unlock." : "Bike disabled; could not save lock state.");
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
        message("Bike reports disabled. Enter PIN to unlock.");
      }
      else
        finish_unlock();
    }
  }
  if (locked && record.armed && !request.pending && bike_confirmed &&
      (link.status != transport::Status::Ready || !telemetry->bike_valid || now - telemetry->bike_ms >= 1500 ||
       telemetry->state != 15)) {
    bike_confirmed = false;
    message("Bike lock state unconfirmed. Enter PIN or retry lock.");
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
    message("Enter your six-digit PIN");
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
