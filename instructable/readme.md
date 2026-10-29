# Instructable

### Stap 1
Bestel alle componenten uit de bill of materials.

### Stap 2
Print het chassis volgens de technische tekeningen.

### Stap 3
Monteer de motoren op het chassis volgens de technische tekeningen.

### Stap 4
Bevestig de wielen op de motorassen.

### Stap 5
Monteer de bout met dopmoer als derde steunpunt.

### Stap 6
Monteer de Pololu QTR-8A vooraan op het chassis, ongeveer 3 mm boven de ondergrond. Plaats het midden tussen sensor 4 en sensor 5 op de middenlijn van het chassis.

### Stap 7
Monteer de printplaten, de ESP32-S3, de DRV8833, de buckconverters en de batterij volgens de technische tekeningen.

### Stap 8
Soldeer de connectoren en de bedrading op de printplaten volgens het elektrische schema. Laat de batterij losgekoppeld.

### Stap 9
Soldeer een keramische condensator van 100 nF met opschrift 104 rechtstreeks over de aansluitingen van elke motor.

### Stap 10
Stel de motorbuckconverter in op 6 V en de sensorbuckconverter op 3,3 V. Controleer beide spanningen met een multimeter voordat je de elektronica aansluit.

### Stap 11
Sluit de voeding aan volgens het elektrische schema. Verbind alle GND-aansluitingen met elkaar. Gebruik voor de ESP32-S3 uitsluitend de voeding die voor jouw ontwikkelbord is aangegeven.

### Stap 12
Sluit de elco van 470 µF / 16 V aan tussen de motorvoeding en GND, dicht bij de motordriver. Verbind de plus met de motorvoeding en de gestreepte minzijde met GND.

### Stap 13
Sluit de ingangen van de DRV8833 aan op de ESP32-S3.

| Motordriver | ESP32-S3 |
|---|---|
| IN1 / AIN1 | GPIO 15 |
| IN2 / AIN2 | GPIO 16 |
| IN3 / BIN1 | GPIO 17 |
| IN4 / BIN2 | GPIO 18 |

### Stap 14
Sluit de linkermotor aan op uitgangspaar A en de rechtermotor op uitgangspaar B. Zorg dat nSLEEP hoog staat volgens het schema van de drivermodule.

### Stap 15
Sluit de QTR-8A aan op 3,3 V en GND. Verbind de sensoruitgangen volgens onderstaande tabel.

| Sensor | ESP32-S3 |
|---|---|
| 1 | GPIO 1 |
| 2 | GPIO 2 |
| 3 | GPIO 4 |
| 4 | GPIO 5 |
| 5 | GPIO 6 |
| 6 | GPIO 7 |
| 7 | GPIO 8 |
| 8 | GPIO 9 |

### Stap 16
Sluit de drukknop en de signaallampen aan volgens het elektrische schema, indien deze in de gebruikte software zijn opgenomen.

### Stap 17
Controleer alle soldeerverbindingen, de polariteit en de bedrading met een multimeter.

### Stap 18
Installeer Arduino IDE op de computer.

### Stap 19
Open de voorkeuren van Arduino IDE en voeg deze URL toe bij de aanvullende bordbeheer-URL's:

https://espressif.github.io/arduino-esp32/package_esp32_index.json

### Stap 20
Open de Boards Manager, zoek naar `esp32` en installeer het pakket van Espressif Systems.

### Stap 21
Download de robotsoftware uit deze repository en pak het ZIP-bestand uit als dat nodig is. Bewaar alle bijbehorende `.h`-bestanden in dezelfde sketchmap als het `.ino`-bestand.

### Stap 22
Open het `.ino`-bestand in Arduino IDE. Installeer de eventuele extra bibliotheken die in de README van de robotsoftware vermeld staan.

### Stap 23
Controleer in de hardwareconfiguratie de sensorpinnen, motorpinnen en instellingen voor de draairichting.

### Stap 24
Koppel de motorvoeding los en verbind de ESP32-S3 met de computer via een USB-datakabel.

### Stap 25
Selecteer in Arduino IDE het juiste ESP32-S3-bord en de bijbehorende COM-poort. Gebruik de bordinstellingen uit de README van de robotsoftware.

### Stap 26
Klik op **Verifiëren** om de software te compileren. Los eventuele foutmeldingen op voordat je verdergaat.

### Stap 27
Klik op **Uploaden** en wacht tot de upload voltooid is.

### Stap 28
Als de verbinding voor het uploaden mislukt, houd dan BOOT ingedrukt, druk kort op RESET en laat BOOT los. Probeer daarna opnieuw te uploaden.

### Stap 29
Plaats de robot op een steun zodat beide wielen vrij kunnen draaien. Sluit de motorvoeding aan.

### Stap 30
Verbind je telefoon of computer met het wifi-netwerk van de robot. Gebruik de netwerknaam, het wachtwoord en het webadres uit de README.

### Stap 31
Test beide motoren via de bediening. Controleer vooruit, achteruit, links, rechts en stoppen.

### Stap 32
Keer een verkeerd draaiende motor om in de software en upload opnieuw, of schakel de voeding uit en verwissel de twee draden van die motor.

### Stap 33
Controleer in de sensorweergave of alle acht sensoren reageren op een witte en een zwarte ondergrond.

### Stap 34
Plaats alle sensoren boven een witte ondergrond en voer de witkalibratie uit via de interface.

### Stap 35
Plaats alle sensoren boven een zwart vlak en voer de zwartkalibratie uit via de interface. Sla de kalibratie op.

### Stap 36
Plaats de robot op het parcours met de zwarte lijn tussen sensor 4 en sensor 5.

### Stap 37
Stel een lage basissnelheid in en start de lijnvolgfunctie.

### Stap 38
Pas de PID-instellingen telkens afzonderlijk aan en test na elke wijziging op hetzelfde parcours.

### Stap 39
Verhoog de snelheid geleidelijk zodra de robot de lijn betrouwbaar volgt. Sla de definitieve instellingen op.

### Stap 40
Schakel de robot uit na het testen en koppel de batterij los voordat je deze oplaadt.
