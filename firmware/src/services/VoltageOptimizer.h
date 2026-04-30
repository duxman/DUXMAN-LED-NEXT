#ifndef VOLTAGE_OPTIMIZER_H
#define VOLTAGE_OPTIMIZER_H

#include <cstdint>
#include <array>
#include "Arduino.h"
#include "Config.h"  // For PowerConfig

/**
 * @file VoltageOptimizer.h
 * @brief Advanced power management and voltage correction for LED systems
 * 
 * Provides intelligent power limiting, voltage sag correction, thermal throttling,
 * and energy prediction for multi-output LED installations with precise control.
 * 
 * PowerConfig is defined in core/Config.h and managed by VoltageOptimizer.
 */

// Real-time power metrics for a single output
struct OutputPowerMetrics {
  uint32_t estimatedCurrentmA = 0;        // Current consumption estimate (mA)
  float predictedPeakmA = 0.0f;          // Lookahead peak prediction
  float voltageDrop = 0.0f;               // Voltage sag at current draw (V)
  float correctionFactor = 1.0f;          // Applied dimming/correction (0.0-1.0)
  bool isThrottled = false;               // Thermal throttle active
  uint8_t throttleLevel = 100;            // %, 100 = no throttle
};

// Historical data point for consumption tracking
struct PowerHistoryPoint {
  uint32_t timestamp = 0;                 // ms since startup
  uint32_t totalCurrentmA = 0;            // All outputs combined
  uint8_t brightnessPercent = 0;          // Global brightness
  uint8_t effectId = 0;                   // Running effect
  float temperatureC = 0.0f;              // If available
};

/**
 * @class VoltageOptimizer
 * @brief Central power management service for LED systems
 * 
 * Handles:
 * - Real-time power consumption estimation
 * - Predictive peak detection
 * - Voltage sag compensation
 * - Thermal management
 * - Intelligent dimming strategies
 */
class VoltageOptimizer {
public:
  static constexpr uint8_t kMaxOutputs = 4;
  static constexpr uint16_t kHistoryBufferSize = 3600;  // 60s @ 60Hz
  
  VoltageOptimizer();
  ~VoltageOptimizer() = default;
  
  // Initialization
  void begin(const PowerConfig& config);
  void setConfig(const PowerConfig& config);
  const PowerConfig& config() const { return config_; }
  
  // Main update cycle (call from renderTask ~60Hz)
  void update(uint32_t currentTimeMs,
              uint8_t brightness,
              uint8_t effectId,
              const uint32_t* colors,  // RGB colors per output (max 4)
              uint8_t outputCount);
  
  // Query metrics
  const OutputPowerMetrics& outputMetrics(uint8_t outputIndex) const;
  uint32_t totalCurrentmA() const { return totalCurrentmA_; }
  uint32_t predictedPeakCurrentmA() const { return predictedPeakMaxmA_; }
  float estimatedSupplyVoltage() const;
  float thermalThrottlePercent() const { return thermalThrottlePercent_; }
  
  // Get correction factor for a given color
  uint32_t applyCorrectionFactor(uint32_t color, uint8_t outputIndex) const;
  
  // History access
  uint16_t historySize() const { return historyWriteIdx_; }
  const PowerHistoryPoint* historyBuffer() const { return history_.data(); }
  
  // Diagnostics
  String getDiagStatsJson() const;
  void logMetrics(bool verbose = false) const;

private:
  // Energy prediction
  void updateEnergyPrediction_(uint32_t colorRGB);
  uint32_t estimateColorConsumption_(uint32_t colorRGB) const;
  uint8_t estimateLedIntensity_(uint8_t r, uint8_t g, uint8_t b) const;
  
  // Voltage sag correction
  void updateVoltageSag_();
  float computeVoltageDrop_(uint32_t currentmA) const;
  
  // Thermal management
  void updateThermalThrottle_();
  float readTemperature_() const;
  
  // Smart dimming
  uint32_t applySmarDimming_(uint32_t color, float correctionFactor) const;
  
  // Configuration
  PowerConfig config_;
  
  // Per-output state
  std::array<OutputPowerMetrics, kMaxOutputs> outputMetrics_;
  uint8_t outputCount_ = 0;
  
  // Aggregate metrics
  uint32_t totalCurrentmA_ = 0;
  uint32_t predictedPeakMaxmA_ = 0;
  float supplyVoltageEstimate_ = 5.0f;
  float thermalThrottlePercent_ = 100.0f;
  
  // Energy prediction state
  std::array<uint32_t, kMaxOutputs> colorHistory_;
  std::array<float, kMaxOutputs> energySmoothingFilter_;
  
  // History buffer (circular)
  std::array<PowerHistoryPoint, kHistoryBufferSize> history_;
  uint16_t historyWriteIdx_ = 0;
  uint32_t lastHistorySampleMs_ = 0;
  
  // Temperature reading
  float lastTemperatureC_ = 25.0f;
  uint32_t lastTempReadMs_ = 0;
  
  // Flags
  bool isInitialized_ = false;
};

#endif // VOLTAGE_OPTIMIZER_H
