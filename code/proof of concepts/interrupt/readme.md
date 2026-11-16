# start/stop interrupt proof of concept
minimale hard- en software die de correcte werking van een start/stop drukknop aantoont, gebruik makend van een hardware interrupt
# Proof of Concept – Start/Stop Interrupt

**Hardware:** ESP32-S3 met drukknop op GPIO10 en externe pull-downweerstand van 10 kΩ naar GND. De drukknop is aangesloten tussen GPIO10 en 3,3 V.

**Software:** Arduino IDE met Espressif ESP32 Arduino-core versie 3.x.

**Werking:** De drukknop werkt via een hardware-interrupt op `CHANGE`. Een softwarematige ontdendering van 35 ms voorkomt ongewenste schakelingen. Bij iedere geldige druk wisselen de motoren tussen START en STOP.

**PWM:** De motoren worden aangestuurd met een PWM-frequentie van 20 kHz en een resolutie van 8 bits.

**Compilatie:** Open de sketch, selecteer het juiste ESP32-S3-board en compileer met Arduino ESP32-core 3.x.
