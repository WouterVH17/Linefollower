#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <cstring>

constexpr uint8_t BUTTON = 10;
constexpr uint8_t LEFT_IN1 = 15;
constexpr uint8_t LEFT_IN2 = 16;
constexpr uint8_t RIGHT_IN1 = 17;
constexpr uint8_t RIGHT_IN2 = 18;
constexpr uint8_t MOTOR_PINS[] = {LEFT_IN1, LEFT_IN2, RIGHT_IN1, RIGHT_IN2};
constexpr uint16_t DEFAULT_PWM = 160;
constexpr uint32_t DEBOUNCE_MS = 35;

WebServer http(80);
WebSocketsServer socketServer(81);
portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool edgePending = false;
bool debounceActive = true;
uint32_t lastEdgeMs = 0;
int stableButton = LOW;
bool running = false;
bool ready = false;
bool storageReady = false;
uint16_t motorPwm = DEFAULT_PWM;
uint32_t lastStatusMs = 0;

const char PAGE[] PROGMEM = R"html(
<!doctype html>
<html lang="nl">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Basisprogramma lijnvolgrobot</title>
<h1>Basisprogramma lijnvolgrobot</h1>
<p id="connection">Verbinden...</p>
<p id="motor-state">Wachten op status...</p>
<form id="settings">
  <label>PWM (1–255) <input id="pwm" type="number" min="1" max="255" required></label>
  <button id="save" disabled>Opslaan</button>
</form>
<p id="result" role="status"></p>
<script>
const connection = document.getElementById("connection");
const motorState = document.getElementById("motor-state");
const pwm = document.getElementById("pwm");
const save = document.getElementById("save");
const result = document.getElementById("result");
const socket = new WebSocket("ws://" + location.hostname + ":81/", "arduino");
socket.onopen = () => { connection.textContent = "Verbonden"; };
socket.onclose = () => {
  connection.textContent = "Verbinding verbroken. Herlaad de pagina.";
  motorState.textContent = "Geen actuele status";
  save.disabled = true;
};
socket.onmessage = ({ data }) => {
  if (data.startsWith("STATUS ")) {
    const [, state, storedPwm, uptime] = data.split(" ");
    motorState.textContent = state + " | PWM " + storedPwm + " | " + uptime + " s";
    if (pwm.value === "") pwm.value = storedPwm;
    save.disabled = state !== "STOP";
  } else {
    result.textContent = data;
  }
};
document.getElementById("settings").onsubmit = (event) => {
  event.preventDefault();
  if (socket.readyState === WebSocket.OPEN) socket.send("SET " + pwm.value);
};
</script>
</html>
)html";

void ARDUINO_ISR_ATTR onButtonChange() {
  portENTER_CRITICAL_ISR(&buttonMux);
  edgePending = true;
  portEXIT_CRITICAL_ISR(&buttonMux);
}

void publishStatus() {
  char message[48];
  snprintf(message, sizeof(message), "STATUS %s %u %lu",
           running ? "START" : "STOP", static_cast<unsigned>(motorPwm),
           static_cast<unsigned long>(millis() / 1000));
  socketServer.broadcastTXT(message);
}

void onSocketEvent(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  if (type != WStype_TEXT) {
    return;
  }
  if (length < 5 || length > 7 || std::memcmp(payload, "SET ", 4) != 0) {
    socketServer.sendTXT(client, "Gebruik SET met een PWM-waarde van 1 tot 255.");
    return;
  }

  uint16_t requestedPwm = 0;
  for (size_t index = 4; index < length; ++index) {
    const uint8_t digit = payload[index];
    if (digit < '0' || digit > '9') {
      socketServer.sendTXT(client, "Ongeldige PWM-waarde.");
      return;
    }
    requestedPwm = requestedPwm * 10 + (digit - '0');
  }

  if (requestedPwm == 0 || requestedPwm > 255) {
    socketServer.sendTXT(client, "PWM moet tussen 1 en 255 liggen.");
  } else if (running) {
    socketServer.sendTXT(client, "Stop eerst de motoren met de drukknop.");
  } else if (!storageReady) {
    socketServer.sendTXT(client, "EEPROM is niet beschikbaar.");
  } else {
    if (requestedPwm != motorPwm) {
      EEPROM.put(0, requestedPwm);
      if (!EEPROM.commit()) {
        socketServer.sendTXT(client, "Opslaan mislukt. De actieve PWM blijft gelijk.");
        return;
      }
      motorPwm = requestedPwm;
      socketServer.sendTXT(client, "PWM opgeslagen in EEPROM.");
    } else {
      socketServer.sendTXT(client, "PWM is ongewijzigd.");
    }
    publishStatus();
  }
}

void handleButton() {
  portENTER_CRITICAL(&buttonMux);
  const bool changed = edgePending;
  edgePending = false;
  portEXIT_CRITICAL(&buttonMux);

  const uint32_t now = millis();
  if (changed) {
    lastEdgeMs = now;
    debounceActive = true;
  }
  if (!debounceActive || now - lastEdgeMs < DEBOUNCE_MS) {
    return;
  }

  const int level = digitalRead(BUTTON);
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
  if (level == LOW) {
    running = !running;
    const uint32_t duty = running ? motorPwm : 0;
    ledcWrite(LEFT_IN1, duty);
    ledcWrite(RIGHT_IN1, duty);
    publishStatus();
  }
}

void setup() {
  Serial.begin(115200);
  for (uint8_t pin : MOTOR_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  if (!ledcAttach(LEFT_IN1, 20000, 8) || !ledcAttach(RIGHT_IN1, 20000, 8)) {
    Serial.println("PWM instellen mislukt.");
    return;
  }
  ledcWrite(LEFT_IN1, 0);
  ledcWrite(RIGHT_IN1, 0);

  storageReady = EEPROM.begin(sizeof(uint16_t));
  if (storageReady) {
    uint16_t savedPwm = 0;
    EEPROM.get(0, savedPwm);
    if (savedPwm >= 1 && savedPwm <= 255) {
      motorPwm = savedPwm;
    }
  }

  const IPAddress ip(192, 168, 4, 1);
  if (!WiFi.mode(WIFI_AP) || !WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0)) ||
      !WiFi.softAP("Lijnrobot-PoC", "Lijnrobot123")) {
    Serial.println("Wifi-toegangspunt starten mislukt.");
    return;
  }
  http.on("/", HTTP_GET, []() {
    http.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  http.begin();
  socketServer.begin();
  socketServer.onEvent(onSocketEvent);

  pinMode(BUTTON, INPUT_PULLUP);
  attachInterrupt(BUTTON, onButtonChange, CHANGE);
  lastEdgeMs = millis();
  ready = true;
  Serial.println("Open http://192.168.4.1/");
}

void loop() {
  if (ready) {
    handleButton();
    socketServer.loop();
    http.handleClient();
    const uint32_t now = millis();
    if (now - lastStatusMs >= 1000) {
      lastStatusMs = now;
      publishStatus();
    }
  }
  delay(1);
}
