# VoltageOptimizer - Integration Guide for Developers

## Quick Start

This guide shows how to integrate `VoltageOptimizer` into your firmware's main render loop and configuration system.

## Step 1: Declarations

Add to your main header or globals:

```cpp
#include "services/VoltageOptimizer.h"

// Global instance
VoltageOptimizer gVoltageOptimizer;
```

## Step 2: Initialization

In `setup()`:

```cpp
void setup() {
  // ... other init code ...
  
  // Initialize voltage optimizer with current config
  gVoltageOptimizer.begin(gConfig.gpio.power);
  
  Serial.println("[SETUP] VoltageOptimizer initialized");
}
```

## Step 3: Update in Render Loop

In your render/display task (runs ~60 Hz), call `update()` before rendering:

```cpp
void renderTask(void *param) {
  const TickType_t xDelay = pdMS_TO_TICKS(16);  // ~60Hz (16ms)
  
  while (true) {
    // Get current frame colors (4 outputs max)
    uint32_t frameColors[4];
    uint8_t outputCount = gConfig.gpio.outputCount;
    
    // Render effect → frameColors[]
    gEffectManager.renderFrame(
      frameColors,
      outputCount,
      gState.brightness,
      gState.effectId
    );
    
    // === UPDATE VOLTAGE OPTIMIZER ===
    gVoltageOptimizer.update(
      millis(),
      gState.brightness,
      gState.effectId,
      frameColors,
      outputCount
    );
    
    // Apply power corrections
    for (uint8_t i = 0; i < outputCount; i++) {
      uint32_t correctedColor = gVoltageOptimizer.applyCorrectionFactor(frameColors[i], i);
      gLedDriver.setColor(i, correctedColor);
    }
    
    // Output to hardware
    gLedDriver.show();
    gLedDriver.delay(1);
    
    vTaskDelay(xDelay);
  }
}
```

## Step 4: Configuration Updates

When config changes (e.g., via API):

```cpp
void handleConfigGpioUpdate(const String &payload) {
  String error;
  
  if (gConfig.gpio.applyPatchJson(payload, &error)) {
    // Sync optimizer with new config
    gVoltageOptimizer.setConfig(gConfig.gpio.power);
    
    // Apply to hardware
    gLedDriver.reconfigure(gConfig.gpio);
    
    gState.saveToStorage();
    
    Serial.println("[CONFIG] GPIO updated, optimizer reconfigured");
  } else {
    Serial.println("[ERROR] GPIO config invalid: " + error);
  }
}
```

## Step 5: Monitoring (Optional)

Expose power metrics via API endpoint:

```cpp
// In ApiService::handleGetRequest()
if (uri == "/api/v1/power/monitor") {
  String response = gVoltageOptimizer.getDiagStatsJson();
  sendJsonResponse(200, response);
  return;
}
```

Example response:
```json
{
  "totalCurrentmA": 2847,
  "predictedPeakmA": 3200,
  "supplyVoltage": 4.92,
  "thermalThrottlePercent": 100.0,
  "temperatureC": 25.3,
  "historySamples": 1842,
  "outputCount": 1
}
```

## Step 6: Diagnostics

Enable detailed logging:

```cpp
// In a debug task or serial command handler
if (command == "power-stats") {
  gVoltageOptimizer.logMetrics(true);  // verbose = true
}
```

Example output:
```
[VoltageOptimizer] Total: 2847mA, Predicted: 3200mA, Voltage: 4.92V, Thermal: 100.0%
  [Out0] 2847mA, corr:0.95, throttle:NO
```

## Configuration via API

Users can configure power management via REST:

```bash
# Get current power config
curl http://device.local/api/v1/config/gpio | jq '.gpio.power'

# Update power limit
curl -X PATCH http://device.local/api/v1/config/gpio \
  -H "Content-Type: application/json" \
  -d '{
    "gpio": {
      "power": {
        "powerLimitEnabled": true,
        "maxTotalCurrentmA": 3000,
        "voltageSagCorrectionEnabled": true,
        "cableResistanceOhms": 0.2
      }
    }
  }'
```

## Common Parameters to Tune

### Conservative Setup (Low Power Supply)
```json
{
  "powerLimitEnabled": true,
  "maxTotalCurrentmA": 2000,
  "voltageSagCorrectionEnabled": true,
  "cableResistanceOhms": 0.2,
  "smartDimmingEnabled": true
}
```

### Aggressive Setup (Strong Power Supply)
```json
{
  "powerLimitEnabled": true,
  "maxTotalCurrentmA": 5000,
  "voltageSagCorrectionEnabled": true,
  "cableResistanceOhms": 0.05,
  "smartDimmingEnabled": false
}
```

### Thermal-Protected Setup
```json
{
  "powerLimitEnabled": true,
  "maxTotalCurrentmA": 5000,
  "thermalThrottlingEnabled": true,
  "temperatureSensorPin": 34,
  "tempThrottleStartC": 60,
  "tempThrottleMaxC": 80
}
```

## Performance Checklist

- [ ] VoltageOptimizer.update() completes in <2ms
- [ ] No stuttering or frame drops when enabled
- [ ] Power metrics accessible via API
- [ ] Configuration persists across reboots
- [ ] Thermal sensors (if used) reading correctly

## Troubleshooting

### VoltageOptimizer Not Dimming

1. **Check enablement**: `gVoltageOptimizer.logMetrics()` should show `correctionFactor < 1.0`
2. **Verify budget**: Is `maxTotalCurrentmA` lower than current consumption?
3. **Test directly**: 
   ```cpp
   uint32_t testColor = 0xFFFFFF;  // white
   uint32_t corrected = gVoltageOptimizer.applyCorrectionFactor(testColor, 0);
   Serial.printf("Original: 0x%06X, Corrected: 0x%06X\n", testColor, corrected);
   ```

### Colors Changing Unexpectedly

1. Enable `smartDimmingEnabled: true` for better saturation preservation
2. Check `priorityMode` value (should be 0 for saturation priority)
3. Verify `cableResistanceOhms` is realistic (0.05-0.5 typical)

### High Memory Usage

- VoltageOptimizer uses ~4.7KB total (1.2KB static + 3.5KB history)
- If memory critical, reduce `kHistoryBufferSize` in VoltageOptimizer.h
- Or disable history collection by removing history updates in `update()`

## API Reference

See:
- `firmware/src/services/VoltageOptimizer.h`: Full class definition
- `firmware/src/services/VoltageOptimizer-README.md`: User guide
- `firmware/config/POWER_CONFIG_GUIDE.md`: Configuration reference
