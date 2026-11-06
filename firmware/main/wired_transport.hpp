#pragma once
#include "transport.hpp"

namespace transport
{

Interface &wired_backend(Kind kind);

bool wired_available(Kind kind);

void wired_proxy_ownership(bool owned, uint32_t epoch);

bool wired_send_proxy_fragment(std::span<const uint8_t> bytes, uint32_t epoch);

void wired_reset_proxy_link();

} // namespace transport
