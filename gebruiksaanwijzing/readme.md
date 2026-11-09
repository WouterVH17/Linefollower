
# Gebruiksaanwijzing – Lijnvolgende robot

## 1. Opladen / vervangen batterijen

De lijnvolgende robot wordt gevoed door een oplaadbare **7,4 V LiPo-batterij (2S)**. De batterij levert de nodige energie aan de motoren en de elektronische componenten.

### Batterij opladen

1. Schakel de robot volledig uit.
2. Koppel de batterij los van de robot.
3. Sluit de batterij aan op een geschikte 2S LiPo-balanslader.
4. Stel de lader in volgens de specificaties van de batterij.
5. Wacht tot de batterij volledig is opgeladen.
6. Koppel de batterij los van de lader.
7. Sluit de batterij opnieuw aan op de robot.

### Batterij vervangen

1. Schakel de robot uit.
2. Maak de batterijconnector voorzichtig los.
3. Verwijder de lege batterij.
4. Plaats een opgeladen batterij met dezelfde specificaties.
5. Controleer of de batterij correct is aangesloten.
6. Schakel de robot opnieuw in.

**Let op:** Gebruik uitsluitend een geschikte LiPo-lader. Laad de batterij niet onbeheerd op en gebruik geen beschadigde of opgezwollen batterijen. De USB-aansluiting van de ESP32-S3 mag niet worden gebruikt om de LiPo-batterij rechtstreeks op te laden.

---

## 2. Draadloze communicatie

De robot beschikt over een **ESP32-S3-microcontroller** waarmee draadloze communicatie via wifi mogelijk is.

Via de draadloze verbinding kunnen de instellingen van de robot worden aangepast, kunnen de sensoren worden gecontroleerd en kan de robot worden gestart of gestopt.

### 2.1 Verbinding maken

Om de robot te bedienen met een laptop of smartphone, volg je onderstaande stappen:

1. Schakel de robot in.
2. Open de wifi-instellingen van je laptop of smartphone.
3. Zoek naar het wifinetwerk `linerobot`.
4. Maak verbinding met dit netwerk.
5. Open een webbrowser, bijvoorbeeld Google Chrome, Microsoft Edge of Safari.
6. Voer het IP-adres van de robot in.
7. De bedieningsinterface van de robot verschijnt.

**Netwerkinstellingen:**

| Instelling | Waarde |
|---|---|
| Wifi-naam (SSID) | linerobot |
| Verbinding | Wifi |
| IP-adres | Afhankelijk van de firmware |
| Bediening | Webbrowser |
| Compatibiliteit | Laptop / smartphone |

Bij een standaard ESP32-accesspoint wordt vaak `192.168.4.1` gebruikt. Controleer of dit adres overeenkomt met de ingestelde firmware.

Een internetverbinding is niet noodzakelijk omdat de communicatie rechtstreeks met de robot gebeurt.

### 2.2 Commando's

Via de draadloze interface kunnen onderstaande commando's worden verstuurd, voor zover deze in de firmware zijn geïmplementeerd.

| Commando | Beschrijving |
|---|---|
| `debug on` | Activeert de weergave van sensorgegevens en diagnostische informatie. |
| `debug off` | Schakelt de diagnostische informatie uit. |
| `start` | Start de robot. |
| `stop` | Stopt de robot. |
| `set cycle [µs]` | Stelt de cyclustijd van de regeling in microseconden in. |
| `set power [0..255]` | Bepaalt het basisvermogen van de motoren via PWM. |
| `set diff [0..1]` | Stelt de verschilfactor voor de motorbijsturing in. |
| `set kp [0..]` | Past de proportionele versterking van de PID-regelaar aan. |
| `set ki [0..]` | Past de integrerende versterking van de PID-regelaar aan. |
| `set kd [0..]` | Past de differentiërende versterking van de PID-regelaar aan. |
| `calibrate black` | Kalibreert de sensoren op een zwart oppervlak. |
| `calibrate white` | Kalibreert de sensoren op een wit oppervlak. |

### 2.3 Voorbeelden van commando's

**Robot starten:**

```text
start
```

**Robot stoppen:**

```text
stop
```

**Motorvermogen aanpassen:**

```text
set power 180
```

**PID-instellingen aanpassen:**

```text
set kp 1.80
set ki 0.02
set kd 0.85
```

**Debugmodus activeren:**

```text
debug on
```

In de debugmodus kunnen de sensorwaarden worden gecontroleerd om eventuele fouten sneller op te sporen.

---

