/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/HardwareInfo.cpp
 * Last commit: 38e7ffb - 2026-04-02
 */

#include "core/HardwareInfo.h"

#include <ArduinoJson.h>
#include <esp_system.h>

namespace {
String formatMac(uint64_t mac) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           static_cast<uint8_t>((mac >> 40) & 0xFF),
           static_cast<uint8_t>((mac >> 32) & 0xFF),
           static_cast<uint8_t>((mac >> 24) & 0xFF),
           static_cast<uint8_t>((mac >> 16) & 0xFF),
           static_cast<uint8_t>((mac >> 8) & 0xFF),
           static_cast<uint8_t>(mac & 0xFF));
  return String(buffer);
}
} // namespace

String HardwareInfo::toJson() {
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo);

  JsonDocument doc;
  doc["chipModel"] = ESP.getChipModel();
  doc["chipRevision"] = ESP.getChipRevision();
  doc["cores"] = chipInfo.cores;
  doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
  doc["hasWifi"] = (chipInfo.features & CHIP_FEATURE_WIFI_BGN) != 0;
  doc["hasBluetoothClassic"] = (chipInfo.features & CHIP_FEATURE_BT) != 0;
  doc["hasBluetoothLe"] = (chipInfo.features & CHIP_FEATURE_BLE) != 0;
  doc["flashSizeBytes"] = ESP.getFlashChipSize();
  doc["flashSizeMb"] = ESP.getFlashChipSize() / (1024 * 1024);
  doc["flashSpeedHz"] = ESP.getFlashChipSpeed();
  doc["mac"] = formatMac(ESP.getEfuseMac());

  String out;
  serializeJsonPretty(doc, out);
  return out;
}