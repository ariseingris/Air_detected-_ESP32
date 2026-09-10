#include <Arduino.h>
#include <math.h>

/*
 * ============================================================
 * Air Detected - ESP32
 * ============================================================
 *
 * Outputs:
 *   GREEN  -> Normal
 *   YELLOW -> Warning
 *   RED    -> Danger
 *   BUZZER -> Alarm when dangerous
 *
 * Sensors:
 *   - Optical dust sensor -> PM2.5
 *   - Gas sensor          -> relative gas index
 *
 * IMPORTANT:
 *   Gas sensor values are treated as a RELATIVE INDEX.
 *   They are NOT direct ppm measurements.
 * ============================================================
 */


// ============================================================
// PIN CONFIGURATION
// ============================================================

// *** LED OUTPUT PINS ***
constexpr uint8_t RED_LED_PIN    = 25;   // GPIO25 -> Red LED (Danger)
constexpr uint8_t YELLOW_LED_PIN = 26;   // GPIO26 -> Yellow LED (Warning)
constexpr uint8_t GREEN_LED_PIN  = 27;   // GPIO27 -> Green LED (Normal)

// *** DUST SENSOR (PM2.5) ***
constexpr uint8_t DUST_SENSOR_PIN = 35;  // GPIO35 (ADC1_CH6) -> Dust sensor analog out
constexpr uint8_t DUST_LED_PIN    = 32;  // GPIO32 -> Dust sensor LED (IR)

// *** GAS SENSOR (VOCs/humidity) ***
// NOTE: MUST be on a different ADC pin than dust sensor
constexpr uint8_t GAS_SENSOR_PIN = 34;   // GPIO34 (ADC1_CH7) -> Gas sensor analog out

// *** BUZZER/ALARM ***
constexpr uint8_t BUZZER_PIN = 14;       // GPIO14 -> Buzzer (piezo alarm)


// ============================================================
// PM2.5 THRESHOLDS
// ============================================================

constexpr float PM25_GREEN_MAX  = 25.0f;
constexpr float PM25_YELLOW_MAX = 110.0f;


// ============================================================
// GAS THRESHOLDS
// ============================================================

constexpr float GAS_GREEN_MAX  = 35.0f;
constexpr float GAS_YELLOW_MAX = 200.0f;


// ============================================================
// SENSOR SETTINGS
// ============================================================

// ESP32 ADC
constexpr float ADC_MAX = 4095.0f;
constexpr float ADC_VREF = 3.3f;


// ============================================================
// ENUMS
// ============================================================

enum class AirLevel {
    GREEN,
    YELLOW,
    RED
};


// ============================================================
// DATA STRUCTURES
// ============================================================

struct SensorData {
    int dustRaw;
    float dustVoltage;
    float pm25;

    int gasRaw;
    float gasVoltage;
    float gasIndex;

    AirLevel pmLevel;
    AirLevel gasLevel;
    AirLevel finalLevel;
};


// ============================================================
// GLOBAL STATE
// ============================================================

SensorData data;


// ============================================================
// LED CONTROL
// ============================================================

void setLEDs(bool green, bool yellow, bool red)
{
    digitalWrite(GREEN_LED_PIN, green ? HIGH : LOW);
    digitalWrite(YELLOW_LED_PIN, yellow ? HIGH : LOW);
    digitalWrite(RED_LED_PIN, red ? HIGH : LOW);
}


void setAirLevel(AirLevel level)
{
    switch (level)
    {
        case AirLevel::GREEN:
            setLEDs(true, false, false);
            break;

        case AirLevel::YELLOW:
            setLEDs(false, true, false);
            break;

        case AirLevel::RED:
            setLEDs(false, false, true);
            break;
    }
}


// ============================================================
// BUZZER
// ============================================================

void buzzerOff()
{
    digitalWrite(BUZZER_PIN, LOW);
}


void buzzerOn()
{
    digitalWrite(BUZZER_PIN, HIGH);
}


// ============================================================
// AIR LEVEL
// ============================================================

AirLevel classifyPM25(float pm25)
{
    if (pm25 < PM25_GREEN_MAX)
        return AirLevel::GREEN;

    if (pm25 <= PM25_YELLOW_MAX)
        return AirLevel::YELLOW;

    return AirLevel::RED;
}


