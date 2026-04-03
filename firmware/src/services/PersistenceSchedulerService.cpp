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

void PersistenceSchedulerService::requestSaveNetwork() {
  setPending(kPendingNetwork);
}

void PersistenceSchedulerService::requestSaveGpio() {
  setPending(kPendingGpio);
}

void PersistenceSchedulerService::requestSaveAll() {
  setPending(kPendingState | kPendingNetwork | kPendingGpio);
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

  if ((work & kPendingNetwork) != 0 && !storageService_.saveNetworkConfig()) {
    Serial.println("[storage][async] saveNetworkConfig failed");
  }

  if ((work & kPendingGpio) != 0 && !storageService_.saveGpioConfig()) {
    Serial.println("[storage][async] saveGpioConfig failed");
  }
}