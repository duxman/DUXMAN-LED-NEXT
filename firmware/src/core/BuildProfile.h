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

#ifndef DUX_FW_VERSION
#define DUX_FW_VERSION "0.2.0"
#endif

#ifndef DUX_FW_DATE
#define DUX_FW_DATE "2026-03-31"
#endif

#ifndef DUX_FW_BRANCH
#define DUX_FW_BRANCH "main"
#endif

namespace BuildProfile {
static constexpr const char *kName = DUX_PROFILE_NAME;
static constexpr int kLedPin = DUX_LED_PIN;
static constexpr int kLedCount = DUX_LED_COUNT;
static constexpr const char *kFwVersion = DUX_FW_VERSION;
static constexpr const char *kFwDate = DUX_FW_DATE;
static constexpr const char *kFwBranch = DUX_FW_BRANCH;
static constexpr const char *kRepoUrl = "https://github.com/duxman/DUXMAN-LED-NEXT";
static constexpr const char *kRepoGitUrl = "https://github.com/duxman/DUXMAN-LED-NEXT.git";
} // namespace BuildProfile