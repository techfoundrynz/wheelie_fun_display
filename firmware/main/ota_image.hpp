#pragma once
#include "bike/vesc_upload.hpp"
#include "esp_ota_ops.h"

namespace ota_image
{

bool busy();

class Image final : public bike::UpdateStorage {
  const esp_partition_t *partition = nullptr;
  esp_ota_handle_t handle = 0;
  bool held = false;
  bool (*commit_gate)(void *) = nullptr;
  void *commit_context = nullptr;

public:

  void set_commit_gate(bool (*gate)(void *), void *context) {
    commit_gate = gate;
    commit_context = context;
  }

  ~Image() override {
    abort();
  }

  size_t capacity() const override;

  bool begin(size_t size) override;

  bool write(size_t offset, std::span<const uint8_t> bytes) override;

  bool read(size_t offset, std::span<uint8_t> bytes) override;

  bool finish() override;

  void abort() override;

};

} // namespace ota_image
