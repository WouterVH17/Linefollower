#include <Arduino.h>

// Volgorde: uitgang 1 t/m 8 van de QTR-8A.
constexpr uint8_t SENSOR_PINS[] = {1, 2, 4, 5, 6, 7, 8, 9};
constexpr uint32_t SAMPLE_INTERVAL_MS = 200;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  for (uint8_t pin : SENSOR_PINS) {
    analogSetPinAttenuation(pin, ADC_11db);
  }
}

void loop() {
  uint8_t sensor = 1;
  for (uint8_t pin : SENSOR_PINS) {
    const uint16_t raw = analogRead(pin);
    if (sensor > 1) {
      Serial.print('\t');
    }
    Serial.print('S');
    Serial.print(sensor);
    Serial.print(':');
    Serial.print(raw);
    ++sensor;
  }
  Serial.println();
  delay(SAMPLE_INTERVAL_MS);
}
