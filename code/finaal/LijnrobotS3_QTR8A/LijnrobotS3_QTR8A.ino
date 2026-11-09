// Open dit bestand in Arduino IDE. RobotCode.h en HardwareConfig.h moeten in dezelfde map staan.
// Board: ESP32S3 Dev Module; boardpakket: esp32 by Espressif Systems 3.3.11.
// De volledige robotcode verschijnt in de tweede tab: RobotCode.h.
#include "RobotCode.h"

void setup() {
  Serial.begin(115200);
  // Niet wachten op een USB-verbinding: de robot moet ook op batterij starten.
  robot_setup();
}

void loop() {
  // De vaste regelcyclus en seriele bediening draaien in afzonderlijke taken.
  delay(1000);
}
