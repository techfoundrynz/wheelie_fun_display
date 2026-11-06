#pragma once
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstdint>

// FreeRTOS's dynamic queues consume internal RAM regardless of the ordinary
// malloc-to-PSRAM threshold. Keep large, task-only packet payloads in PSRAM;
// queue control blocks, task stacks and display DMA buffers stay internal.
// The supplied control block and returned queue have application lifetime.
template <class Packet> QueueHandle_t create_packet_queue(UBaseType_t count, StaticQueue_t &control) {
  configASSERT(count && count <= SIZE_MAX / sizeof(Packet));
  auto *storage = static_cast<uint8_t *>(heap_caps_malloc(count * sizeof(Packet), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  configASSERT(storage);
  auto queue = xQueueCreateStatic(count, sizeof(Packet), storage, &control);
  configASSERT(queue);
  return queue;
}
