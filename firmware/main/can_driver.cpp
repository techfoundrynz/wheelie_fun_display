#include "can_driver.hpp"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <array>
#include <new>

namespace transport
{

struct CanDriver::State {
  struct TxSlot {
    twai_frame_t frame{};
    std::array<uint8_t, 8> data{};
    bool busy = false;
    uint64_t submitted = 0;
  };

  struct Completion {
    uint8_t slot;
    bool success;
  };

  static constexpr size_t tx_count = 8, rx_count = 64;
  twai_node_handle_t node = nullptr;
  std::array<TxSlot, tx_count> tx{};
  std::array<uint8_t, tx_count * sizeof(Completion)> completion_storage{};
  std::array<uint8_t, rx_count * sizeof(bike::CanFrame)> rx_storage{};
  std::array<uint8_t, 1> fault_storage{};
  StaticQueue_t completion_control{}, rx_control{}, fault_control{};
  QueueHandle_t completions = nullptr, received = nullptr, faults = nullptr;
  bool fault = false;

  static void signal_fault(State &s, BaseType_t &awakened);

  static bool tx_done(twai_node_handle_t, const twai_tx_done_event_data_t *event, void *context);

  static bool rx_done(twai_node_handle_t node, const twai_rx_done_event_data_t *, void *context);

  void complete(const Completion &completion) {
    if (completion.slot >= tx_count || !tx[completion.slot].busy) {
      fault = true;
      return;
    }
    tx[completion.slot].busy = false;
    if (!completion.success)
      fault = true;
  }

  void drain() {
    Completion completion{};
    while (xQueueReceive(completions, &completion, 0) == pdTRUE)
      complete(completion);
    uint8_t failed;
    if (xQueueReceive(faults, &failed, 0) == pdTRUE)
      fault = true;
  }

