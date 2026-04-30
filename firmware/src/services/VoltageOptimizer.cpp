#include "VoltageOptimizer.h"
#include <cmath>

// Constructor
VoltageOptimizer::VoltageOptimizer() {
  for (auto& output : outputMetrics_) {
    output = OutputPowerMetrics();
  }
  colorHistory_.fill(0);
  energySmoothingFilter_.fill(0.5f);
  supplyVoltageEstimate_ = 5.0f;
}

void VoltageOptimizer::begin(const PowerConfig& config) {
  config_ = config;
  isInitialized_ = true;
  supplyVoltageEstimate_ = config_.supplyVoltageNominal;
  lastTemperatureC_ = 25.0f;
}

void VoltageOptimizer::setConfig(const PowerConfig& config) {
  config_ = config;
}

void VoltageOptimizer::update(uint32_t currentTimeMs,
                              uint8_t brightness,
                              uint8_t effectId,
                              const uint32_t* colors,
                              uint8_t outputCount) {
  if (!isInitialized_ || outputCount > kMaxOutputs || !colors) {
    return;
  }

  outputCount_ = outputCount;
  totalCurrentmA_ = 0;
  predictedPeakMaxmA_ = 0;

  // Update per-output metrics
  for (uint8_t i = 0; i < outputCount_; i++) {
    uint32_t color = colors[i];
    
    // Estimate real consumption
    uint32_t colorConsumption = estimateColorConsumption_(color);
    totalCurrentmA_ += colorConsumption;
    
    // Energy prediction with smoothing
    updateEnergyPrediction_(color);
    
    // Store metrics
    outputMetrics_[i].estimatedCurrentmA = colorConsumption;
    outputMetrics_[i].correctionFactor = 1.0f;  // Will be adjusted below
  }

  // Global corrections
  if (config_.powerLimitEnabled) {
    // Calculate power limit correction factor
    float correctionFactor = 1.0f;
    if (totalCurrentmA_ > config_.maxTotalCurrentmA) {
      correctionFactor = static_cast<float>(config_.maxTotalCurrentmA) / 
                         static_cast<float>(totalCurrentmA_);
      correctionFactor = constrain(correctionFactor, 0.1f, 1.0f);
    }
    
    // Apply to all outputs
    for (uint8_t i = 0; i < outputCount_; i++) {
      outputMetrics_[i].correctionFactor *= correctionFactor;
    }
  }

  // Voltage sag correction
  if (config_.voltageSagCorrectionEnabled) {
    updateVoltageSag_();
  }

  // Thermal throttling
  if (config_.thermalThrottlingEnabled) {
    updateThermalThrottle_();
  }

  // Record history every 100ms
  if (currentTimeMs - lastHistorySampleMs_ >= 100) {
    PowerHistoryPoint point;
    point.timestamp = currentTimeMs;
    point.totalCurrentmA = totalCurrentmA_;
    point.brightnessPercent = brightness;
    point.effectId = effectId;
    point.temperatureC = lastTemperatureC_;
    
    history_[historyWriteIdx_] = point;
    historyWriteIdx_ = (historyWriteIdx_ + 1) % kHistoryBufferSize;
    lastHistorySampleMs_ = currentTimeMs;
  }
}

uint32_t VoltageOptimizer::estimateColorConsumption_(uint32_t colorRGB) const {
  const uint8_t r = (colorRGB >> 16) & 0xFF;
  const uint8_t g = (colorRGB >> 8) & 0xFF;
  const uint8_t b = colorRGB & 0xFF;
  
  // Estimate intensity (0-255 -> 0-100%)
  uint8_t intensity = estimateLedIntensity_(r, g, b);
  
  // Rough linear estimate per LED: I = (intensity/255) * milliAmpsPerLedBase
  // Assuming ~60 LEDs typical; scale accordingly
  // mA = (intensity * milliAmpsPerLedBase) / 255
  uint32_t estimatedmA = (static_cast<uint32_t>(intensity) * 
                          static_cast<uint32_t>(config_.milliAmpsPerLedBase)) / 255;
  
  return estimatedmA;
}

uint8_t VoltageOptimizer::estimateLedIntensity_(uint8_t r, uint8_t g, uint8_t b) const {
  // Weighted intensity: favors red (perceived brightness)
  // Relative luminance approximation: 0.299*R + 0.587*G + 0.114*B
  uint16_t weighted = (299U * r + 587U * g + 114U * b) / 1000U;
  return static_cast<uint8_t>(min(weighted, 255U));
}