AirLevel classifyGas(float gasIndex)
{
    if (gasIndex < GAS_GREEN_MAX)
        return AirLevel::GREEN;

    if (gasIndex <= GAS_YELLOW_MAX)
        return AirLevel::YELLOW;

    return AirLevel::RED;
}


AirLevel combineLevels(AirLevel pmLevel, AirLevel gasLevel)
{
    /*
     * Priority:
     *
     * RED > YELLOW > GREEN
     */

    if (pmLevel == AirLevel::RED ||
        gasLevel == AirLevel::RED)
    {
        return AirLevel::RED;
    }

    if (pmLevel == AirLevel::YELLOW ||
        gasLevel == AirLevel::YELLOW)
    {
        return AirLevel::YELLOW;
    }

    return AirLevel::GREEN;
}


// ============================================================
// ADC
// ============================================================

float adcToVoltage(int raw)
{
    return (static_cast<float>(raw) / ADC_MAX) * ADC_VREF;
}


// ============================================================
// PM2.5 SENSOR
// ============================================================

int readDustRaw()
{
    /*
     * GP2Y1010-style optical dust sensor timing.
     *
     * Sensor LED:
     *   ON
     *   wait ~280 us
     *   ADC sample
     *   OFF
     */

    digitalWrite(DUST_LED_PIN, LOW);

    delayMicroseconds(280);

    int raw = analogRead(DUST_SENSOR_PIN);

    delayMicroseconds(40);

    digitalWrite(DUST_LED_PIN, HIGH);

    delayMicroseconds(9680);

    return raw;
}


float calculatePM25(int raw)
{
    /*
     * IMPORTANT:
     *
     * This is an INITIAL ESTIMATE.
     * The exact coefficient depends on:
     *   - sensor model
     *   - supply voltage
     *   - ADC reference
     *   - optical chamber
     *   - calibration
     *
     * Therefore this value should be calibrated experimentally.
     */

    float voltage = adcToVoltage(raw);

    /*
     * Common GP2Y1010-style approximation:
     *
     * dust density (mg/m3)
     *      = 0.17 * V - 0.1
     *
     * Convert:
     *
     * mg/m3 -> ug/m3
     *
     * multiply by 1000.
     */

    float densityMgM3 = 0.17f * voltage - 0.1f;

    if (densityMgM3 < 0.0f)
        densityMgM3 = 0.0f;

    float pm25 = densityMgM3 * 1000.0f;

    return pm25;
}


// ============================================================
// GAS SENSOR
// ============================================================

int readGasRaw()
{
    /*
     * Gas sensor (e.g., MiCS-6814 or similar) 
     * outputs analog voltage to GPIO35.
     *
     * DO NOT interpret this directly as ppm.
     * This is read as a RELATIVE INDEX only.
     */

    return analogRead(GAS_SENSOR_PIN);
}


// ============================================================
// SIMPLE GAS INDEX
// ============================================================

float calculateGasIndex(int raw)
{
    /*
     * Temporary normalized index.
     *
     * ADC:
     *   0    -> 0
     *   4095 -> 1000
     *
     * This gives us a controllable index for your
     * 0-35 / 35-200 / >200 thresholds.
     *
     * Later this should be replaced by a calibrated
     * MiCS-6814 algorithm.
     */

    return (static_cast<float>(raw) / ADC_MAX) * 1000.0f;
}


// ============================================================
// SERIAL OUTPUT
// ============================================================

void printSensorData()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("SENSOR READINGS");
    Serial.println("========================================");

    // Dust sensor (PM2.5)
    Serial.println("[DUST SENSOR - PM2.5]");
    Serial.printf("  RAW ADC      : %d (0-4095)\n", data.dustRaw);
    Serial.printf("  Voltage      : %.3f V\n", data.dustVoltage);
    Serial.printf("  PM2.5        : %.2f ug/m3\n", data.pm25);
    Serial.printf("  Level        : %s\n", 
        data.pmLevel == AirLevel::GREEN ? "GREEN" :
        data.pmLevel == AirLevel::YELLOW ? "YELLOW" : "RED");

    // Gas sensor
    Serial.println();
    Serial.println("[GAS SENSOR - Relative Index]");
    Serial.printf("  RAW ADC      : %d (0-4095)\n", data.gasRaw);
    Serial.printf("  Voltage      : %.3f V\n", data.gasVoltage);
    Serial.printf("  Gas Index    : %.2f\n", data.gasIndex);
    Serial.printf("  Level        : %s\n",
        data.gasLevel == AirLevel::GREEN ? "GREEN" :
        data.gasLevel == AirLevel::YELLOW ? "YELLOW" : "RED");

    // Final decision
    Serial.println();
    Serial.println("[FINAL AIR QUALITY]");
    Serial.printf("  Status       : %s\n",
        data.finalLevel == AirLevel::GREEN ? "✓ GOOD" :
        data.finalLevel == AirLevel::YELLOW ? "⚠ WARNING" : "✗ DANGER");
    Serial.println("========================================");
}


