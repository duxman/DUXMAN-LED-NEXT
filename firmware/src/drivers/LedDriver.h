#pragma once

#include <Arduino.h>

class LedDriver {
public:
  void begin();
  void setAll(uint8_t level);
  void show();

private:
  uint8_t level_ = 0;
};
