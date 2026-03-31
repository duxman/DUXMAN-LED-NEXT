#include <Arduino.h>

#include "api/ApiService.h"
#include "core/BuildProfile.h"
#include "core/CoreState.h"
#include "core/Config.h"
#include "core/ReleaseInfo.h"
#include "drivers/LedDriver.h"
#include "effects/EffectEngine.h"
#include "services/StorageService.h"
#include "services/WifiService.h"

namespace {
CoreState state = CoreState::defaults();
NetworkConfig networkConfig = NetworkConfig::defaults();
ReleaseInfo releaseInfo = ReleaseInfo::defaults();
LedDriver ledDriver;
EffectEngine effectEngine(state, ledDriver);
StorageService storageService(state, networkConfig, releaseInfo);
WifiService wifiService(networkConfig);
ApiService apiService(state, networkConfig, releaseInfo, storageService, wifiService);

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
  wifiService.begin();
  effectEngine.begin();
  apiService.begin();

  Serial.println("[boot] DUXMAN-LED-NEXT started");
  Serial.print("[boot] profile=");
  Serial.print(BuildProfile::kName);
  Serial.print(" ledPin=");
  Serial.print(BuildProfile::kLedPin);
  Serial.print(" ledCount=");
  Serial.println(BuildProfile::kLedCount);
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
