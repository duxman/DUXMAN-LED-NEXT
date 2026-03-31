#pragma once

#ifndef DUX_PROFILE_NAME
#define DUX_PROFILE_NAME "custom"
#endif

#ifndef DUX_LED_PIN
#define DUX_LED_PIN 2
#endif

#ifndef DUX_LED_COUNT
#define DUX_LED_COUNT 60
#endif

namespace BuildProfile {
static constexpr const char *kName = DUX_PROFILE_NAME;
static constexpr int kLedPin = DUX_LED_PIN;
static constexpr int kLedCount = DUX_LED_COUNT;
} // namespace BuildProfile