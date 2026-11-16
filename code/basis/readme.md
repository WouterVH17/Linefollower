# basis programma

volgende zaken werken samen in dit template project  

* instelbare parameters mbv. SerialCommands 
  * cyclus tijd
  * debug on/off
  * start/stop
* parameters worden opgeslagen in eeprom geheugen
* start/stop kan ook geactiveerd worden mbv. een externe interrupt
* draadloze communicatie

Met behulp van de cyclus kan je bvb. een knipperende led programmeren.

# Basisprogramma – ESP32-S3 Lijnvolgrobot

## 1. Doel

Dit basisprogramma combineert de verschillende proof of concepts van de lijnvolgrobot tot één geïntegreerd programma.

De volgende functionaliteiten zijn geïmplementeerd:

- Draadloze communicatie via WiFi en WebSocket.
- Instelbare PWM-motorsturing.
- Instelbare cyclustijd.
- Debugmodus die via commando's kan worden in- en uitgeschakeld.
- Periodieke uitvoering van een vaste cyclus.
- Opslag van parameters in EEPROM-emulatie.
- Start/stop via een hardware-interrupt met softwarematige ontdendering.

Het programma vormt de basis voor de verdere ontwikkeling van de lijnvolgrobot, waarbij later de lijnsensoren en PID-regeling geïntegreerd kunnen worden.

## 2. Gebruikte hardware

| Component | Aansluiting |
|---|---|
| ESP32-S3 | Microcontroller |
| Drukknop | GPIO10 – 3,3 V |
| Externe pull-downweerstand | 10 kΩ tussen GPIO10 en GND |
| Linkermotor IN1 | GPIO15 |
| Linkermotor IN2 | GPIO16 |
| Rechtermotor IN1 | GPIO17 |
| Rechtermotor IN2 | GPIO18 |
| Motordriver | Voor aansturing van de twee DC-motoren |

De drukknop is aangesloten op 3,3 V en GPIO10. Een externe pull-downweerstand van 10 kΩ zorgt ervoor dat de ingang LOW blijft zolang de knop niet ingedrukt wordt.

Bij het indrukken wordt de ingang HIGH.

De motoren worden via een motordriver aangestuurd. De ESP32 en motordriver hebben een gemeenschappelijke GND nodig.

## 3. Softwarevereisten

Het programma is ontwikkeld voor:

- Arduino IDE 2.x.
- ESP32 Arduino-core **versie 3.x** van Espressif Systems.
- Het juiste ESP32-S3-board in de Arduino IDE.
- Bibliotheken `Arduino.h`, `WiFi.h`, `WebServer.h` en `EEPROM.h` uit de ESP32-core.
- Bibliotheek `WebSockets.h` / `WebSocketsServer.h` van Markus Sattler (Links2004).

**Belangrijk:** De sketch gebruikt de LEDC-API van Arduino ESP32-core 3.x, waaronder `ledcAttach()` en `ledcWrite()` met GPIO-pinnummers.

De compatibiliteit met oudere coreversies wordt niet gegarandeerd.

## 4. Werking van het programma

### 4.1 Communicatie

De ESP32-S3 maakt een eigen WiFi-netwerk aan.

| Instelling | Waarde |
|---|---|
| SSID | Lijnrobot-PoC |
| Wachtwoord | Lijnrobot123 |
| IP-adres | 192.168.4.1 |
| HTTP-poort | 80 |
| WebSocket-poort | 81 |

Via de browser kan de gebruiker verbinding maken met de webinterface op `http://192.168.4.1`.

De webinterface toont de actuele motorstatus en instellingen. Daarnaast kunnen parameters worden aangepast.

De communicatie verloopt via WebSocket. Er is ook communicatie mogelijk via de seriële monitor op 115200 baud.

### 4.2 Instelbare parameters en commando's

| Commando | Functie |
|---|---|
| `SET 160` | PWM-waarde instellen op 160 |
| `CYCLE 20` | Cyclustijd instellen op 20 ms |
| `DEBUG ON` | Debugmodus inschakelen |
| `DEBUG OFF` | Debugmodus uitschakelen |
| `STATUS` | Actuele instellingen en motorstatus opvragen |
| `PING` | Communicatie testen met PONG-antwoord |

De PWM-waarde kan ingesteld worden tussen 1 en 255. Voor het wijzigen van deze waarde moeten de motoren gestopt zijn.

De cyclustijd kan ingesteld worden tussen 10 en 1000 milliseconden.

### 4.3 Vaste uitvoeringscyclus

De robot werkt met een instelbare periodieke uitvoeringscyclus.

De standaardcyclustijd bedraagt 20 ms.

De functie `handleFixedCycle()` controleert op basis van `micros()` wanneer de volgende cyclus uitgevoerd moet worden.

De functie `executeCycle()` voert de cyclische taken uit. In dit basisprogramma wordt de motoruitgang bijgewerkt.

De planning gebruikt geen blokkerende wachttijd voor de volledige cyclustijd. Hierdoor kunnen communicatie en drukknopbediening tussendoor verwerkt worden.

Wanneer de uitvoering vertraging oploopt, worden gemiste cycli geregistreerd. Er wordt geen harde realtimegarantie gegeven.

