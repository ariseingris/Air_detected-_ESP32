# Sensor Calibration & Testing Guide

## Dust Sensor (PM2.5) - Calibration

### Current Calibration Formula
```
PM2.5 (µg/m³) = (Voltage * 0.17 - 0.1) * 1000
```

Where:
- `Voltage` = ADC reading converted to volts (0-3.3V)
- Coefficient `0.17` = slope (depends on sensor model)
- Offset `-0.1` = intercept (voltage at zero dust)

### Calibration Steps

1. **Get Reference PM2.5 Value**
   - Use an external PM2.5 meter or air quality monitor
   - Note the reference value in the same location as the sensor

2. **Read Sensor Raw ADC Value**
   - Connect to ESP32 serial output (115200 baud)
   - Note the "Dust RAW" and "Dust Voltage" values
   - Take multiple readings over 1-2 minutes

3. **Calculate New Coefficient (if needed)**
   ```
   Reference_PM25 = (Sensor_Voltage * K - 0.1) * 1000
   
   Solve for K:
   K = (Reference_PM25 / 1000 + 0.1) / Sensor_Voltage
   ```

4. **Update Formula in Code**
   - Change `0.17f` in `calculatePM25()` function
   - This coefficient is specific to your sensor model and calibration

### Example Calibration

Assume you have:
- Reference PM2.5: 50 µg/m³
- Sensor ADC reading: 1024 (12-bit)
- Sensor voltage: 0.828V

Calculate new coefficient:
```
K = (50 / 1000 + 0.1) / 0.828
K = (0.05 + 0.1) / 0.828
K = 0.15 / 0.828
K ≈ 0.181
```

Update in code: `calculatePM25()` line with `0.181f` instead of `0.17f`

---

## Gas Sensor (MiCS-6814 or Similar) - Calibration

### Current Index Formula
```
Gas_Index = (ADC_Raw / 4095) * 1000
```

This normalizes ADC 0-4095 to Index 0-1000.

### For Real ppm Conversion

If your sensor provides a datasheet with ppm conversion:

```cpp
float calculateGasIndex(int raw) 
{
    float voltage = adcToVoltage(raw);
    
    // Example for MiCS-6814 CO measurement:
    // ppm = 10.54 * V^4 - 13.67 * V^3 + 24.17 * V^2 - 44.85 * V + 77.255
    
    // For now, use normalized index (0-1000)
    return (static_cast<float>(raw) / ADC_MAX) * 1000.0f;
}
```

### Sensor-Specific Calibration

**MiCS-6814 (if available):**
- CO, NO₂, NH₃ measurements
- Requires polynomial calibration from datasheet
- Must warm up for 1-2 minutes after power-on

**BME680 (Alternative - I²C):**
- Includes onboard calibration
- Returns TVOC and absolute gas resistance
- Different pin setup required

**CCS811 (Alternative - I²C):**
- CO₂ and TVOC measurements
- Requires I²C pins (GPIO21 = SDA, GPIO22 = SCL)

---

## Hardware Testing Checklist

### Startup Test
The code will cycle through LEDs and buzzer on startup to verify connections:
1. Green LED ON (300ms)
2. Yellow LED ON (300ms)
3. Red LED ON (300ms)
4. Buzzer ON (300ms)
5. All OFF
6. Serial message: "Air Detector READY"

### Manual Testing Steps

**1. Test LEDs**
```
Expected: Green → Yellow → Red LEDs light sequentially
Issue: If LED doesn't light:
  - Check GPIO pin connection
  - Verify 330Ω resistor
  - Check LED polarity (+/-)
  - Test LED directly with 3.3V
```

**2. Test Dust Sensor**
```
Expected: ADC value changes when object/dust near sensor
Serial output:
  Dust RAW: [2000-3500 clean air, 3500+ with dust]
  
Issue: If value stuck at 4095 or 0:
  - Check GPIO34 connection to sensor
  - Verify sensor power supply (3.3V)
  - Check ADC reference voltage
  - Try different ADC pin (GPIO35, GPIO36, GPIO39)
```

**3. Test Gas Sensor**
```
Expected: ADC value 1000-2000 in clean air
Issue: If index stuck at 0 or 1000:
  - Check GPIO35 connection to sensor
  - Verify sensor power supply
  - Sensor may need warm-up time (1-2 min)
```

**4. Test Buzzer**
```
Expected: Buzzer activates when RED level detected
Issue: If buzzer silent:
  - Check GPIO27 connection
  - Verify buzzer polarity (+/-)
  - Test directly with 3.3V
  - Check if buzzer has built-in driver (some need transistor)
```

**5. Verify Serial Output**
```
Expected: Data printed every 1 second to serial at 115200 baud
```

---

## Threshold Adjustment

### PM2.5 Thresholds

Edit these constants in code:
```cpp
constexpr float PM25_GREEN_MAX  = 25.0f;   // µg/m³
constexpr float PM25_YELLOW_MAX = 110.0f;  // µg/m³
```

**Standard PM2.5 Levels (WHO/EPA):**
- Good: 0-12 µg/m³
- Acceptable: 12-35 µg/m³  
- Poor: 35-150 µg/m³
- Hazardous: >150 µg/m³

### Gas Index Thresholds

Edit these constants in code:
```cpp
constexpr float GAS_GREEN_MAX  = 35.0f;   // Index
constexpr float GAS_YELLOW_MAX = 200.0f;  // Index
```

Adjust based on your sensor's response characteristics.

---

## Troubleshooting Guide

### Problem: Serial Monitor Shows Garbage
**Solution:** 
- Check baud rate is 115200
- Verify USB driver installed
- Try different USB cable
- Check COM port selection in IDE

### Problem: ADC Values Never Change
**Solution:**
- Verify pin is truly ADC-capable (34, 35, 36, 39 on GPIO1)
- Check analog reference voltage (should be ~3.3V near sensor pin)
- Try reading from different ADC pin

### Problem: High Noise in Sensor Readings
**Solution:**
- Add 100nF capacitor across sensor output (GND)
- Reduce sampling rate in code (increase delay)
- Implement moving average filter:
  ```cpp
  const int BUFFER_SIZE = 5;
  int readings[BUFFER_SIZE];
  int average_raw() {
      int sum = 0;
      for(int i=0; i<BUFFER_SIZE; i++)
          sum += readings[i];
      return sum / BUFFER_SIZE;
  }
  ```

### Problem: Buzzer Always ON or Always OFF
**Solution:**
- Check finalLevel classification logic
- Verify buzzer pins HIGH/LOW logic (some buzzers are active LOW)
- Test buzzer voltage requirements (may need external driver)

---

## Data Sheet References

- **GP2Y1010**: Sharp dust sensor - IR LED timing critical
- **MiCS-6814**: Gas sensor multiplex readings
- **ESP32**: ADC max 12-bit on ADC1, ADC2 conflicts with WiFi

---

**Last Updated:** 2026-09-10
