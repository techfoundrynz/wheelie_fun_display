#include "ota_image.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)

esp_partition_t partition{4096};
esp_app_desc_t running{"waveshare_esp32s3_206"}, candidate = running;
bool fail_begin = false, fail_verify = false, fail_select = false;
bool gate = true, invalidate_at_verify = false;
int activated = 0, aborts = 0;

const esp_app_desc_t *esp_app_get_description() {
  return &running;
}

const esp_partition_t *esp_ota_get_next_update_partition(const void *) {
  return &partition;
}

esp_err_t esp_ota_begin(const esp_partition_t *, size_t, esp_ota_handle_t *handle) {
  if (fail_begin)
    return -1;
  *handle = 1;
  return ESP_OK;
}

esp_err_t esp_ota_write_with_offset(esp_ota_handle_t, const void *, size_t, uint32_t) {
  return ESP_OK;
}

esp_err_t esp_partition_read(const esp_partition_t *, size_t, void *bytes, size_t length) {
  std::memset(bytes, 255, length);
  return ESP_OK;
}

esp_err_t esp_ota_get_partition_description(const esp_partition_t *, esp_app_desc_t *desc) {
  *desc = candidate;
  return ESP_OK;
}

esp_err_t esp_ota_end(esp_ota_handle_t) {
  if (invalidate_at_verify)
    gate = false;
  return fail_verify ? -1 : ESP_OK;
}

esp_err_t esp_ota_set_boot_partition(const esp_partition_t *) {
  if (fail_select)
    return -1;
  ++activated;
  return ESP_OK;
}

esp_err_t esp_ota_abort(esp_ota_handle_t) {
  ++aborts;
  return ESP_OK;
}

int main() {
  ota_image::Image first, second;
  std::array<uint8_t, 8> bytes{};
  CHECK(!first.begin(0));
  CHECK(!first.begin(4097));
  CHECK(!ota_image::busy());
  fail_begin = true;
  CHECK(!first.begin(1024));
  CHECK(!ota_image::busy());
  fail_begin = false;
  CHECK(first.begin(1024));
  CHECK(ota_image::busy());
  CHECK(!second.begin(1024));
  CHECK(first.write(0, bytes));
  CHECK(!first.write(4090, bytes));
  CHECK(!first.read(4090, bytes));
  candidate = {"guition_jc4880p443c"};
  CHECK(!first.finish());
  CHECK(activated == 0);
  first.abort();
  CHECK(!ota_image::busy());
  candidate = running;
  first.set_commit_gate([](void *) { return gate; }, nullptr);
  for (int reason = 0; reason < 4; ++reason) {
    CHECK(first.begin(1024));
    gate = reason != 0;
    fail_verify = reason == 1;
    invalidate_at_verify = reason == 2;
    fail_select = reason == 3;
    CHECK(!first.finish());
    CHECK(activated == 0);
    first.abort();
    CHECK(!ota_image::busy());
  }
  gate = true;
  fail_verify = fail_select = invalidate_at_verify = false;
  {
    ota_image::Image abandoned;
    CHECK(abandoned.begin(512));
  }
  CHECK(!ota_image::busy());
  CHECK(second.begin(1024));
  CHECK(second.finish());
  CHECK(activated == 1);
  second.abort();
  CHECK(ota_image::busy());
  CHECK(!first.begin(1024));
  std::cout << "OTA exclusivity, board identity, validation and activation gates passed\n";
}
