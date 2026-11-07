#pragma once
#include "bike/telemetry.hpp"
#include "dashboard.h"
#include <cstdint>
#include <span>

namespace tuning
{

void init(Dashboard &ui, const bike::Telemetry &telemetry);

void tick(uint64_t now);

void receive(std::span<const uint8_t> payload);

void reset();

void cancel();

} // namespace tuning
