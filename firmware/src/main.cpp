#include <Arduino.h>

#include "api/ApiService.h"
#include "core/BuildProfile.h"
#include "core/CoreState.h"
#include "core/Config.h"
#include "drivers/CurrentLedDriver.h"
#include "effects/EffectEngine.h"
#include "services/StorageService.h"
#include "services/WifiService.h"

namespace {
CoreState state = CoreState::defaults();
NetworkConfig networkConfig = NetworkConfig::defaults();
GpioConfig gpioConfig = GpioConfig::defaults();
DefaultLedDriver ledDriver;
EffectEngine effectEngine(state, ledDriver);
StorageService storageService(state, networkConfig, gpioConfig);
WifiService wifiService(networkConfig);
ApiService apiService(state, networkConfig, gpioConfig, storageService, wifiService);

unsigned long lastFrameAtMs = 0;
constexpr unsigned long kFrameIntervalMs = 16;
unsigned long lastHeartbeatAtMs = 0;

void waitForSerialIfUsbCdcEnabled(unsigned long timeoutMs) {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  const unsigned long start = millis();
  while (!Serial && (millis() - start) < timeoutMs) {
    delay(10);
  }
#else
  (void)timeoutMs;
#endif
}
} // namespace

void setup() {
  Serial.begin(115200);
  waitForSerialIfUsbCdcEnabled(3000);
  delay(150);

  storageService.begin();
  ledDriver.configure(gpioConfig);
  wifiService.begin();
  effectEngine.begin();
  apiService.begin();

  Serial.println("[boot] DUXMAN-LED-NEXT started");
  Serial.print("[boot] profile=");
  Serial.println(BuildProfile::kName);
  Serial.print("[boot] led.backend=");
  Serial.println(ledDriver.backendName());
  for (uint8_t i = 0; i < gpioConfig.outputCount; ++i) {
    const LedOutput &o = gpioConfig.outputs[i];
    Serial.print("[boot] output[");
    Serial.print(i);
    Serial.print("] pin=");
    Serial.print(o.pin);
    Serial.print(" count=");
    Serial.print(o.ledCount);
    Serial.print(" type=");
    Serial.print(o.ledType);
    Serial.print(" order=");
    Serial.println(o.colorOrder);
  }
  Serial.print("[boot] debug.enabled=");
  Serial.print(networkConfig.debug.enabled ? "true" : "false");
  Serial.print(" heartbeatMs=");
  Serial.println(networkConfig.debug.heartbeatMs);
}

void loop() {
  apiService.handle();
  wifiService.handle();

  const unsigned long now = millis();
  const unsigned long heartbeatIntervalMs = networkConfig.debug.heartbeatMs;
  if (heartbeatIntervalMs > 0 && now - lastHeartbeatAtMs >= heartbeatIntervalMs) {
    lastHeartbeatAtMs = now;
    Serial.print("[hb] alive ms=");
    Serial.println(now);
  }

  if (now - lastFrameAtMs >= kFrameIntervalMs) {
    lastFrameAtMs = now;
    effectEngine.renderFrame();
  }
}