### 4.4 Start/stop met hardware-interrupt

De start/stopfunctie wordt bediend met een fysieke drukknop op GPIO10.

De interrupt wordt ingesteld op `CHANGE`, waardoor zowel een stijgende als dalende flank gedetecteerd wordt.

De interruptfunctie registreert een verandering. De verdere verwerking gebeurt in de hoofdloop.

Softwarematige ontdendering van 35 ms voorkomt ongewenste schakelingen door contactdendering.

De werking is als volgt:

1. Bij het opstarten staan de motoren stil.
2. Bij de eerste druk schakelen de motoren naar START.
3. Bij het loslaten blijft de toestand behouden.
4. Bij de volgende druk schakelen de motoren naar STOP.

De start/stopfunctie werkt onafhankelijk van de webinterface.

### 4.5 PWM-motorsturing

De twee motoren worden met PWM aangestuurd.

| Parameter | Waarde |
|---|---|
| PWM-frequentie | 20.000 Hz |
| Resolutie | 8 bits |
| Standaard PWM-waarde | 160 |
| Instelbereik | 1–255 |

De motoren worden in deze proef in één draairichting aangestuurd. De tweede stuuringang van iedere motor blijft LOW.

Bij STOP wordt de PWM-dutycycle op 0 ingesteld.

### 4.6 EEPROM

De instellingen worden opgeslagen via de EEPROM-emulatie van de ESP32.

De volgende parameters worden bewaard:

- PWM-waarde.
- Cyclustijd.
- Debugmodus.

Bij het opstarten probeert het programma de opgeslagen instellingen opnieuw in te lezen.

Wanneer er geen geldige instellingen beschikbaar zijn, worden de standaardwaarden gebruikt:

| Parameter | Standaardwaarde |
|---|---|
| PWM | 160 |
| Cyclustijd | 20 ms |
| Debug | OFF |

De instellingen worden alleen naar EEPROM geschreven wanneer een parameter effectief verandert.

### 4.7 Debugmodus

Met `DEBUG ON` kan extra informatie via de seriële monitor weergegeven worden.

De debuguitvoer bevat onder andere:

- Het aantal uitgevoerde cycli.
- Het aantal gemiste cycli.
- De ingestelde cyclustijd.
- De motorstatus.
- De ingestelde PWM-waarde.

De debuginformatie wordt maximaal eenmaal per seconde weergegeven.

Met `DEBUG OFF` wordt deze extra uitvoer uitgeschakeld.

## 5. Programma compileren en uploaden

1. Open Arduino IDE.
2. Installeer via Boards Manager het pakket `esp32` van Espressif Systems, versie 3.x.
3. Installeer de bibliotheek `WebSockets` van Markus Sattler.
4. Selecteer het juiste ESP32-S3-board.
5. Open `Basisprogramma.ino`.
6. Klik op *Verifiëren* om de sketch te compileren.
7. Selecteer de juiste USB/COM-poort en upload de sketch.
8. Open de seriële monitor op 115200 baud.

Voor seriële commando's moet het regeleinde ingesteld zijn op *Newline*.

## 6. Testprocedure

De werking kan met onderstaande stappen worden gecontroleerd.

**Test 1 – Communicatie**

Verbind een laptop of smartphone met `Lijnrobot-PoC` en open `http://192.168.4.1`.

Controleer of de webinterface verschijnt. Verstuur `PING` en controleer of `PONG` ontvangen wordt.

**Test 2 – Hardware-interrupt**

Druk op de fysieke start/stopknop en controleer of de motoren starten. Druk opnieuw en controleer of ze stoppen. Loslaten mag geen extra schakeling veroorzaken.

**Test 3 – Cyclustijd**

Stel met `CYCLE 50` de cyclustijd in op 50 ms. Controleer de ingestelde waarde met `STATUS`.

**Test 4 – Debugmodus**

Schakel de debugmodus in met `DEBUG ON`. Controleer of de seriële monitor periodieke debuggegevens toont. Schakel de functie vervolgens uit met `DEBUG OFF`.

**Test 5 – EEPROM**

Sla een nieuwe PWM-waarde en cyclustijd op. Herstart de ESP32 en controleer of dezelfde instellingen opnieuw ingelezen worden.

**Test 6 – PWM**

Wijzig de PWM-waarde terwijl de motoren gestopt zijn. Start de motoren en controleer of de gewijzigde PWM-dutycycle gebruikt wordt.

## 7. Bestandsstructuur

```text
code/
└── basis/
    ├── Basisprogramma/
    │   └── Basisprogramma.ino
    └── readme.md
```

## 8. Status en beperkingen

De code bevat alle vereiste onderdelen van het basisprogramma: communicatie, EEPROM, instelbare parameters, vaste cyclus en hardware-interrupt.

De werkelijke compilatie met ESP32 Arduino-core 3.x en de hardwaretests moeten nog bevestigd worden.

De lijnsensoren en PID-sturing zijn nog niet opgenomen in deze basisversie. De huidige motorsturing ondersteunt één draairichting.

Dit programma dient als geïntegreerde basis voor de verdere ontwikkeling van de lijnvolgrobot.