  TxSlot *available() {
    for (auto &slot : tx)
      if (!slot.busy)
        return &slot;
    return nullptr;
  }
};

void IRAM_ATTR CanDriver::State::signal_fault(State &s, BaseType_t &awakened) {
  const uint8_t failed = 1;
  // Overwrite latches faults even if a prior notification is still pending.
  // FreeRTOS's ISR primitives work on targets without lock-free C++ atomics;
  // callbacks never read or change the worker's busy/fault flags.
  xQueueOverwriteFromISR(s.faults, &failed, &awakened);
}

bool IRAM_ATTR CanDriver::State::tx_done(twai_node_handle_t, const twai_tx_done_event_data_t *event, void *context) {
  auto &s = *static_cast<State *>(context);
  BaseType_t awakened = pdFALSE;
  for (size_t index = 0; index < tx_count; ++index) {
    if (&s.tx[index].frame != event->done_tx_frame)
      continue;
    const Completion completion{uint8_t(index), event->is_tx_success};
    if (xQueueSendFromISR(s.completions, &completion, &awakened) != pdTRUE)
      signal_fault(s, awakened);
    return awakened == pdTRUE;
  }
  signal_fault(s, awakened);
  return awakened == pdTRUE;
}

bool IRAM_ATTR CanDriver::State::rx_done(twai_node_handle_t node, const twai_rx_done_event_data_t *, void *context) {
  auto &s = *static_cast<State *>(context);
  bike::CanFrame copy;
  twai_frame_t frame{};
  frame.buffer = copy.data.data();
  frame.buffer_len = copy.data.size();
  BaseType_t awakened = pdFALSE;
  if (twai_node_receive_from_isr(node, &frame) != ESP_OK) {
    signal_fault(s, awakened);
    return awakened == pdTRUE;
  }
  // The protocol uses Classic extended data frames only. Do not truncate FD
  // payloads into something the packet reassembler could mistake for a frame.
  if (!frame.header.ide || frame.header.rtr || frame.header.fdf || frame.header.dlc > 8)
    return false;
  copy.id = frame.header.id;
  copy.size = frame.header.dlc;
  if (xQueueSendFromISR(s.received, &copy, &awakened) != pdTRUE)
    signal_fault(s, awakened);
  return awakened == pdTRUE;
}

esp_err_t CanDriver::open(int tx, int rx, uint32_t bitrate) {
  close();
  auto *allocation = heap_caps_malloc(sizeof(State), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!allocation)
    return ESP_ERR_NO_MEM;
  state = new (allocation) State;
  state->completions = xQueueCreateStatic(State::tx_count, sizeof(State::Completion), state->completion_storage.data(),
                                          &state->completion_control);
  state->received =
      xQueueCreateStatic(State::rx_count, sizeof(bike::CanFrame), state->rx_storage.data(), &state->rx_control);
  state->faults = xQueueCreateStatic(1, sizeof(uint8_t), state->fault_storage.data(), &state->fault_control);
  if (!state->completions || !state->received || !state->faults) {
    close();
    return ESP_ERR_NO_MEM;
  }
  twai_onchip_node_config_t config{};
  config.io_cfg.tx = gpio_num_t(tx);
  config.io_cfg.rx = gpio_num_t(rx);
  config.io_cfg.quanta_clk_out = GPIO_NUM_NC;
  config.io_cfg.bus_off_indicator = GPIO_NUM_NC;
  config.bit_timing.bitrate = bitrate;
  config.fail_retry_cnt = -1;
  config.tx_queue_depth = State::tx_count;
  config.flags.no_receive_rtr = true;
  auto result = twai_new_node_onchip(&config, &state->node);
  if (result == ESP_OK) {
    twai_event_callbacks_t callbacks{};
    callbacks.on_tx_done = State::tx_done;
    callbacks.on_rx_done = State::rx_done;
    result = twai_node_register_event_callbacks(state->node, &callbacks, state);
  }
  if (result == ESP_OK) {
    twai_mask_filter_config_t filter{};
    filter.is_ext = true;
    filter.no_fd = true;
    result = twai_node_config_mask_filter(state->node, 0, &filter);
  }
  if (result == ESP_OK)
    result = twai_node_enable(state->node);
  if (result != ESP_OK)
    close();
  return result;
}

void CanDriver::close() {
  if (!state)
    return;
  if (state->node) {
    // Disabling aborts transmission; deleting unregisters/frees the ISR before
    // any application frame, callback context or queue storage is reclaimed.
    twai_node_disable(state->node);
    ESP_ERROR_CHECK(twai_node_delete(state->node));
  }
  if (state->completions)
    vQueueDelete(state->completions);
  if (state->faults)
    vQueueDelete(state->faults);
  if (state->received)
    vQueueDelete(state->received);
  state->~State();
  heap_caps_free(state);
  state = nullptr;
}

bool CanDriver::send(const bike::CanFrame &source) {
  if (!state || !bike::valid_can_data_frame(source) || !healthy())
    return false;
  auto *slot = state->available();
  if (!slot) {
    State::Completion completion{};
    if (xQueueReceive(state->completions, &completion, pdMS_TO_TICKS(20)) != pdTRUE)
      return false;
    state->complete(completion);
    if (!healthy())
      return false;
    slot = state->available();
    if (!slot)
      return false;
  }
  slot->frame = {};
  slot->frame.header.id = source.id;
  slot->frame.header.ide = source.extended;
  slot->frame.header.dlc = source.size;
  slot->data = source.data;
  slot->frame.buffer = slot->data.data();
  slot->frame.buffer_len = source.size;
  slot->submitted = esp_timer_get_time() / 1000;
  slot->busy = true;
  if (twai_node_transmit(state->node, &slot->frame, 20) != ESP_OK) {
    slot->busy = false;
    return false;
  }
  return true;
}

bool CanDriver::receive(bike::CanFrame &frame) {
  return state && xQueueReceive(state->received, &frame, 0) == pdTRUE;
}

bool CanDriver::healthy() {
  if (!state)
    return false;
  state->drain();
  if (state->fault)
    return false;
  twai_node_status_t status{};
  if (twai_node_get_info(state->node, &status, nullptr) != ESP_OK || status.state == TWAI_ERROR_BUS_OFF)
    return false;
  const uint64_t now = esp_timer_get_time() / 1000;
  for (const auto &slot : state->tx)
    if (slot.busy && now - slot.submitted >= 250)
      return false;
  return true;
}

} // namespace transport
