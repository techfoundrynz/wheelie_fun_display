#pragma once
#include <array>
#include <string>

namespace network
{

struct AccessPoint {
  std::string ssid;
  int rssi = 0;
  bool secured = true;
};

struct Snapshot {
  bool busy = false, connected = false, available = false, updating = false, reboot_ready = false;
  bool cancellable = false;
  int progress = 0, count = 0;
  std::string status = "Wi-Fi is off", ssid, ip, version;
  std::array<AccessPoint, 12> networks{};
};

void start();

Snapshot snapshot();

void scan();

void connect(const std::string &ssid, const std::string &password);

void connect_saved();

void forget();

void check_release();

void install();

void cancel();

void reboot();

} // namespace network