// ============================================================
// STARTUP TEST
// ============================================================

void startupTest()
{
    Serial.println("Starting Air Detector...");

    // GREEN
    setLEDs(true, false, false);
    delay(300);

    // YELLOW
    setLEDs(false, true, false);
    delay(300);

    // RED
    setLEDs(false, false, true);
    delay(300);

    // BUZZER
    buzzerOn();
    delay(300);
    buzzerOff();

    // OFF
    setLEDs(false, false, false);
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    // ========================================================
    // PIN MODE SETUP
    // ========================================================

    // LED pins - OUTPUT
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);

    // Control pins - OUTPUT
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(DUST_LED_PIN, OUTPUT);

    // Sensor pins - INPUT (ADC)
    pinMode(DUST_SENSOR_PIN, INPUT);
    pinMode(GAS_SENSOR_PIN, INPUT);

    // ========================================================
    // ADC CONFIGURATION
    // ========================================================

    // ESP32 ADC resolution: 12-bit (0-4095)
    analogReadResolution(12);

    // ========================================================
    // INITIAL STATE
    // ========================================================

    setLEDs(false, false, false);
    buzzerOff();
    digitalWrite(DUST_LED_PIN, HIGH);

    // ========================================================
    // STARTUP SELF-TEST
    // ========================================================

    startupTest();

    // ========================================================
    // READY
    // ========================================================

    Serial.println();
    Serial.println("========================================");
    Serial.println("Air Detector - INITIALIZED");
    Serial.println("========================================");
    Serial.printf("Dust Sensor   : GPIO%d (ADC1_CH6)\n", DUST_SENSOR_PIN);
    Serial.printf("Dust LED      : GPIO%d (IR control)\n", DUST_LED_PIN);
    Serial.printf("Gas Sensor    : GPIO%d (ADC1_CH7)\n", GAS_SENSOR_PIN);
    Serial.printf("Red LED       : GPIO%d\n", RED_LED_PIN);
    Serial.printf("Yellow LED    : GPIO%d\n", YELLOW_LED_PIN);
    Serial.printf("Green LED     : GPIO%d\n", GREEN_LED_PIN);
    Serial.printf("Buzzer        : GPIO%d\n", BUZZER_PIN);
    Serial.println("========================================");
    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // Read PM2.5 Dust Sensor
    // --------------------------------------------------------

    data.dustRaw = readDustRaw();

    data.dustVoltage = adcToVoltage(data.dustRaw);

    data.pm25 = calculatePM25(data.dustRaw);


    // --------------------------------------------------------
    // Read Gas Sensor
    // --------------------------------------------------------

    data.gasRaw = readGasRaw();

    data.gasVoltage = adcToVoltage(data.gasRaw);

    data.gasIndex = calculateGasIndex(data.gasRaw);


    // --------------------------------------------------------
    // Classification
    // --------------------------------------------------------

    data.pmLevel = classifyPM25(data.pm25);

    data.gasLevel = classifyGas(data.gasIndex);

    data.finalLevel = combineLevels(data.pmLevel, data.gasLevel);


    // --------------------------------------------------------
    // OUTPUT - LEDs
    // --------------------------------------------------------

    setAirLevel(data.finalLevel);


    // --------------------------------------------------------
    // OUTPUT - Buzzer (Alarm on RED)
    // --------------------------------------------------------

    if (data.finalLevel == AirLevel::RED)
    {
        buzzerOn();
    }
    else
    {
        buzzerOff();
    }


    // --------------------------------------------------------
    // DEBUG - Serial Output
    // --------------------------------------------------------

    printSensorData();


    // --------------------------------------------------------
    // Sampling Interval (1 second)
    // --------------------------------------------------------

    delay(1000);
}