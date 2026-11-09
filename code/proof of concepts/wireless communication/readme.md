# Draadloze communicatie proof of concept

Met een ESP32-S3, een USB-datakabel en een laptop toon ik aan dat de microcontroller en de laptop draadloos in beide richtingen kunnen communiceren. De laptop gebruikt een **webbrowser** en wisselt berichten uit met de ESP32-S3 via één **WebSocket-verbinding**.

### Configuratie

| Onderdeel | Instelling |
| --- | --- |
| Microcontroller | ESP32-S3 |
| Programma op de laptop | Webbrowser |
| Software op de ESP32-S3 | Arduino-sketch met de bibliotheek **WebSockets** van Markus Sattler |
| Wifi-netwerk | `Lijnrobot-PoC` |
| Wifiwachtwoord | `Lijnrobot123` |
| Adres in de browser | `http://192.168.4.1/` |
| WebSocket-adres | `ws://192.168.4.1:81/` |

### Opmerkingen

De ESP32-S3 maakt zelf het wifi-netwerk aan. Een router of internetverbinding is tijdens de proef overbodig. De USB-kabel dient voor voeding en het uploaden van de sketch. De testberichten lopen via wifi.

### Gebruiksaanwijzing

1. Installeer de bibliotheek **WebSockets** van Markus Sattler in Arduino IDE en upload `DraadlozeCommunicatiePoC.ino` naar de ESP32-S3.
2. Verbind de laptop met `Lijnrobot-PoC` en open `http://192.168.4.1/` in de browser. Wacht tot de pagina **Verbonden** toont.
3. Kijk of de looptijd elke seconde stijgt zonder dat je iets aanklikt. Zo controleer je dat de ESP32-S3 zelfstandig gegevens naar de laptop stuurt.
4. Klik drie keer op **Verstuur PING**. De proef slaagt als de PONG-teller van 0 naar 3 gaat terwijl de looptijd blijft oplopen. Elke PONG bevestigt dat de ESP32-S3 een PING van de laptop heeft ontvangen en via dezelfde verbinding heeft geantwoord.