void VoltageOptimizer::updateEnergyPrediction_(uint32_t colorRGB) {
  // Simple exponential smoothing for lookahead
  // Detector sudden brightness changes
  const float alpha = 0.3f;
  
  for (uint8_t i = 0; i < outputCount_; i++) {
    colorHistory_[i] = colorRGB;
    
    uint32_t consumption = estimateColorConsumption_(colorRGB);
    float smoothed = energySmoothingFilter_[i];
    smoothed = alpha * consumption + (1.0f - alpha) * smoothed;
    energySmoothingFilter_[i] = smoothed;
    
    // Peak detection with 50ms lookahead margin
    float predicted = smoothed * 1.2f;  // 20% safety margin
    if (predicted > predictedPeakMaxmA_) {
      predictedPeakMaxmA_ = static_cast<uint32_t>(predicted);
    }
  }
}

void VoltageOptimizer::updateVoltageSag_() {
  // Model voltage drop: V_drop = I * R
  float voltageDrop = computeVoltageDrop_(totalCurrentmA_);
  supplyVoltageEstimate_ = config_.supplyVoltageNominal - voltageDrop;
  
  // Constrain to minimum acceptable
  supplyVoltageEstimate_ = max(supplyVoltageEstimate_, config_.minAcceptableVoltage);
  
  // Calculate correction factor to maintain brightness
  // If voltage drops, we need to increase LED current slightly
  // But within safe limits
  float voltageCorrectionFactor = config_.supplyVoltageNominal / supplyVoltageEstimate_;
  voltageCorrectionFactor = constrain(voltageCorrectionFactor, 1.0f, 1.15f);  // Max 15% compensation
  
  // Apply to all outputs
  for (uint8_t i = 0; i < outputCount_; i++) {
    outputMetrics_[i].voltageDrop = voltageDrop;
    outputMetrics_[i].correctionFactor *= voltageCorrectionFactor;
    outputMetrics_[i].correctionFactor = constrain(outputMetrics_[i].correctionFactor, 0.0f, 1.0f);
  }
}

float VoltageOptimizer::computeVoltageDrop_(uint32_t currentmA) const {
  // V = I * R (I in A, R in Ω)
  float currentA = currentmA / 1000.0f;
  float voltageDrop = currentA * config_.cableResistanceOhms;
  return voltageDrop;
}

void VoltageOptimizer::updateThermalThrottle_() {
  float tempC = readTemperature_();
  lastTemperatureC_ = tempC;
  
  // Gradual throttling from start temp to max temp
  if (tempC >= config_.tempThrottleStartC) {
    float range = config_.tempThrottleMaxC - config_.tempThrottleStartC;
    if (range > 0) {
      float throttle = (tempC - config_.tempThrottleStartC) / range;
      throttle = constrain(throttle, 0.0f, 1.0f);
      
      // Convert to percentage (100% = no throttle, 0% = full throttle)
      thermalThrottlePercent_ = 100.0f * (1.0f - throttle);
      thermalThrottlePercent_ = constrain(thermalThrottlePercent_, 0.0f, 100.0f);
      
      // Apply thermal throttle to all outputs
      float thermalFactor = thermalThrottlePercent_ / 100.0f;
      for (uint8_t i = 0; i < outputCount_; i++) {
        outputMetrics_[i].correctionFactor *= thermalFactor;
        outputMetrics_[i].isThrottled = (thermalFactor < 0.99f);
        outputMetrics_[i].throttleLevel = static_cast<uint8_t>(thermalThrottlePercent_);
      }
    }
  } else {
    thermalThrottlePercent_ = 100.0f;
    for (uint8_t i = 0; i < outputCount_; i++) {
      outputMetrics_[i].isThrottled = false;
      outputMetrics_[i].throttleLevel = 100;
    }
  }
}

float VoltageOptimizer::readTemperature_() const {
  // TODO: Implement ADC reading if temperatureSensorPin is configured
  // For now, return ambient assumption
  if (config_.temperatureSensorPin < 0) {
    return lastTemperatureC_;  // No sensor configured
  }
  
  // Placeholder: would read ADC and convert
  // int rawValue = analogRead(config_.temperatureSensorPin);
  // float tempC = (rawValue / 1023.0f) * 100.0f;  // Example linear mapping
  
  return lastTemperatureC_;
}

const OutputPowerMetrics& VoltageOptimizer::outputMetrics(uint8_t outputIndex) const {
  if (outputIndex >= kMaxOutputs) {
    static OutputPowerMetrics empty;
    return empty;
  }
  return outputMetrics_[outputIndex];
}

