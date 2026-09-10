# ESP32 Air Quality Detector - Pin Configuration

## Overview
This document details the ESP32 pin assignments for the air quality detection system, including sensors, LED indicators, and buzzer alarm.

---

## GPIO Pin Assignments

### LED Output Pins (Digital Output)

| Function | GPIO | Color | Purpose | Active |
|----------|------|-------|---------|--------|
| GREEN_LED_PIN | **27** | Green | Normal air quality | HIGH |
| YELLOW_LED_PIN | **26** | Yellow | Warning air quality | HIGH |
| RED_LED_PIN | **25** | Red | Dangerous air quality | HIGH |

### Dust Sensor - PM2.5 (Optical Type)

| Component | GPIO | Type | Notes |
|-----------|------|------|-------|
| DUST_SENSOR_PIN | **35** | ADC1 CH6 | Analog voltage output from dust sensor |
| DUST_LED_PIN | **32** | Digital Output | Controls IR LED on dust sensor module |

**Timing Sequence (readDustRaw):**
```
1. Set DUST_LED_PIN to LOW
2. Wait ~280 µs for IR to stabilize
3. Read DUST_SENSOR_PIN (ADC)
4. Wait ~40 µs
5. Set DUST_LED_PIN to HIGH
6. Wait ~9680 µs (total cycle ~10ms)
```

**Sensor Model:** GP2Y1010-style optical dust sensor
- Measures PM2.5 (particles ≤ 2.5 micrometers)
- Conversion formula: `PM2.5 = (voltage * 0.17 - 0.1) * 1000`

---

### Gas Sensor (VOCs/Air Quality Index)

| Component | GPIO | Type | Notes |
|-----------|------|------|-------|
| GAS_SENSOR_PIN | **34** | ADC1 CH7 | Analog voltage output from gas sensor |

**Sensor Model:** MiCS-6814 compatible (or similar)
- Measures relative gas index (NOT direct ppm)
- Normalized to 0-1000 scale
- ADC 0 → Index 0
- ADC 4095 → Index 1000

---

### Control Output Pins

| Function | GPIO | Type | Purpose | Active |
|----------|------|------|---------|--------|
| BUZZER_PIN | **27** | Digital Output | Alarm buzzer/piezo speaker | HIGH |

**Buzzer Behavior:**
- Activates (HIGH) when air quality reaches RED level
- Deactivates (LOW) when air quality improves

---

## Sensor Thresholds

### PM2.5 Classification (ug/m³)

| Level | Range | Status | LED |
|-------|-------|--------|-----|
| GREEN | 0 - 25 | Good | Green ON |
| YELLOW | 25 - 110 | Warning | Yellow ON |
| RED | > 110 | Dangerous | Red ON |

### Gas Index Classification

| Level | Range | Status | LED |
|-------|-------|--------|-----|
| GREEN | 0 - 35 | Good | Green ON |
| YELLOW | 35 - 200 | Warning | Yellow ON |
| RED | > 200 | Dangerous | Red ON |

---

## ADC Configuration

- **ESP32 ADC Pins Used:** ADC1_CH6 (GPIO34), ADC1_CH7 (GPIO35)
- **Resolution:** 12-bit (0-4095)
- **Reference Voltage:** 3.3V
- **Formula:** `Voltage = (ADC_raw / 4095) * 3.3`

**Important:** ADC1 and ADC2 cannot be used simultaneously when WiFi is active.

---

## Pin Verification Checklist

- [ ] GPIO27 connected to Green LED (with limiting resistor ~330Ω)
- [ ] GPIO26 connected to Yellow LED (with limiting resistor ~330Ω)
- [ ] GPIO25 connected to Red LED (with limiting resistor ~330Ω)
- [ ] GPIO35 connected to Dust Sensor analog output (A0)
- [ ] GPIO32 connected to Dust Sensor LED control (active HIGH)
- [ ] GPIO34 connected to Gas Sensor analog output
- [ ] GPIO14 connected to Buzzer/Piezo speaker (active HIGH)
- [ ] All grounds properly connected
- [ ] 3.3V power supply properly distributed to sensors

---

## Wiring Diagram Reference

```
ESP32 DevKit V1
┌─────────────────────┐
│                     │
│  GND ─────────────── (All GND rails)
│  3V3 ─────────────── (All Power rails)
│                     │
│  GPIO27 ──────[R]──→ GREEN LED (+)
│  GPIO26 ──────[R]──→ YELLOW LED (+)
│  GPIO25 ──────[R]──→ RED LED (+)
│                     │
│  GPIO35 ←─────────── DUST SENSOR (Analog out)
│  GPIO32 ──────────→ DUST SENSOR (LED control)
│  GPIO34 ←─────────── GAS SENSOR (Analog out)
│                     │
│  GPIO14 ──────[R]──→ BUZZER (+)
│                     │
└─────────────────────┘

R = 330Ω resistor (for LEDs)
```

---

## Notes

- **Sampling Rate:** 1 second (1000ms delay)
- **ADC Resolution:** 12-bit ≈ 0.8mV per step
- **Dust Sensor:** Requires careful IR LED timing (GP2Y1010 spec)
- **Gas Sensor:** Index is relative - calibration required for specific sensor model
- **Serial Debug:** Output at 115200 baud

---

**Last Updated:** 2026-09-10  
**Device:** ESP32 DevKit V1
