/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/CurrentLedDriver.h
 * Last commit: 2c35a63 - 2026-04-28
 */

#pragma once

#include "core/BuildProfile.h"

#if DUX_LED_BACKEND == DUX_LED_BACKEND_NEOPIXELBUS
#include "drivers/LedDriverNeoPixelBus.h"
using DefaultLedDriver = LedDriverNeoPixelBus;
#elif DUX_LED_BACKEND == DUX_LED_BACKEND_FASTLED
#include "drivers/LedDriverFastLed.h"
using DefaultLedDriver = LedDriverFastLed;
#elif DUX_LED_BACKEND == DUX_LED_BACKEND_DIGITAL
#include "drivers/LedDriverDigital.h"
using DefaultLedDriver = LedDriverDigital;
#else
#error Unsupported DUX_LED_BACKEND value
#endif