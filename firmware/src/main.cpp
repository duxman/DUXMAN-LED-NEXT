/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/main.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "api/ApiService.h"
#include "core/BuildProfile.h"
#include "core/CoreState.h"
#include "core/Config.h"
#include "drivers/CurrentLedDriver.h"
#include "effects/EffectManager.h"
#include "services/AudioService.h"
#include "services/EffectPersistenceService.h"
#include "services/PersistenceSchedulerService.h"
#include "services/ProfileService.h"
#include "services/StorageService.h"
#include "services/UserPaletteService.h"
#include "services/WatchdogService.h"
#include "services/WifiService.h"

namespace {
CoreState state = CoreState::defaults();
NetworkConfig networkConfig = NetworkConfig::defaults();
GpioConfig gpioConfig = GpioConfig::defaults();
MicrophoneConfig microphoneConfig = MicrophoneConfig::defaults();
DebugConfig debugConfig = DebugConfig::defaults();
DefaultLedDriver ledDriver;
EffectManager effectManager(state, ledDriver);
StorageService storageService(state, networkConfig, gpioConfig, microphoneConfig, debugConfig);
PersistenceSchedulerService persistenceSchedulerService(storageService);
EffectPersistenceService effectPersistenceService(state);
ProfileService profileService(networkConfig, gpioConfig, microphoneConfig, debugConfig, storageService, persistenceSchedulerService, ledDriver);
UserPaletteService userPaletteService(persistenceSchedulerService);
WifiService wifiService(networkConfig, debugConfig);
WatchdogService watchdogService;
AudioService audioService(microphoneConfig, state, debugConfig);
ApiService apiService(state, networkConfig, gpioConfig, microphoneConfig, debugConfig, storageService, wifiService,
                      persistenceSchedulerService,
                      effectPersistenceService, profileService, userPaletteService, watchdogService);

SemaphoreHandle_t coreStateMutex = nullptr;
TaskHandle_t controlTaskHandle = nullptr;
TaskHandle_t renderTaskHandle = nullptr;

constexpr TickType_t kRenderIntervalTicks = pdMS_TO_TICKS(16);
constexpr TickType_t kControlIntervalTicks = pdMS_TO_TICKS(10);

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

void controlTask(void *parameter) {
  (void)parameter;
  unsigned long lastHeartbeatAtMs = 0;
  TickType_t lastWake = xTaskGetTickCount();

  while (true) {
    watchdogService.feed();  // Reset watchdog timer
    apiService.handle();
    wifiService.handle();
    audioService.handle(millis());
    profileService.processPendingPersistence();
    userPaletteService.processPendingPersistence();
    persistenceSchedulerService.processPending();

    const unsigned long now = millis();
    effectPersistenceService.handle(now);
    const unsigned long heartbeatIntervalMs = debugConfig.heartbeatMs;
    if (heartbeatIntervalMs > 0 && now - lastHeartbeatAtMs >= heartbeatIntervalMs) {
      lastHeartbeatAtMs = now;
      Serial.print("[hb] alive ms=");
      Serial.println(now);
    }

    vTaskDelayUntil(&lastWake, kControlIntervalTicks);
  }
}

void renderTask(void *parameter) {
  (void)parameter;
  TickType_t lastWake = xTaskGetTickCount();

  while (true) {
    watchdogService.feed();  // Reset watchdog timer
    effectManager.renderFrame();
    vTaskDelayUntil(&lastWake, kRenderIntervalTicks);
  }
}
} // namespace

void setup() {
  Serial.begin(115200);
  waitForSerialIfUsbCdcEnabled(3000);
  delay(150);

  storageService.begin();
  coreStateMutex = xSemaphoreCreateMutex();
  CoreState::setMutex(coreStateMutex);
  watchdogService.init(5, true);  // 5 second timeout, auto-reboot on timeout
  effectPersistenceService.begin();
  profileService.begin();
  userPaletteService.begin();
  String appliedProfileId;
  String profileError;
  const bool startupProfileApplied = profileService.applyStartupProfile(&appliedProfileId, &profileError);
  const bool startupEffectApplied = effectPersistenceService.applyStartupEffect();
  ledDriver.configure(gpioConfig);
  wifiService.begin();
  audioService.begin();
  effectManager.begin();
  apiService.begin();

  Serial.println("[boot] DUXMAN-LED-NEXT started");
  Serial.print("[boot] profile=");
  Serial.println(BuildProfile::kName);
  Serial.print("[boot] led.backend=");
  Serial.println(ledDriver.backendName());
  if (!appliedProfileId.isEmpty()) {
    Serial.print("[boot] startupProfile=");
    Serial.println(appliedProfileId);
  } else if (!profileError.isEmpty()) {
    Serial.print("[boot] startupProfile.error=");
    Serial.println(profileError);
  } else if (startupProfileApplied) {
    Serial.println("[boot] startupProfile applied");
  }
  if (startupEffectApplied) {
    Serial.println("[boot] startupEffect applied");
  }
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
  Serial.print(debugConfig.enabled ? "true" : "false");
  Serial.print(" heartbeatMs=");
  Serial.println(debugConfig.heartbeatMs);

  const BaseType_t controlTaskOk = xTaskCreatePinnedToCore(
      controlTask,
      "dux-control",
      4096,
      nullptr,
      2,
      &controlTaskHandle,
      0);

  const BaseType_t renderTaskOk = xTaskCreatePinnedToCore(
      renderTask,
      "dux-render",
      4096,
      nullptr,
      3,
      &renderTaskHandle,
      1);

  if (controlTaskOk == pdPASS && renderTaskOk == pdPASS) {
    // Register tasks with watchdog
    watchdogService.registerTask(controlTaskHandle, "dux-control");
    watchdogService.registerTask(renderTaskHandle, "dux-render");
    Serial.println("[rtos] tasks started control(core0) render(core1)");
    return;
  }

  Serial.println("[rtos] task creation failed; fallback to loop() scheduler");
}

void loop() {
  if (controlTaskHandle != nullptr && renderTaskHandle != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    return;
  }

  apiService.handle();
  wifiService.handle();
  effectPersistenceService.handle(millis());
  effectManager.renderFrame();
  delay(16);
}
