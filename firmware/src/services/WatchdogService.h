/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/WatchdogService.h
 * Last commit: bd45789 - 2026-04-03
 */

#ifndef WATCHDOG_SERVICE_H
#define WATCHDOG_SERVICE_H

#include <cstdint>
#include <esp_task_wdt.h>

/**
 * @class WatchdogService
 * @brief Task Watchdog Timer (TWDT) management for FreeRTOS tasks.
 * 
 * Monitors control and render tasks for unexpected hangs/deadlocks.
 * Logs statistics and triggers recovery actions on timeout.
 */
class WatchdogService {
 public:
  struct Stats {
    uint32_t resetCount;         // Total watchdog resets
    uint32_t controlTaskResets;  // Control task hang count
    uint32_t renderTaskResets;   // Render task hang count
    uint64_t lastResetTimeMs;    // Timestamp of last reset
  };

  WatchdogService();

  /**
   * @brief Initialize watchdog with timeout and recovery strategy.
   * @param timeoutS Watchdog timeout in seconds (typically 4-10s for dual-core)
   * @param autoResetEnabled If true, device will reboot on timeout
   * @return true if initialization succeeded
   */
  bool init(uint32_t timeoutS = 5, bool autoResetEnabled = true);

  /**
   * @brief Register a FreeRTOS task for watchdog monitoring.
   * @param taskHandle FreeRTOS task handle (from xTaskCreatePinnedToCore)
   * @param taskName Human-readable task name for logging
   * @return true if registration succeeded
   */
  bool registerTask(TaskHandle_t taskHandle, const char* taskName);

  /**
   * @brief Feed the watchdog to reset the timer.
   * Call this periodically from the monitored task.
   */
  void feed();

  /**
   * @brief Get current watchdog statistics.
   */
  Stats getStats() const { return stats_; }

  /**
   * @brief Reset watchdog statistics (typically at boot).
   */
  void resetStats();

 private:
  Stats stats_;
  bool initialized_;
  uint32_t timeoutSeconds_;
  bool autoResetOnTimeout_;
};

#endif // WATCHDOG_SERVICE_H
