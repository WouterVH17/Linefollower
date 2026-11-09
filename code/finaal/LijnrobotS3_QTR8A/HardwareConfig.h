#pragma once
// HW-095/L298N: ENA en ENB aan (jumpers). Gebruik correcte 5V-logicavoeding.
#define LEFT_IN1  GPIO_NUM_15
#define LEFT_IN2  GPIO_NUM_16
#define RIGHT_IN1 GPIO_NUM_17
#define RIGHT_IN2 GPIO_NUM_18

// Zelfde aanname als de recente wifi-code: IN1/2 links; IN3/4 rechts.
// Test met wielen vrij. Verander de bool van de verkeerd draaiende motor.
static constexpr bool LEFT_INVERT = false;
static constexpr bool RIGHT_INVERT = true;

// QTR-8A uitgangen 1..8. Allemaal ADC1; GPIO3 (strapping) bewust overgeslagen.
// Voed de QTR-8A met 3,3 V, niet met 5 V bij rechtstreekse aansluiting.
static const gpio_num_t SENSOR_PINS[8] = {
    GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_5,
    GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9
};
// false: sensor 1 links, sensor 8 rechts, gezien in rijrichting.
// true: sensor 1 rechts, sensor 8 links. Nummering/kalibratie blijft hetzelfde.
static constexpr bool SENSOR_ORDER_REVERSED = false;
