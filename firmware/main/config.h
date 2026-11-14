#pragma once

// Diagnostic UI telemetry at a target of 60 Hz. Keep disabled for normal builds.
// Generated values never replace controller telemetry used by security/tuning.
#ifndef WFD_TELEMETRY_TEST
  #define WFD_TELEMETRY_TEST 0
#endif
