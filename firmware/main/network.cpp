#include "network.hpp"
#include "bike/update_policy.hpp"
#include "ble_proxy.hpp"
#include "board.hpp"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_image_format.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ota_image.hpp"
#include "psa/crypto.h"
#include "transport.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <strings.h>

namespace network
{
namespace
{

constexpr const char *feed = "https://api.github.com/repos/techfoundrynz/wheelie_fun_display/releases/latest";
constexpr const char *asset_name = board::release_asset;
Snapshot state;
SemaphoreHandle_t mutex;
QueueHandle_t queue;
EventGroupHandle_t events;
constexpr EventBits_t got_ip = 1, lost_ip = 2;
enum class Command {
  Scan,
  Connect,
  Saved,
  Forget,
  Check,
  Install
};

struct Request {
  Command command;
  char ssid[33]{};
  char password[65]{};
};
enum class Phase {
  Idle,
  Downloading,
  Cancelled,
  Committing,
  Ready
};
std::atomic<Phase> phase{Phase::Idle};
std::string download_url, expected_digest;
size_t expected_size = 0;

void lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  xSemaphoreGive(mutex);
}

void status(const std::string &text) {
  lock();
  state.status = text;
  unlock();
}

void event(void *, esp_event_base_t base, int32_t id, void *data) {
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    const auto *ip = static_cast<ip_event_got_ip_t *>(data);
    char value[24];
    std::snprintf(value, sizeof(value), IPSTR, IP2STR(&ip->ip_info.ip));
    lock();
    state.connected = true;
    state.ip = value;
    unlock();
    xEventGroupSetBits(events, got_ip);
  }
  else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    lock();
    state.connected = false;
    state.ip.clear();
    if (!state.busy)
      state.status = "Wi-Fi disconnected";
    unlock();
    xEventGroupSetBits(events, lost_ip);
  }
}

struct Headers {
  std::string location;
};

esp_err_t header(esp_http_client_event_t *event) {
  auto *headers = static_cast<Headers *>(event->user_data);
  if (!headers)
    return ESP_OK;
  if (event->event_id == HTTP_EVENT_ON_HEADER && !strcasecmp(event->header_key, "Location") &&
      std::strlen(event->header_value) <= 4096)
    headers->location = event->header_value;
  return ESP_OK;
}

esp_http_client_handle_t open_https(std::string url) {
  for (int redirects = 0; redirects <= 5; ++redirects) {
    if (!bike::trusted_update_url(url)) {
      status("Rejected an untrusted or non-HTTPS URL");
      return nullptr;
    }
    Headers headers;
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 10000;
    config.disable_auto_redirect = true;
    config.event_handler = header;
    config.user_data = &headers;
    config.buffer_size = 2048;
    config.user_agent = "BikeDisplay/0.1";
    auto client = esp_http_client_init(&config);
    if (!client)
      return nullptr;
    if (esp_http_client_open(client, 0) != ESP_OK || esp_http_client_fetch_headers(client) < 0) {
      esp_http_client_cleanup(client);
      status("HTTPS connection failed");
      return nullptr;
    }
    const int code = esp_http_client_get_status_code(client);
    if (code == 200) {
      // The callback's stack context must not outlive this function.
      esp_http_client_set_user_data(client, nullptr);
      return client;
    }
    const auto location = headers.location;
    esp_http_client_cleanup(client);
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      url = location;
      continue;
    }
    status(code == 404 ? "No public release found in techfoundrynz/wheelie_fun_display"
                       : "GitHub HTTP error " + std::to_string(code));
    return nullptr;
  }
  status("Too many HTTPS redirects");
  return nullptr;
}

bool clock_ready() {
  if (std::time(nullptr) > 1735689600)
    return true;
  status("Synchronizing clock for HTTPS...");
  static bool initialized = false;
  if (!initialized) {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&config) != ESP_OK)
      return false;
    initialized = true;
  }
  if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(20000)) != ESP_OK) {
    status("Clock synchronization failed; try again");
    return false;
  }
  return true;
}

std::string json_text(cJSON *object, const char *key) {
  auto value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsString(value) && value->valuestring ? value->valuestring : "";
}

