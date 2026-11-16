
#include <Arduino.h>

constexpr uint8_t BUTTON_PIN = 10;
constexpr uint8_t LEFT_IN1 = 15;
constexpr uint8_t LEFT_IN2 = 16;
constexpr uint8_t RIGHT_IN1 = 17;
constexpr uint8_t RIGHT_IN2 = 18;

constexpr uint8_t MOTOR_PINS[] = {
  LEFT_IN1, LEFT_IN2, RIGHT_IN1, RIGHT_IN2
};

constexpr uint32_t PWM_FREQ = 20000;
constexpr uint8_t PWM_BITS = 8;
constexpr uint32_t RUN_DUTY = 160;
constexpr uint32_t DEBOUNCE_MS = 35;

portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool edgePending = false;

bool debounceActive = true;
uint32_t lastEdgeMs = 0;
int stableButton = LOW;
bool running = false;

// Hardware-interrupt bij verandering van de drukknop
void ARDUINO_ISR_ATTR onButtonChange() {
  portENTER_CRITICAL_ISR(&buttonMux);
  edgePending = true;
  portEXIT_CRITICAL_ISR(&buttonMux);
}

void setup() {
  Serial.begin(115200);

  for (uint8_t pin : MOTOR_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  // IN2 blijft laag: één draairichting
  if (!ledcAttach(LEFT_IN1, PWM_FREQ, PWM_BITS) ||
      !ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_BITS)) {
    debounceActive = false;
    Serial.println("PWM instellen mislukt.");
    return;
  }

  ledcWrite(LEFT_IN1, 0);
  ledcWrite(RIGHT_IN1, 0);

  // Externe pull-downweerstand van 10 kOhm
  // Knop tussen GPIO10 en 3.3V
  pinMode(BUTTON_PIN, INPUT);

  // Hardware-interrupt op beide flanken
  attachInterrupt(
    digitalPinToInterrupt(BUTTON_PIN),
    onButtonChange,
    CHANGE
  );

  lastEdgeMs = millis();
  Serial.println("STOP - klaar voor de eerste druk.");
}

void loop() {
  portENTER_CRITICAL(&buttonMux);
  const bool changed = edgePending;
  edgePending = false;
  portEXIT_CRITICAL(&buttonMux);

  const uint32_t now = millis();

  if (changed) {
    lastEdgeMs = now;
    debounceActive = true;
  }

  // Ontdendering van 35 ms
  if (!debounceActive ||
      now - lastEdgeMs < DEBOUNCE_MS) {
    return;
  }

  const int level = digitalRead(BUTTON_PIN);

  portENTER_CRITICAL(&buttonMux);
  const bool newEdge = edgePending;
  portEXIT_CRITICAL(&buttonMux);

  if (newEdge) {
    return;
  }

  debounceActive = false;

  if (level == stableButton) {
    return;
  }

  stableButton = level;

  // Met externe pull-down is HIGH = ingedrukt
  if (stableButton == HIGH) {
    running = !running;

    const uint32_t duty = running ? RUN_DUTY : 0;

    ledcWrite(LEFT_IN1, duty);
    ledcWrite(RIGHT_IN1, duty);

    Serial.println(running ? "START" : "STOP");
  }
}
