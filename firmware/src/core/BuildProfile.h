/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/BuildProfile.h
 * Last commit: 2c35a63 - 2026-04-28
 */

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
#define DUX_FW_VERSION "0.3.11-beta"
#endif

#ifndef DUX_FW_DATE
#define DUX_FW_DATE "2026-04-30"
#endif

#ifndef DUX_FW_BRANCH
#define DUX_FW_BRANCH "main"
#endif

#ifndef DUX_LED_BACKEND
#define DUX_LED_BACKEND 1
#endif

#define DUX_LED_BACKEND_NEOPIXELBUS 1
#define DUX_LED_BACKEND_FASTLED 2
#define DUX_LED_BACKEND_DIGITAL 3

namespace BuildProfile {
static constexpr const char *kName = DUX_PROFILE_NAME;
static constexpr int kLedPin = DUX_LED_PIN;
static constexpr int kLedCount = DUX_LED_COUNT;
static constexpr int kLedBackend = DUX_LED_BACKEND;
static constexpr const char *kFwVersion = DUX_FW_VERSION;
static constexpr const char *kFwDate = DUX_FW_DATE;
static constexpr const char *kFwBranch = DUX_FW_BRANCH;
static constexpr const char *kRepoUrl = "https://github.com/duxman/DUXMAN-LED-NEXT";
static constexpr const char *kRepoGitUrl = "https://github.com/duxman/DUXMAN-LED-NEXT.git";
#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
static constexpr const char *kLedBackendName = "neopixelbus";
#elif DUX_LED_BACKEND == DUX_LED_BACKEND_FASTLED
static constexpr const char *kLedBackendName = "fastled";
#elif DUX_LED_BACKEND == DUX_LED_BACKEND_DIGITAL
static constexpr const char *kLedBackendName = "digital";
#else
static constexpr const char *kLedBackendName = "unknown";
#endif
} // namespace BuildProfile