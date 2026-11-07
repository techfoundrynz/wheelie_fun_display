#pragma once

namespace bike
{

// RSSI is approximate received power, not a throughput or distance estimate.
constexpr int ble_signal_level(int rssi) {
  return rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
}

} // namespace bike
