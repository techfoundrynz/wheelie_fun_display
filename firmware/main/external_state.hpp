#pragma once
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include <memory>
#include <new>
#include <utility>

// Large protocol/state objects belong in PSRAM. RTOS controls, driver memory
// and task stacks are allocated separately with their required capabilities.
template <class T> struct ExternalStateDeleter {
  void operator()(T *value) const {
    if (value) {
      value->~T();
      heap_caps_free(value);
    }
  }
};
template <class T> using ExternalState = std::unique_ptr<T, ExternalStateDeleter<T>>;

template <class T, class... Args> ExternalState<T> make_external_state(Args &&...args) {
  void *storage = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  configASSERT(storage);
  return ExternalState<T>(new (storage) T(std::forward<Args>(args)...));
}
