#include <Arduino.h>

// Matches your image wiring
const int SENSOR_PIN = 34; 
const int RED_LED    = 14; 
const int YELLOW_LED = 13;
const int GREEN_LED  = 12;

void setup() {
  Serial.begin(115200);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  // Quick test: Cycle LEDs at startup
  digitalWrite(RED_LED, HIGH); delay(500);
  digitalWrite(YELLOW_LED, HIGH); delay(500);
  digitalWrite(GREEN_LED, HIGH); delay(500);
  digitalWrite(RED_LED, LOW); digitalWrite(YELLOW_LED, LOW); digitalWrite(GREEN_LED, LOW);
}

void loop() {
  int rawValue = analogRead(SENSOR_PIN);
  
  // Convert 12-bit (0-4095) to a simpler 0-1000 scale for easier logic
  int airQuality = map(rawValue, 0, 4095, 0, 1000);

  Serial.printf("Raw: %d | Scaled: %d\n", rawValue, airQuality);

  if (airQuality < 1200) {       // Clean Air
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
  } 
  else if (airQuality > 1200 && airQuality < 2500) {  // Moderate/Warning
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  } 
  else {                        // Danger!
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }

  delay(500); 
}