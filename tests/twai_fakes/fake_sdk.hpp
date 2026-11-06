#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

#define IRAM_ATTR
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_NO_MEM = 1;
#define ESP_ERROR_CHECK(x)                                                                                             \
  do {                                                                                                                 \
    if ((x) != ESP_OK)                                                                                                 \
      std::abort();                                                                                                    \
  } while (0)
using BaseType_t = int;
constexpr int pdFALSE = 0, pdTRUE = 1;
#define pdMS_TO_TICKS(x) (x)
constexpr unsigned MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2;
using gpio_num_t = int;
constexpr int GPIO_NUM_NC = -1;

struct StaticQueue_t {
  size_t capacity = 0, size = 0;
  std::deque<std::vector<uint8_t>> items;
};

using QueueHandle_t = StaticQueue_t *;

inline QueueHandle_t xQueueCreateStatic(size_t count, size_t size, uint8_t *, StaticQueue_t *q) {
  q->capacity = count;
  q->size = size;
  return q;
}

inline BaseType_t xQueueSend(QueueHandle_t q, const void *v, unsigned) {
  if (q->items.size() == q->capacity)
    return pdFALSE;
  auto *bytes = static_cast<const uint8_t *>(v);
  q->items.emplace_back(bytes, bytes + q->size);
  return pdTRUE;
}

inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *v, BaseType_t *) {
  return xQueueSend(q, v, 0);
}

inline BaseType_t xQueueOverwriteFromISR(QueueHandle_t q, const void *v, BaseType_t *awakened) {
  assert(q->capacity == 1);
  q->items.clear();
  return xQueueSendFromISR(q, v, awakened);
}

inline BaseType_t xQueueReceive(QueueHandle_t q, void *v, unsigned) {
  if (q->items.empty())
    return pdFALSE;
  std::memcpy(v, q->items.front().data(), q->size);
  q->items.pop_front();
  return pdTRUE;
}

inline void vQueueDelete(QueueHandle_t q) {
  q->items.clear();
}

inline uint64_t clock_us = 1000000;
inline int allocations = 0;
inline bool fail_allocation = false;

inline void *heap_caps_malloc(size_t size, unsigned caps) {
  assert(caps == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (fail_allocation)
    return nullptr;
  ++allocations;
  return std::malloc(size);
}

inline void heap_caps_free(void *p) {
  --allocations;
  std::free(p);
}

inline int64_t esp_timer_get_time() {
  return clock_us;
}

struct twai_frame_header_t {
  uint32_t id = 0;
  uint16_t dlc = 0;
  bool ide = false, rtr = false, fdf = false;
};

struct twai_frame_t {
  twai_frame_header_t header{};
  uint8_t *buffer = nullptr;
  size_t buffer_len = 0;
};

struct twai_tx_done_event_data_t {
  bool is_tx_success;
  const twai_frame_t *done_tx_frame;
};

struct twai_rx_done_event_data_t {};
struct Node;
using twai_node_handle_t = Node *;

struct twai_event_callbacks_t {
  bool (*on_tx_done)(Node *, const twai_tx_done_event_data_t *, void *) = nullptr;
  bool (*on_rx_done)(Node *, const twai_rx_done_event_data_t *, void *) = nullptr;
};

struct twai_onchip_node_config_t {
  struct {
    int tx, rx, quanta_clk_out, bus_off_indicator;
  } io_cfg{};

  struct {
    uint32_t bitrate;
  } bit_timing{};

  int fail_retry_cnt = 0;
  unsigned tx_queue_depth = 0;

  struct {
    bool no_receive_rtr = false;
  } flags;
};

struct twai_mask_filter_config_t {
  bool is_ext = false, no_fd = false;
};

constexpr int TWAI_ERROR_ACTIVE = 0, TWAI_ERROR_BUS_OFF = 2;

struct twai_node_status_t {
  int state = TWAI_ERROR_ACTIVE;
};

struct Node {
  twai_event_callbacks_t cbs{};
  void *context = nullptr;
  bool enabled = false, bus_off = false;
  std::vector<const twai_frame_t *> pending;
  twai_frame_header_t rx_header{};
  uint8_t rx_data[8]{};
};

inline Node *node = nullptr;
inline int fail_step = 0, create_count = 0, delete_count = 0;
inline bool fail_transmit = false, complete_inside_transmit = false;

inline esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t *c, Node **out) {
  assert(c->tx_queue_depth == 8 && c->fail_retry_cnt == -1 && c->bit_timing.bitrate == 500000);
  assert(c->io_cfg.quanta_clk_out == -1 && c->io_cfg.bus_off_indicator == -1);
  if (fail_step == 1)
    return ESP_FAIL;
  *out = node = new Node;
  ++create_count;
  return ESP_OK;
}

inline esp_err_t twai_node_register_event_callbacks(Node *n, const twai_event_callbacks_t *c, void *ctx) {
  if (fail_step == 2)
    return ESP_FAIL;
  n->cbs = *c;
  n->context = ctx;
  return ESP_OK;
}

inline esp_err_t twai_node_config_mask_filter(Node *, int id, const twai_mask_filter_config_t *f) {
  assert(id == 0 && f->is_ext && f->no_fd);
  return fail_step == 3 ? ESP_FAIL : ESP_OK;
}

inline esp_err_t twai_node_enable(Node *n) {
  if (fail_step == 4)
    return ESP_FAIL;
  n->enabled = true;
  return ESP_OK;
}

inline esp_err_t twai_node_disable(Node *n) {
  n->enabled = false;
  return ESP_OK;
}

inline esp_err_t twai_node_delete(Node *n) {
  assert(!n->enabled);
  delete n;
  node = nullptr;
  ++delete_count;
  return ESP_OK;
}

inline esp_err_t twai_node_transmit(Node *n, const twai_frame_t *f, int timeout) {
  assert(timeout == 20 && n->enabled && f->buffer_len == f->header.dlc);
  if (fail_transmit)
    return ESP_FAIL;
  n->pending.push_back(f);
  if (complete_inside_transmit) {
    const twai_tx_done_event_data_t event{true, f};
    n->cbs.on_tx_done(n, &event, n->context);
    n->pending.pop_back();
  }
  return ESP_OK;
}

inline esp_err_t twai_node_get_info(Node *n, twai_node_status_t *s, void *) {
  s->state = n->bus_off ? TWAI_ERROR_BUS_OFF : TWAI_ERROR_ACTIVE;
  return ESP_OK;
}

inline esp_err_t twai_node_receive_from_isr(Node *n, twai_frame_t *f) {
  assert(f->buffer_len == 8);
  f->header = n->rx_header;
  std::memcpy(f->buffer, n->rx_data, 8);
  return ESP_OK;
}

inline void finish_tx(size_t index, bool success = true) {
  auto *f = node->pending.at(index);
  node->pending.erase(node->pending.begin() + index);
  twai_tx_done_event_data_t event{success, f};
  node->cbs.on_tx_done(node, &event, node->context);
}

inline void deliver_rx(uint32_t id, uint8_t size, uint8_t value, bool fd = false, bool remote = false,
                       bool extended = true) {
  node->rx_header = {id, size, extended, remote, fd};
  std::memset(node->rx_data, value, 8);
  node->cbs.on_rx_done(node, nullptr, node->context);
}
