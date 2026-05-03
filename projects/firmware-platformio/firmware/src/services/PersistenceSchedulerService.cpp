/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/services/PersistenceSchedulerService.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

#include "services/PersistenceSchedulerService.h"

#include <freertos/FreeRTOS.h>

namespace {
portMUX_TYPE gPersistenceMux = portMUX_INITIALIZER_UNLOCKED;
}

PersistenceSchedulerService::PersistenceSchedulerService(StorageService &storageService)
    : storageService_(storageService) {}

void PersistenceSchedulerService::setPending(uint8_t mask) {
  taskENTER_CRITICAL(&gPersistenceMux);
  pendingFlags_ |= mask;
  taskEXIT_CRITICAL(&gPersistenceMux);
}

void PersistenceSchedulerService::requestSaveState() {
  setPending(kPendingState);
}

void PersistenceSchedulerService::requestSaveConfig() {
  setPending(kPendingConfig);
}

void PersistenceSchedulerService::requestSaveAll() {
  setPending(kPendingState | kPendingConfig);
}

void PersistenceSchedulerService::processPending() {
  uint8_t work = 0;

  taskENTER_CRITICAL(&gPersistenceMux);
  work = pendingFlags_;
  pendingFlags_ = 0;
  taskEXIT_CRITICAL(&gPersistenceMux);

  if ((work & kPendingState) != 0 && !storageService_.saveState()) {
    Serial.println("[storage][async] saveState failed");
  }

  if ((work & kPendingConfig) != 0 && !storageService_.saveConfig()) {
    Serial.println("[storage][async] saveConfig failed");
  }
}