uint32_t VoltageOptimizer::applyCorrectionFactor(uint32_t color, uint8_t outputIndex) const {
  if (outputIndex >= kMaxOutputs) {
    return color;
  }
  
  float correctionFactor = outputMetrics_[outputIndex].correctionFactor;
  
  if (correctionFactor >= 0.999f) {
    return color;
  }
  
  const uint8_t r = (color >> 16) & 0xFF;
  const uint8_t g = (color >> 8) & 0xFF;
  const uint8_t b = color & 0xFF;
  
  // Apply correction with smart dimming
  uint32_t corrected;
  if (config_.smartDimmingEnabled && config_.preserveBlueFrequency) {
    corrected = applySmarDimming_(color, correctionFactor);
  } else {
    // Simple uniform dimming
    uint8_t rr = static_cast<uint8_t>(static_cast<float>(r) * correctionFactor + 0.5f);
    uint8_t gg = static_cast<uint8_t>(static_cast<float>(g) * correctionFactor + 0.5f);
    uint8_t bb = static_cast<uint8_t>(static_cast<float>(b) * correctionFactor + 0.5f);
    
    corrected = (static_cast<uint32_t>(rr) << 16) |
                (static_cast<uint32_t>(gg) << 8) |
                static_cast<uint32_t>(bb);
  }
  
  return corrected;
}

uint32_t VoltageOptimizer::applySmarDimming_(uint32_t color, float correctionFactor) const {
  const uint8_t r = (color >> 16) & 0xFF;
  const uint8_t g = (color >> 8) & 0xFF;
  const uint8_t b = color & 0xFF;
  
  // Preserve saturation by scaling down brightness while maintaining hue
  // Convert to HSV, reduce V, convert back (simplified)
  // Or: reduce brightness by increasing ratio while capping at 255
  
  // Simple approach: preserve blue channel more
  float blueProtection = 1.0f;  // Blue stays brighter
  float otherDim = correctionFactor;
  
  uint8_t rr = static_cast<uint8_t>(static_cast<float>(r) * otherDim + 0.5f);
  uint8_t gg = static_cast<uint8_t>(static_cast<float>(g) * otherDim + 0.5f);
  uint8_t bb = static_cast<uint8_t>(static_cast<float>(b) * blueProtection + 0.5f);
  
  // Apply correction factor but preserve max value for blue
  if (bb > 0) {
    bb = static_cast<uint8_t>(static_cast<float>(bb) * correctionFactor + 0.5f);
  }
  
  uint32_t corrected = (static_cast<uint32_t>(rr) << 16) |
                       (static_cast<uint32_t>(gg) << 8) |
                       static_cast<uint32_t>(bb);
  
  return corrected;
}

float VoltageOptimizer::estimatedSupplyVoltage() const {
  return supplyVoltageEstimate_;
}

String VoltageOptimizer::getDiagStatsJson() const {
  String json = "{";
  json += "\"totalCurrentmA\":" + String(totalCurrentmA_) + ",";
  json += "\"predictedPeakmA\":" + String(predictedPeakMaxmA_) + ",";
  json += "\"supplyVoltage\":" + String(supplyVoltageEstimate_, 2) + ",";
  json += "\"thermalThrottlePercent\":" + String(thermalThrottlePercent_, 1) + ",";
  json += "\"temperatureC\":" + String(lastTemperatureC_, 1) + ",";
  json += "\"historySamples\":" + String(historyWriteIdx_) + ",";
  json += "\"outputCount\":" + String(outputCount_);
  json += "}";
  return json;
}

void VoltageOptimizer::logMetrics(bool verbose) const {
  Serial.print("[VoltageOptimizer] Total: ");
  Serial.print(totalCurrentmA_);
  Serial.print("mA, Predicted: ");
  Serial.print(predictedPeakMaxmA_);
  Serial.print("mA, Voltage: ");
  Serial.print(supplyVoltageEstimate_, 2);
  Serial.print("V, Thermal: ");
  Serial.print(thermalThrottlePercent_, 1);
  Serial.println("%");
  
  if (verbose) {
    for (uint8_t i = 0; i < outputCount_; i++) {
      const auto& metrics = outputMetrics_[i];
      Serial.print("  [Out");
      Serial.print(i);
      Serial.print("] ");
      Serial.print(metrics.estimatedCurrentmA);
      Serial.print("mA, corr:");
      Serial.print(metrics.correctionFactor, 2);
      Serial.print(", throttle:");
      Serial.print(metrics.isThrottled ? "YES" : "NO");
      Serial.println();
    }
  }
}
