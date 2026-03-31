#include "drivers/LedDriver.h"

void LedDriver::begin() {
  level_ = 0;
}

void LedDriver::setAll(uint8_t level) {
  level_ = level;
}

void LedDriver::show() {
  (void)level_;
  // Punto de extensión para FastLED/NeoPixelBus.
}
