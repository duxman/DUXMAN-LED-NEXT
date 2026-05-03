# PowerConfig - Advanced Voltage Optimizer Configuration

## Overview

The `PowerConfig` structure provides comprehensive control over LED power management, voltage correction, and thermal protection. It replaces the legacy `GpioPowerLimitConfig` with significantly enhanced capabilities.

## Configuration Parameters

### Power Limiting Section

```json
{
  "powerLimitEnabled": true,
  "maxTotalCurrentmA": 5000,
  "milliAmpsPerLedBase": 60
}
```

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| **powerLimitEnabled** | bool | true/false | false | Enable/disable total current limiting |
| **maxTotalCurrentmA** | uint16 | 200-50000 | 5000 | Maximum current budget across all outputs (mA) |
| **milliAmpsPerLedBase** | uint8 | 5-80 | 60 | Estimated current per LED at full white (mA) |

**Usage**: When `powerLimitEnabled` is true, VoltageOptimizer automatically scales all LED colors uniformly if total consumption exceeds `maxTotalCurrentmA`.

**Calibration**: Measure actual current consumption at full white for your specific LED type:
- WS2812B: 50-70 mA per LED typical
- SK6812: 40-60 mA per LED typical  
- APA102: 20-30 mA per LED typical

---

### Voltage Sag Correction Section

```json
{
  "voltageSagCorrectionEnabled": true,
  "cableResistanceOhms": 0.15,
  "supplyVoltageNominal": 5.0,
  "minAcceptableVoltage": 4.5
}
```

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| **voltageSagCorrectionEnabled** | bool | true/false | false | Enable voltage drop compensation |
| **cableResistanceOhms** | float | 0.0-1.0 | 0.1 | Cable resistance from PSU to LEDs (Ω) |
| **supplyVoltageNominal** | float | 3.0-48.0 | 5.0 | Target supply voltage (V) |
| **minAcceptableVoltage** | float | 1.0-48.0 | 4.5 | Minimum voltage floor to maintain (V) |

**Math**: `V_drop = I(A) × R(Ω)`

**Example Calibration**:
1. Connect 300 LEDs at full white (~3.6A)
2. Measure voltage at PSU: 5.00V
3. Measure voltage at LED strip: 4.85V
4. Drop = 0.15V, so: R = 0.15V / 3.6A = 0.042 Ω

When voltage drops below `minAcceptableVoltage`, the optimizer reduces brightness to compensate (max 15% boost).

---

### Thermal Throttling Section

```json
{
  "thermalThrottlingEnabled": false,
  "temperatureSensorPin": -1,
  "tempThrottleStartC": 50,
  "tempThrottleMaxC": 70
}
```

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| **thermalThrottlingEnabled** | bool | true/false | false | Enable thermal management |
| **temperatureSensorPin** | int8 | -1 to 48 | -1 | ADC pin for temp sensor (-1 = disabled) |
| **tempThrottleStartC** | int16 | -20 to 100 | 50 | Start reducing current (°C) |
| **tempThrottleMaxC** | int16 | 0 to 120 | 70 | Hard limit / emergency throttle (°C) |

**Behavior**:
- Below `tempThrottleStartC`: No throttling (100%)
- Between start and max: Linear reduction
- At `tempThrottleMaxC`: Full throttle (0% consumption)

**Example**: If start=50°C and max=70°C at 60°C:
- Throttle factor = (60-50) / (70-50) = 50%
- Current consumption reduced to 50%

**Sensor Setup** (optional):
```cpp
// NTC thermistor on ADC pin 34 (ESP32)
// Configure: temperatureSensorPin: 34
// Future: automatic ADC-to-C calibration
```

---

### Smart Dimming Section

```json
{
  "smartDimmingEnabled": true,
  "preserveBlueFrequency": true,
  "priorityMode": 0
}
```

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| **smartDimmingEnabled** | bool | true/false | false | Enable intelligent dimming |
| **preserveBlueFrequency** | true/false | true/false | true | Protect blue from PWM noise |
| **priorityMode** | uint8 | 0-2 | 0 | Dimming strategy |

**Priority Modes**:
- **0 = Saturation**: Maintains color hue, reduces brightness (recommended)
- **1 = Brightness**: Uniform RGB reduction (classic approach)
- **2 = Balanced**: Hybrid approach

**Blue Frequency Protection**: 
- Blue LEDs at low PWM (~240Hz) can produce audible tones
- When enabled, blue channel is protected at higher levels during correction
- Reduces power by preferentially dimming red/green

---

## API Endpoints

### Get Power Configuration

```
GET /api/v1/config/gpio
```

Response (GPIO config including power):
```json
{
  "gpio": {
    "outputs": [...],
    "power": {
      "powerLimitEnabled": true,
      "maxTotalCurrentmA": 5000,
      ...
    }
  }
}
```

### Update Power Configuration

```
PATCH /api/v1/config/gpio
Content-Type: application/json
```

Request body (partial update):
```json
{
  "gpio": {
    "power": {
      "powerLimitEnabled": true,
      "maxTotalCurrentmA": 3000,
      "cableResistanceOhms": 0.2
    }
  }
}
```

