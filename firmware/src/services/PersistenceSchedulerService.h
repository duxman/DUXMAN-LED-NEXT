#pragma once

#include <Arduino.h>

#include "services/StorageService.h"

// PersistenceSchedulerService gestiona la escritura différée de la configuración.
// Toda la config (network, gpio, microphone, debug) se guarda como un bloque en /config.json.
class PersistenceSchedulerService {
public:
  explicit PersistenceSchedulerService(StorageService &storageService);

  void requestSaveState();
  void requestSaveConfig();  // Guarda toda la configuración unificada
  void requestSaveAll();

  // Procesa una tanda pequeña de trabajo de persistencia pendiente.
  void processPending();

private:
  enum PendingFlags : uint8_t {
    kPendingState  = 1 << 0,
    kPendingConfig = 1 << 1,
  };

  StorageService &storageService_;
  volatile uint8_t pendingFlags_ = 0;

  void setPending(uint8_t mask);
};