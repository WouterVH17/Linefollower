# Elektronisch ontwerp – Plan A en Plan B

## 1. Situering

Voor de ontwikkeling van de lijnvolgrobot werden twee verschillende hardwareontwerpen uitgewerkt: **Plan A en Plan B**.

Beide plannen hebben hetzelfde einddoel, namelijk het realiseren van een werkende lijnvolgrobot. De elektronische opbouw, gebruikte componenten, aansluitingen en bijbehorende schema's verschillen echter.

Het is daarom belangrijk om beide ontwerpen afzonderlijk te beoordelen.

## 2. Plan A – Zelfontworpen PCB

Plan A bestaat uit een zelfontworpen printplaat (PCB), uitgewerkt in KiCad.

Het ontwerp omvat onder andere:

- 11 infraroodleds en 11 fototransistoren.
- Weerstanden voor de sensorschakelingen.
- Een multiplexer voor het uitlezen van de sensoren.
- Een DRV8833-motordriver.
- Twee drukknoppen.
- Twee statusleds.
- Aansluitheaders voor de microcontroller.

Het elektronische schema en de Bill of Materials (BOM) van Plan A hebben uitsluitend betrekking op deze zelfontworpen PCB.

## 3. Plan B – Alternatieve hardwareopstelling

Plan B is een afzonderlijke hardwareopstelling waarbij gebruikgemaakt wordt van een ESP32-S3.

Dit ontwerp heeft een andere elektronische opbouw en maakt gebruik van andere aansluitingen en mogelijk andere componenten dan de zelfontworpen PCB van Plan A.

De code, pinconfiguratie en aansluitingen van Plan B zijn daarom niet automatisch van toepassing op Plan A.

## 4. Belangrijk onderscheid

**Plan A en Plan B zijn twee afzonderlijke hardwareontwerpen en mogen niet als één identiek elektronisch schema beschouwd worden.**

Hierdoor kunnen verschillen voorkomen in:

- Het gebruikte microcontrollerbord.
- Het aantal en type sensoren.
- De gebruikte motordriver.
- De pinbezetting.
- De weerstandswaarden.
- De elektronische componenten en hun footprints.
- De Bill of Materials.

Deze verschillen zijn het gevolg van de afzonderlijke ontwerpkeuzes en betekenen niet automatisch dat een van beide schema's foutief is.

## 5. Documentatie en beoordeling

De bestanden `LineFollower_PCB_V1.kicad_sch`, `LineFollower_PCB_V1.kicad_pcb` en `PCB_Schema.pdf` behoren tot **Plan A – Zelfontworpen PCB**.

De BOM van Plan A moet uitsluitend overeenkomen met de onderdelen uit het elektronische schema van Plan A.

De software en aansluitschema's die specifiek voor Plan B werden ontwikkeld, moeten afzonderlijk worden geïnterpreteerd.

Bij het controleren van de componenten, packages en aansluitingen moet daarom steeds rekening gehouden worden met het plan waarop de bestanden betrekking hebben.

## 6. Conclusie

Er zijn bewust twee verschillende hardwareontwerpen uitgewerkt voor dezelfde lijnvolgrobot.

**Plan A is het ontwerp van de zelfgemaakte PCB. Plan B is de alternatieve hardwareopstelling.**

De BOM, elektronische schema's en componentkeuzes van Plan A mogen daarom niet rechtstreeks vergeleken of gelijkgesteld worden met die van Plan B.

Elk ontwerp moet beoordeeld worden aan de hand van zijn eigen elektronische schema, componentenlijst en bijbehorende documentatie.
