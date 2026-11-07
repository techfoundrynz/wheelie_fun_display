#include "ota_image.hpp"
#include "esp_app_desc.h"
#include <atomic>
#include <cstring>

namespace ota_image
{
namespace
{

std::atomic<int> ownership{0};

}

bool busy() {
  return ownership.load() != 0;
}

size_t Image::capacity() const {
  const auto *target = esp_ota_get_next_update_partition(nullptr);
  return target ? target->size : 0;
}

bool Image::begin(size_t size) {
  int expected = 0;
  if (held || !size || !ownership.compare_exchange_strong(expected, 1))
    return false;
  held = true;
  partition = esp_ota_get_next_update_partition(nullptr);
  if (!partition || size > partition->size || esp_ota_begin(partition, size, &handle) != ESP_OK) {
    abort();
    return false;
  }
  return true;
}

bool Image::write(size_t offset, std::span<const uint8_t> bytes) {
  return held && handle && offset <= partition->size && bytes.size() <= partition->size - offset &&
         esp_ota_write_with_offset(handle, bytes.data(), bytes.size(), offset) == ESP_OK;
}

bool Image::read(size_t offset, std::span<uint8_t> bytes) {
  return held && partition && offset <= partition->size && bytes.size() <= partition->size - offset &&
         esp_partition_read(partition, offset, bytes.data(), bytes.size()) == ESP_OK;
}

bool Image::finish() {
  esp_app_desc_t description{};
  if (!held || !handle || (commit_gate && !commit_gate(commit_context)) ||
      esp_ota_get_partition_description(partition, &description) != ESP_OK ||
      std::strncmp(description.project_name, esp_app_get_description()->project_name, sizeof(description.project_name)))
    return false;
  const auto result = esp_ota_end(handle);
  handle = 0;
  if (result != ESP_OK || (commit_gate && !commit_gate(commit_context)) ||
      esp_ota_set_boot_partition(partition) != ESP_OK) {
    abort();
    return false;
  }
  ownership.store(2); // Keep a verified candidate reserved until restart.
  held = false;
  return true;
}

void Image::abort() {
  if (handle)
    esp_ota_abort(handle);
  handle = 0;
  partition = nullptr;
  if (held)
    ownership.store(0);
  held = false;
}

} // namespace ota_image
