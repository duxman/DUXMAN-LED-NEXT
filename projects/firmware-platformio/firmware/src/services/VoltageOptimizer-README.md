# VoltageOptimizer - Software Voltage Management System

## Overview

`VoltageOptimizer` is an advanced power management service for LED systems that provides:

- **Real-time power consumption estimation** - Accurate mA tracking per output
- **Predictive peak detection** - Lookahead to prevent brownouts
- **Voltage sag correction** - Compensates for cable resistance (V = Vcc - I*R)
- **Thermal throttling** - Reduces power at high temperatures
- **Smart dimming** - Preserves color saturation while reducing brightness
- **Energy history tracking** - 60 seconds of consumption data

## Architecture

### Core Components

1. **EnergyEstimator**
   - Estimates LED consumption based on color (RGB values)
   - Uses weighted luminance model (299*R + 587*G + 114*B)
   - Exponential smoothing for stability

2. **VoltageSagCorrector**
   - Models physical voltage drop: V_drop = I * R
   - Configurable cable resistance and acceptable voltage floor
   - Auto-compensates dimming to maintain perceived brightness

3. **ThermalThrottler**
   - Optional temperature sensor support (ADC-based)
   - Gradual current reduction as temperature rises
   - Prevents thermal runaway

4. **SmartDimmer**
   - Preserves color saturation during power reduction
   - Protects blue channel from PWM-induced noise
   - Multiple priority modes: saturation, brightness, balanced

## Configuration

### PowerConfig Structure

```cpp
struct PowerConfig {
  // Power limiting
  bool powerLimitEnabled = false;
  uint16_t maxTotalCurrentmA = 5000;
  uint8_t milliAmpsPerLedBase = 60;
  
  // Voltage sag correction
  bool voltageSagCorrectionEnabled = false;
  float cableResistanceOhms = 0.1f;
  float supplyVoltageNominal = 5.0f;
  float minAcceptableVoltage = 4.5f;
  
  // Thermal throttling
  bool thermalThrottlingEnabled = false;
  int8_t temperatureSensorPin = -1;
  int16_t tempThrottleStartC = 50;
  int16_t tempThrottleMaxC = 70;
  
  // Smart dimming
  bool smartDimmingEnabled = false;
  bool preserveBlueFrequency = true;
  uint8_t priorityMode = 0;
};
```

## Usage Example

### Basic Initialization

```cpp
#include "services/VoltageOptimizer.h"

VoltageOptimizer voltageOptimizer;

void setup() {
  PowerConfig config;
  config.powerLimitEnabled = true;
  config.maxTotalCurrentmA = 5000;  // 5A budget
  config.milliAmpsPerLedBase = 60;   // 60mA per LED @ full white
  
  config.voltageSagCorrectionEnabled = true;
  config.cableResistanceOhms = 0.15f;  // 0.15Ω from ESP to LEDs
  
  voltageOptimizer.begin(config);
}

void loop() {
  // In renderTask (60Hz):
  uint32_t colors[4] = { 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF };
  
  voltageOptimizer.update(
    millis(),
    brightness,
    effectId,
    colors,
    4  // number of outputs
  );
  
  // Apply corrections to your LED colors
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t correctedColor = voltageOptimizer.applyCorrectionFactor(colors[i], i);
    // Use correctedColor for LED output
  }
}
```

### Monitoring Power

```cpp
// Get real-time metrics
uint32_t totalCurrent = voltageOptimizer.totalCurrentmA();
uint32_t predictedPeak = voltageOptimizer.predictedPeakCurrentmA();
float voltage = voltageOptimizer.estimatedSupplyVoltage();
float throttle = voltageOptimizer.thermalThrottlePercent();

Serial.println(voltageOptimizer.getDiagStatsJson());
```

### Per-Output Metrics

```cpp
const OutputPowerMetrics& metrics = voltageOptimizer.outputMetrics(outputIndex);

Serial.print("Current: ");
Serial.print(metrics.estimatedCurrentmA);
Serial.print("mA, Correction: ");
Serial.print(metrics.correctionFactor, 2);
Serial.print(", Voltage Drop: ");
Serial.print(metrics.voltageDrop, 2);
Serial.println("V");
```

## API Endpoints (Future)

```
GET    /api/v1/power/monitor
       Returns real-time power metrics (mA, V, throttle %)

GET    /api/v1/power/history
       Returns last 60 seconds of consumption data

PATCH  /api/v1/config/power
       Update PowerConfig parameters
```

## Calibration Guide

### Measuring Cable Resistance

1. Connect a known load (e.g., 60 LEDs at max white = ~3600mA)
2. Measure voltage at source and at LED strip
3. V_drop = V_source - V_strip
4. R = V_drop / I_amps

Example:
- Source: 5.0V
- Strip: 4.85V
- Current: 3.6A
- R = (5.0 - 4.85) / 3.6 = 0.042 Ω

### Temperature Sensor Calibration

If using thermal throttling:
1. Mount sensor on ESP32 or near power supply
2. Calibrate ADC reading to Celsius
3. Set throttle start/max temperatures appropriate for your setup

## Performance Notes

- **CPU overhead**: ~2-3% per update cycle (negligible)
- **Memory**: ~1.2KB static + ~3KB history buffer
- **Resolution**: 100ms sample rate for history
- **Update frequency**: Call every render cycle (~60Hz ideal)

## Troubleshooting

### LEDs Dimming Unexpectedly

1. Check if `powerLimitEnabled` is too restrictive
2. Verify `milliAmpsPerLedBase` matches your LED type
3. Enable logging: `voltageOptimizer.logMetrics(true)`
4. Check history data via `/api/v1/power/history`

### Voltage Sag Issues

1. Calculate actual cable resistance (see Calibration)
2. Verify `supplyVoltageNominal` is correct
3. Check for loose connections
4. Consider thicker wiring or multiple power injection points

### Thermal Throttling Too Aggressive

1. Adjust `tempThrottleStartC` higher
2. Verify temperature sensor calibration
3. Improve thermal dissipation (heatsink, airflow)

## Future Enhancements

- [ ] FFT-based consumption prediction
- [ ] Per-effect consumption profiles
- [ ] Automatic voltage sag calibration
- [ ] Cloud-based statistics and analytics
- [ ] Multi-source power balancing
- [ ] Online calibration wizard
