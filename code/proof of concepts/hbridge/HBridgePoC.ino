#include <Arduino.h>

constexpr uint8_t LEFT_IN1 = 15;
constexpr uint8_t LEFT_IN2 = 16;
constexpr uint8_t RIGHT_IN1 = 17;
constexpr uint8_t RIGHT_IN2 = 18;
constexpr uint8_t MOTOR_PINS[] = {LEFT_IN1, LEFT_IN2, RIGHT_IN1, RIGHT_IN2};
constexpr uint32_t PWM_FREQ = 20000;
constexpr uint8_t PWM_BITS = 8;
constexpr int PWM_MAX = (1 << PWM_BITS) - 1;

// Positief: IN1 krijgt PWM. Negatief: IN2 krijgt PWM.
// Bij 0 blijven beide ingangen laag en loopt de motor vrij uit.
void setMotor(uint8_t in1, uint8_t in2, int command) {
  command = constrain(command, -PWM_MAX, PWM_MAX);
  if (command >= 0) {
    ledcWrite(in2, 0);
    ledcWrite(in1, command);
  } else {
    ledcWrite(in1, 0);
    ledcWrite(in2, -command);
  }
}

void setMotors(int left, int right) {
  setMotor(LEFT_IN1, LEFT_IN2, left);
  setMotor(RIGHT_IN1, RIGHT_IN2, right);
}

void setup() {
  Serial.begin(115200);
  for (uint8_t pin : MOTOR_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  for (uint8_t pin : MOTOR_PINS) {
    if (!ledcAttach(pin, PWM_FREQ, PWM_BITS)) {
      Serial.println("PWM instellen mislukt. De motortest is afgebroken.");
      return;
    }
    ledcWrite(pin, 0);
  }

  Serial.println("Motortest start over 4 seconden.");
  delay(4000);

  Serial.println("1. Links vooruit, rechts uit.");
  setMotors(160, 0);
  delay(2000);
  setMotors(0, 0);
  delay(1500);

  Serial.println("2. Rechts vooruit, links uit.");
  setMotors(0, 160);
  delay(2000);
  setMotors(0, 0);
  delay(1500);

  Serial.println("3. Links versnelt vooruit, rechts blijft achteruit draaien.");
  setMotors(110, -160);
  for (int left = 111; left <= 220; ++left) {
    setMotor(LEFT_IN1, LEFT_IN2, left);
    delay(25);
  }
  setMotors(0, 0);
  delay(1500);

  Serial.println("4. Links achteruit, rechts vooruit.");
  setMotors(-160, 190);
  delay(2000);
  setMotors(0, 0);
  Serial.println("Klaar. Druk op RESET om de test te herhalen.");
}

void loop() {
  // De test draait eenmaal per reset. Daarna blijven beide motoren uit.
}