## 3. Kalibratie

De robot beschikt over **11 lijnsensoren (Q1 tot Q11)** waarmee de zwarte lijn op een witte ondergrond wordt gedetecteerd.

Om een nauwkeurige werking te garanderen, moeten de sensoren worden gekalibreerd.

De kalibratie zorgt ervoor dat de robot een onderscheid kan maken tussen de zwarte lijn en de witte ondergrond.

### 3.1 Kalibratie op wit

1. Schakel de robot in.
2. Maak verbinding met het wifinetwerk `linerobot`.
3. Open de bedieningsinterface.
4. Plaats alle 11 sensoren boven een egaal wit oppervlak.
5. Voer het volgende commando uit:

```text
calibrate white
```

6. Controleer of de kalibratie correct werd uitgevoerd.

### 3.2 Kalibratie op zwart

1. Plaats alle 11 sensoren boven een egaal zwart oppervlak.
2. Zorg ervoor dat alle sensoren het zwarte oppervlak kunnen detecteren.
3. Voer het volgende commando uit:

```text
calibrate black
```

4. Controleer of de kalibratie correct werd uitgevoerd.

**Opmerking:** Gebruik een voldoende groot zwart vlak zodat alle sensoren tegelijk kunnen worden gekalibreerd. De zwarte lijn op het wedstrijdparcours is hiervoor mogelijk te smal.

### 3.3 Controle na kalibratie

Na de kalibratie kan de correcte werking worden gecontroleerd:

1. Plaats de robot op het parcours.
2. Zorg ervoor dat de zwarte lijn zich onder het midden van de sensorbalk bevindt.
3. Activeer eventueel de debugmodus.
4. Controleer of de sensoren correct reageren op de zwarte lijn.
5. Start de robot voor een korte testrit.

Het is aanbevolen om de kalibratie opnieuw uit te voeren wanneer de verlichting, de ondergrond of de sensorhoogte verandert.

---

## 4. Settings

De onderstaande **voorbeeldinstellingen** zijn gekozen als uitgangspunt voor een stabiele werking van de robot. De waarden zijn fictief en moeten op het parcours worden getest.

| Parameter | Waarde |
|---|---|
| Cycle | 5000 µs |
| Power | 180 |
| Diff | 0.65 |
| Kp | 1.80 |
| Ki | 0.02 |
| Kd | 0.85 |

### 4.1 Instellingen invoeren

Voer de volgende commando's uit via de bedieningsinterface:

```text
set cycle 5000
set power 180
set diff 0.65
set kp 1.80
set ki 0.02
set kd 0.85
```

### 4.2 Betekenis van de instellingen

**Cycle (5000 µs)**

De cyclustijd bepaalt hoe vaak de sensoren worden uitgelezen en de motorsturing wordt bijgewerkt.

Een cyclustijd van 5000 microseconden komt overeen met een regelfrequentie van 200 Hz.

**Power (180)**

Het motorvermogen wordt ingesteld met een PWM-waarde tussen 0 en 255.

Een waarde van 180 komt overeen met ongeveer 71% van de maximale PWM-waarde.

**Diff (0.65)**

Deze parameter bepaalt de verschilfactor die gebruikt wordt bij de motorbijsturing. De exacte werking hangt af van de implementatie in de software.

**Kp (1.80)**

De proportionele parameter bepaalt hoe sterk de robot reageert op een afwijking van de zwarte lijn.

**Ki (0.02)**

De integrerende parameter corrigeert aanhoudende afwijkingen van de lijn.

**Kd (0.85)**

De differentiërende parameter reageert op veranderingen in de afwijking en helpt om slingeren en overshoot te beperken.

### 4.3 Instellingen optimaliseren

Indien de robot niet stabiel rijdt, kunnen de parameters worden aangepast.

| Probleem | Mogelijke aanpassing |
|---|---|
| Robot reageert te traag | Kp verhogen |
| Robot slingert voortdurend | Kp verlagen of Kd verhogen |
| Robot vliegt uit de bocht | Power verlagen |
| Robot corrigeert te bruusk | Kp verlagen of Kd aanpassen |
| Robot heeft een blijvende afwijking | Ki voorzichtig verhogen |
| Robot rijdt te langzaam | Power geleidelijk verhogen |

Pas bij voorkeur één parameter tegelijk aan en test telkens opnieuw op hetzelfde parcours.

---

## 5. Start/stop button

De robot beschikt over twee fysieke drukknoppen waarmee hij eenvoudig kan worden gestart en gestopt.

