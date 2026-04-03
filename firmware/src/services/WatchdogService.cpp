#include "WatchdogService.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "[watchdog]";

WatchdogService::WatchdogService()
    : initialized_(false), timeoutSeconds_(5), autoResetOnTimeout_(true) {
  memset(&stats_, 0, sizeof(stats_));
}

bool WatchdogService::init(uint32_t timeoutS, bool autoResetEnabled) {
  if (initialized_) {
    ESP_LOGW(TAG, "Watchdog already initialized");
    return false;
  }

  timeoutSeconds_ = timeoutS;
  autoResetOnTimeout_ = autoResetEnabled;

  // Initialize task watchdog timer with configuration structure
  // The task watchdog handles monitoring FreeRTOS tasks
  esp_err_t err = esp_task_wdt_init(timeoutS, autoResetEnabled);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Watchdog init failed: %s", esp_err_to_name(err));
    return false;
  }

  initialized_ = true;
  stats_.lastResetTimeMs = millis();
  ESP_LOGI(TAG, "Watchdog initialized: timeout=%u sec, auto_reset=%d",
           timeoutSeconds_, autoResetEnabled);

  return true;
}

bool WatchdogService::registerTask(TaskHandle_t taskHandle, const char* taskName) {
  if (!initialized_) {
    ESP_LOGW(TAG, "Watchdog not initialized; cannot register task %s", taskName);
    return false;
  }

  if (taskHandle == nullptr) {
    ESP_LOGW(TAG, "Invalid task handle for %s", taskName);
    return false;
  }

  esp_err_t err = esp_task_wdt_add(taskHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register task %s: %s", taskName, esp_err_to_name(err));
    return false;
  }

  ESP_LOGI(TAG, "Task registered: %s", taskName);
  return true;
}

void WatchdogService::feed() {
  if (!initialized_) {
    return;
  }

  esp_task_wdt_reset();
}

void WatchdogService::resetStats() {
  stats_.resetCount = 0;
  stats_.controlTaskResets = 0;
  stats_.renderTaskResets = 0;
  stats_.lastResetTimeMs = millis();
}
