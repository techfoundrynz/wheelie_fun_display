#pragma once
#include "bike/telemetry.hpp"
#include "dashboard.h"
#include <cstdint>

namespace security
{

void init(Dashboard &ui, const bike::Telemetry &telemetry);

void tick(uint64_t now);

bool blocked();

bool pairing_permitted();

} // namespace security
