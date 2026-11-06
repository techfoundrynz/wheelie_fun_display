#pragma once

namespace bike
{

enum class LinkStatus {
  Disconnected,
  Scanning,
  Connecting,
  Discovering,
  Ready,
  Retry,
  Paused,
  SecurityRequired,
  Error
};

} // namespace bike