void check() {
  download_url.clear();
  expected_digest.clear();
  expected_size = 0;
  lock();
  state.available = false;
  state.version.clear();
  unlock();
  if (!snapshot().connected) {
    status("Connect to Wi-Fi first");
    return;
  }
  if (!clock_ready())
    return;
  status("Checking GitHub Releases...");
  auto client = open_https(feed);
  if (!client)
    return;
  std::string body;
  body.reserve(8192);
  char buffer[1024];
  int n;
  bool ok = true;
  while ((n = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
    if (body.size() + n > 65536) {
      ok = false;
      break;
    }
    body.append(buffer, n);
  }
  ok &= n >= 0 && esp_http_client_is_complete_data_received(client);
  esp_http_client_cleanup(client);
  if (!ok) {
    status("Release response incomplete or too large");
    return;
  }
  auto json = cJSON_ParseWithLength(body.data(), body.size());
  if (!json) {
    status("Invalid release metadata");
    return;
  }
  const auto version = json_text(json, "tag_name");
  cJSON *asset = nullptr;
  cJSON_ArrayForEach(asset, cJSON_GetObjectItemCaseSensitive(json, "assets")) {
    if (json_text(asset, "name") != asset_name)
      continue;
    download_url = json_text(asset, "browser_download_url");
    expected_digest = json_text(asset, "digest");
    const auto size = cJSON_GetObjectItemCaseSensitive(asset, "size");
    expected_size = cJSON_IsNumber(size) && size->valuedouble > 0 ? size_t(size->valuedouble) : 0;
    break;
  }
  cJSON_Delete(json);
  const auto partition = esp_ota_get_next_update_partition(nullptr);
  if (!asset || !bike::trusted_update_url(download_url) || !bike::release_digest(expected_digest) || !expected_size ||
      !partition || expected_size > partition->size) {
    status(std::string("Release needs ") + asset_name + " with a GitHub SHA256 digest");
    return;
  }
  const std::string current = esp_app_get_description()->version;
  const auto normalized = version.starts_with('v') ? version.substr(1) : version;
  lock();
  state.version = version;
  state.available = normalized != current;
  unlock();
  status(normalized == current ? "Already running the latest published version" : "Update available: " + version);
}

void update() {
  if (!snapshot().connected || !snapshot().available) {
    status("Check for an update while connected first");
    return;
  }
  if (!ble_proxy::begin_maintenance()) {
    status("Disconnect the phone from the VESC proxy before updating");
    return;
  }
  phase.store(Phase::Downloading);
  lock();
  state.updating = true;
  state.progress = 0;
  unlock();
  transport::set_paused(true);
  status("Downloading and verifying firmware...");
  vTaskDelay(pdMS_TO_TICKS(250));
  auto client = open_https(download_url);
  ota_image::Image image;
  bool begun = false, success = false;
  const auto target = esp_ota_get_next_update_partition(nullptr);
  psa_hash_operation_t hash = PSA_HASH_OPERATION_INIT;
  const bool hash_ready = psa_crypto_init() == PSA_SUCCESS && psa_hash_setup(&hash, PSA_ALG_SHA_256) == PSA_SUCCESS;
  size_t total = 0;
  uint8_t buffer[2048];
  if (hash_ready && client && target && image.begin(expected_size)) {
    begun = true;
    while (phase.load() == Phase::Downloading) {
      const int n = esp_http_client_read(client, reinterpret_cast<char *>(buffer), sizeof(buffer));
      if (n < 0) {
        status("Firmware download failed");
        break;
      }
      if (!n) {
        success = esp_http_client_is_complete_data_received(client) && total == expected_size;
        break;
      }
      if (total + size_t(n) > expected_size || !image.write(total, std::span(buffer).first(n))) {
        status("Invalid firmware or flash write failure");
        break;
      }
      if (psa_hash_update(&hash, buffer, n) != PSA_SUCCESS) {
        status("Firmware verification failed");
        break;
      }
      total += n;
      lock();
      state.progress = int(total * 100 / expected_size);
      unlock();
    }
  }
  else
    status("Unable to start OTA download");
  if (client)
    esp_http_client_cleanup(client);
  uint8_t result[32]{};
  if (success)
    status("Verifying downloaded firmware...");
  size_t hash_size = 0;
  success &= hash_ready && psa_hash_finish(&hash, result, sizeof(result), &hash_size) == PSA_SUCCESS &&
             hash_size == sizeof(result);
  psa_hash_abort(&hash);
  const auto expected_hash = bike::release_digest(expected_digest);
  success &= expected_hash.has_value() && std::equal(expected_hash->begin(), expected_hash->end(), result);
  esp_app_desc_t desc{};
  success &= target && esp_ota_get_partition_description(target, &desc) == ESP_OK &&
             std::strncmp(desc.project_name, esp_app_get_description()->project_name, sizeof(desc.project_name)) == 0;
  Phase expected = Phase::Downloading;
  if (success && phase.compare_exchange_strong(expected, Phase::Committing)) {
    success = image.finish();
    if (!success)
      image.abort();
    begun = false;
    if (success) {
      phase.store(Phase::Ready);
      lock();
      state.reboot_ready = true;
      state.progress = 100;
      unlock();
      status("Verified update ready. Restart to install.");
    }
  }
  else
    success = false;
  if (begun)
    image.abort();
  if (!success) {
    status(phase.load() == Phase::Cancelled ? "Update cancelled; current firmware unchanged"
                                            : "Update failed validation; current firmware unchanged");
    phase.store(Phase::Idle);
    transport::set_paused(false);
  }
  lock();
  state.updating = false;
  unlock();
}

void connect_wifi(const Request &request, bool saved) {
  wifi_config_t config{};
  if (saved) {
    esp_wifi_get_config(WIFI_IF_STA, &config);
    if (!config.sta.ssid[0]) {
      status("No saved Wi-Fi network");
      return;
    }
  }
  else {
    std::memcpy(config.sta.ssid, request.ssid, std::strlen(request.ssid));
    std::memcpy(config.sta.password, request.password, std::strlen(request.password));
    config.sta.pmf_cfg.capable = true;
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK) {
      status("Wi-Fi configuration rejected");
      return;
    }
  }
  esp_wifi_disconnect();
  lock();
  state.ssid.assign(reinterpret_cast<char *>(config.sta.ssid), strnlen(reinterpret_cast<char *>(config.sta.ssid), 32));
  unlock();
  std::memset(config.sta.password, 0, sizeof(config.sta.password));
  for (int attempt = 0; attempt < 3; ++attempt) {
    status("Connecting to Wi-Fi...");
    xEventGroupClearBits(events, got_ip | lost_ip);
    if (esp_wifi_connect() == ESP_OK &&
        (xEventGroupWaitBits(events, got_ip, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000)) & got_ip)) {
      status("Wi-Fi connected: " + snapshot().ip);
      return;
    }
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  status("Could not connect. Check password and signal.");
}