Deze knoppen bevinden zich op de elektronica van de robot en zijn aangeduid met **SW1** en **SW2**.

### 5.1 Startknop (SW1)

De knop `SW1` wordt gebruikt om de robot te starten.

**Werking:**

1. Schakel de robot in.
2. Plaats de robot op het parcours.
3. Zorg ervoor dat de zwarte lijn zich onder de sensoren bevindt.
4. Druk op `SW1`.
5. De robot begint automatisch de zwarte lijn te volgen.

### 5.2 Stopknop (SW2)

De knop `SW2` wordt gebruikt om de robot te stoppen.

**Werking:**

1. Druk op `SW2` terwijl de robot rijdt.
2. De besturing stopt de motoren.
3. De robot blijft stilstaan.
4. De robot kan opnieuw gestart worden door op `SW1` te drukken.

### 5.3 Draadloos starten en stoppen

Naast de fysieke drukknoppen kan de robot ook draadloos worden bediend.

**Starten:**

```text
start
```

**Stoppen:**

```text
stop
```

De werking van de fysieke en draadloze stopfunctie moet worden gecontroleerd voordat de robot op volle snelheid rijdt.

---

## 6. Problemen oplossen

Tijdens het gebruik kunnen verschillende problemen optreden.

| Probleem | Mogelijke oorzaak | Oplossing |
|---|---|---|
| Robot start niet | Batterij leeg of niet aangesloten | Controleer de batterij en aansluitingen. |
| Robot volgt de lijn niet | Verkeerde sensorkalibratie | Kalibreer de sensoren opnieuw. |
| Robot slingert | PID-regeling niet goed afgesteld | Pas Kp en Kd aan. |
| Robot rijdt uit de bocht | Motorvermogen te hoog | Verlaag Power. |
| Motoren draaien niet | Motorsturing of voeding werkt niet | Controleer de motorsturing en aansluitingen. |
| Wifinetwerk niet zichtbaar | Robot staat uit of wifi is niet actief | Herstart de robot en controleer de firmware. |
| Geen reactie op commando's | Verbindings- of softwareprobleem | Controleer de verbinding en de commandoverwerking. |
| Sensorwaarden zijn fout | Verkeerde kalibratie of sensorhoogte | Controleer de sensoren en kalibreer opnieuw. |

---

## 7. Checklist voor gebruik

Controleer onderstaande punten voordat de robot wordt gestart:

- [ ] Batterij is voldoende opgeladen.
- [ ] Batterij is correct aangesloten.
- [ ] Alle elektronische componenten zijn goed bevestigd.
- [ ] De 11 lijnsensoren zijn correct gekalibreerd.
- [ ] De juiste instellingen zijn ingevoerd.
- [ ] De startknop (SW1) werkt.
- [ ] De stopknop (SW2) werkt.
- [ ] De robot staat correct op de zwarte lijn.
- [ ] Het parcours is vrij van obstakels.

Wanneer alle controles zijn uitgevoerd, is de lijnvolgende robot klaar voor gebruik.

---

## 8. Technische specificaties

| Onderdeel | Specificatie |
|---|---|
| Type robot | Lijnvolgende robot |
| Microcontroller | ESP32-S3 |
| Sensoren | 11 lijnsensoren (Q1–Q11) |
| Sensormultiplexer | CD74HC4067 |
| Motoren | 2 DC-motoren (6 V) |
| Batterij | 7,4 V LiPo (2S) |
| Spanningsregeling | DC-DC step-down converters |
| Draadloze communicatie | Wifi |
| Wifi-SSID | linerobot |
| Bediening | Webinterface en drukknoppen |
| Startknop | SW1 |
| Stopknop | SW2 |
| Regeling | PID |
| Lijndetectie | Zwarte lijn op witte ondergrond |
| Lijnbreedte parcours | 15 mm |
| Minimale bochtstraal parcours | 10 cm |

---

## 9. Veiligheid

- Schakel de robot uit voordat je elektronische aansluitingen wijzigt.
- Controleer de polariteit bij het aansluiten van de batterij.
- Vermijd kortsluiting.
- Gebruik uitsluitend een geschikte LiPo-balanslader.
- Plaats de robot op een stabiele ondergrond.
- Houd vingers en losse voorwerpen uit de buurt van draaiende wielen.
- Test nieuwe instellingen eerst met een beperkt motorvermogen.
- Controleer de stopfunctie voordat je de robot op volle snelheid laat rijden.
- Koppel de batterij los wanneer de robot langdurig niet wordt gebruikt.
