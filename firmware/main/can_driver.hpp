#pragma once
#include "bike/can_packet.hpp"
#include "esp_err.h"

namespace transport
{

// Owns asynchronous driver buffers until completion; protocol handling stays in
// the wired worker. ISR storage is allocated internally only while CAN is open.
class CanDriver {
  struct State;
  State *state = nullptr;

public:

  CanDriver() = default;

  CanDriver(const CanDriver &) = delete;

  CanDriver &operator=(const CanDriver &) = delete;

  ~CanDriver() {
    close();
  }

  esp_err_t open(int tx, int rx, uint32_t bitrate);

  void close();

  bool send(const bike::CanFrame &frame);

  bool receive(bike::CanFrame &frame);

  bool healthy();

};

} // namespace transport