### Monitor Real-Time Power (Future API)

```
GET /api/v1/power/monitor
```

Response:
```json
{
  "totalCurrentmA": 2845,
  "predictedPeakmA": 3200,
  "supplyVoltage": 4.92,
  "thermalThrottlePercent": 95.0,
  "temperatureC": 55.3,
  "outputsMetrics": [
    {
      "outputId": 0,
      "estimatedCurrentmA": 2845,
      "correctionFactor": 0.95,
      "isThrottled": false
    }
  ]
}
```

---

## Configuration Examples

### Example 1: Simple Setup (5V, ~100 LEDs)

```json
{
  "gpio": {
    "power": {
      "powerLimitEnabled": true,
      "maxTotalCurrentmA": 2000,
      "milliAmpsPerLedBase": 60,
      "voltageSagCorrectionEnabled": false,
      "thermalThrottlingEnabled": false,
      "smartDimmingEnabled": false
    }
  }
}
```

### Example 2: Long Cable Run (300 LEDs, 3m cable)

```json
{
  "gpio": {
    "power": {
      "powerLimitEnabled": true,
      "maxTotalCurrentmA": 5000,
      "milliAmpsPerLedBase": 60,
      "voltageSagCorrectionEnabled": true,
      "cableResistanceOhms": 0.35,
      "supplyVoltageNominal": 5.0,
      "minAcceptableVoltage": 4.5,
      "smartDimmingEnabled": true,
      "preserveBlueFrequency": true
    }
  }
}
```

### Example 3: Thermal-Sensitive Installation

```json
{
  "gpio": {
    "power": {
      "powerLimitEnabled": true,
      "maxTotalCurrentmA": 5000,
      "voltageSagCorrectionEnabled": true,
      "thermalThrottlingEnabled": true,
      "temperatureSensorPin": 34,
      "tempThrottleStartC": 55,
      "tempThrottleMaxC": 75,
      "smartDimmingEnabled": true
    }
  }
}
```

---

## Backward Compatibility

The firmware supports **legacy `powerLimit` field** for backward compatibility:

```json
{
  "gpio": {
    "powerLimit": {
      "enabled": true,
      "maxCurrentmA": 2500,
      "milliAmpsPerLed": 60
    }
  }
}
```

This is automatically mapped to the new `power` configuration:
- `enabled` → `powerLimitEnabled`
- `maxCurrentmA` → `maxTotalCurrentmA`
- `milliAmpsPerLed` → `milliAmpsPerLedBase`

⚠️ **Migration Note**: The legacy format will be deprecated in v0.6.0. Use `power` field for new deployments.

---

## Validation Rules

| Rule | Error Code | Condition |
|------|-----------|-----------|
| Power current in range | `invalid_power_max_total_current_ma` | 200 mA ≤ maxTotalCurrentmA ≤ 50000 mA |
| LED mA estimate in range | `invalid_power_milliamps_per_led_base` | 5 mA ≤ base ≤ 80 mA |
| Supply voltage realistic | `invalid_power_supply_voltage_nominal` | 3.0V ≤ nominal ≤ 48.0V |
| Min voltage < nominal | `power_min_voltage_exceeds_nominal` | minAcceptable < supplyNominal |
| Cable resistance valid | `invalid_power_cable_resistance` | 0 Ω ≤ resistance ≤ 1.0 Ω |
| Temp range sensible | `power_temp_max_must_exceed_start` | tempThrottleMaxC > tempThrottleStartC |
| Temp values in range | Multiple | -20°C ≤ start ≤ 100°C, 0°C ≤ max ≤ 120°C |
| Priority mode valid | `invalid_power_priority_mode` | 0 ≤ priorityMode ≤ 2 |

---

## Troubleshooting

### LEDs Dimmer Than Expected

1. **Check calibration**: Verify `milliAmpsPerLedBase` matches your LED type
2. **Monitor consumption**: Use `/api/v1/power/monitor` endpoint
3. **Reduce budget**: If `maxTotalCurrentmA` is too low, increase it incrementally
4. **Disable corrections**: Try disabling `voltageSagCorrectionEnabled` temporarily

### Colors Look Wrong After Dimming

1. **Smart dimming mode**: Ensure `priorityMode: 0` (saturation priority)
2. **Blue artifacts**: Check if `preserveBlueFrequency` is causing issues
3. **Enable smart dimming**: Set `smartDimmingEnabled: true` for better color preservation

### Configuration Not Persisting

1. **API response**: Check `/api/v1/config/gpio` returns your changes
2. **LittleFS**: Device must have writable storage
3. **Watchdog**: Long operations might trigger reset; increase delay in persistence scheduler

---

## Performance Impact

- **CPU**: ~2-3% per render cycle (negligible)
- **Memory**: ~1.2 KB static + ~3 KB history buffer (for 60s window)
- **Latency**: <1ms per frame with all optimizations enabled

## Future Enhancements

- [ ] Per-output current budgeting
- [ ] FFT-based consumption prediction
- [ ] Effect-based profiles (static vs. animated)
- [ ] Online calibration wizard UI
- [ ] Cloud statistics export
