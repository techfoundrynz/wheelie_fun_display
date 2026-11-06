#pragma once
#include <cstdint>
#include <string_view>

namespace bike
{

struct ProxyAuthorization {
  bool pin_required = false, pin_known = false;
  uint32_t revision = 0, applied_revision = 0, connection_revision = 0;

  bool ready() const {
    return pin_known && revision == applied_revision;
  }

  bool allows(bool encrypted, bool authenticated) const {
    return ready() && connection_revision == revision && (!pin_required || (encrypted && authenticated));
  }
};

constexpr bool valid_proxy_name(std::string_view name) {
  if (name.empty() || name.size() > 29)
    return false;
  for (unsigned char c : name)
    if (c < 32 || c == 127)
      return false;
  return true;
}

} // namespace bike