void worker(void *) {
  Request request;
  for (;;) {
    if (xQueueReceive(queue, &request, portMAX_DELAY) != pdTRUE)
      continue;
    switch (request.command) {
    case Command::Scan: {
      status("Scanning Wi-Fi networks...");
      if (esp_wifi_scan_start(nullptr, true) == ESP_OK) {
        wifi_ap_record_t records[24]{};
        uint16_t count = 24;
        if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
          lock();
          state.count = 0;
          for (int i = 0; i < count && state.count < 12; ++i) {
            std::string ssid(reinterpret_cast<char *>(records[i].ssid),
                             strnlen(reinterpret_cast<char *>(records[i].ssid), 32));
            if (ssid.empty())
              continue;
            bool duplicate = false;
            for (int j = 0; j < state.count; ++j)
              duplicate |= state.networks[j].ssid == ssid;
            if (!duplicate)
              state.networks[state.count++] = {ssid, records[i].rssi, records[i].authmode != WIFI_AUTH_OPEN};
          }
          unlock();
          status("Choose a network or enter a hidden SSID");
        }
      }
      else
        status("Wi-Fi scan failed");
      break;
    }
    case Command::Connect:
      connect_wifi(request, false);
      break;
    case Command::Saved:
      connect_wifi(request, true);
      break;
    case Command::Forget: {
      esp_wifi_disconnect();
      wifi_config_t empty{};
      esp_wifi_set_config(WIFI_IF_STA, &empty);
      lock();
      state.ssid.clear();
      state.connected = false;
      state.available = false;
      unlock();
      status("Saved network forgotten");
      break;
    }
    case Command::Check:
      check();
      break;
    case Command::Install:
      update();
      break;
    }
    std::memset(request.password, 0, sizeof(request.password));
    lock();
    state.busy = false;
    unlock();
  }
}

void submit(Request request) {
  lock();
  if (state.busy || state.reboot_ready) {
    unlock();
    return;
  }
  state.busy = true;
  unlock();
  if (xQueueSend(queue, &request, 0) != pdTRUE) {
    lock();
    state.busy = false;
    unlock();
  }
  std::memset(request.password, 0, sizeof(request.password));
}

} // namespace

void start() {
  mutex = xSemaphoreCreateMutex();
  queue = xQueueCreate(2, sizeof(Request));
  events = xEventGroupCreate();
  configASSERT(mutex && queue && events);
  ESP_ERROR_CHECK(esp_netif_init());
  const auto error = esp_event_loop_create_default();
  if (error != ESP_ERR_INVALID_STATE)
    ESP_ERROR_CHECK(error);
  configASSERT(esp_netif_create_default_wifi_sta());
  wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&config));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, nullptr));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  wifi_config_t saved{};
  esp_wifi_get_config(WIFI_IF_STA, &saved);
  state.ssid.assign(reinterpret_cast<char *>(saved.sta.ssid), strnlen(reinterpret_cast<char *>(saved.sta.ssid), 32));
  std::memset(saved.sta.password, 0, sizeof(saved.sta.password));
  state.status = "Scan networks or reconnect to the saved network";
  configASSERT(xTaskCreate(worker, "maintenance", 12288, nullptr, 4, nullptr) == pdPASS);
}

Snapshot snapshot() {
  lock();
  auto copy = state;
  copy.cancellable = phase.load() == Phase::Downloading;
  unlock();
  return copy;
}

void scan() {
  submit({Command::Scan});
}

void connect(const std::string &ssid, const std::string &password) {
  if (ssid.empty() || ssid.size() > 32 || password.size() > 64) {
    status("SSID or password is too long");
    return;
  }
  Request r{Command::Connect};
  std::memcpy(r.ssid, ssid.data(), ssid.size());
  std::memcpy(r.password, password.data(), password.size());
  submit(r);
  std::memset(r.password, 0, sizeof(r.password));
}

void connect_saved() {
  submit({Command::Saved});
}

void forget() {
  submit({Command::Forget});
}

void check_release() {
  submit({Command::Check});
}

void install() {
  submit({Command::Install});
}

void cancel() {
  Phase expected = Phase::Downloading;
  phase.compare_exchange_strong(expected, Phase::Cancelled);
}

void reboot() {
  if (snapshot().reboot_ready)
    esp_restart();
}

} // namespace network
