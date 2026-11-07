#pragma once
#include "esp_app_desc.h"
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using esp_ota_handle_t = uint32_t;
inline constexpr esp_err_t ESP_OK = 0;

struct esp_partition_t {
  size_t size;
};

const esp_partition_t *esp_ota_get_next_update_partition(const void *);

esp_err_t esp_ota_begin(const esp_partition_t *, size_t, esp_ota_handle_t *);

esp_err_t esp_ota_write_with_offset(esp_ota_handle_t, const void *, size_t, uint32_t);

esp_err_t esp_partition_read(const esp_partition_t *, size_t, void *, size_t);

esp_err_t esp_ota_get_partition_description(const esp_partition_t *, esp_app_desc_t *);

esp_err_t esp_ota_end(esp_ota_handle_t);

esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);

esp_err_t esp_ota_abort(esp_ota_handle_t);
