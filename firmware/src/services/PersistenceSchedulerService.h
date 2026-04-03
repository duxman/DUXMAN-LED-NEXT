#pragma once

#include <Arduino.h>

#include "services/StorageService.h"

class PersistenceSchedulerService {
public:
  explicit PersistenceSchedulerService(StorageService &storageService);

  void requestSaveState();
  void requestSaveNetwork();
  void requestSaveGpio();
  void requestSaveAll();

  // Procesa una tanda pequeña de trabajo de persistencia pendiente.
  void processPending();

private:
  enum PendingFlags : uint8_t {
    kPendingState = 1 << 0,
    kPendingNetwork = 1 << 1,
    kPendingGpio = 1 << 2,
  };

  StorageService &storageService_;
  volatile uint8_t pendingFlags_ = 0;

  void setPending(uint8_t mask);
};