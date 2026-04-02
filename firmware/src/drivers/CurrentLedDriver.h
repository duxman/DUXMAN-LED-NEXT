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