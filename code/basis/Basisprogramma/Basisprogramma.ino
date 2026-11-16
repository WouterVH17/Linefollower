
#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// =====================================================
// 1. HARDWARE
// =====================================================

// ESP32-S3
constexpr uint8_t BUTTON = 10;

constexpr uint8_t LEFT_IN1  = 15;
constexpr uint8_t LEFT_IN2  = 16;
constexpr uint8_t RIGHT_IN1 = 17;
constexpr uint8_t RIGHT_IN2 = 18;

constexpr uint8_t MOTOR_PINS[] = {
  LEFT_IN1, LEFT_IN2, RIGHT_IN1, RIGHT_IN2
};

// PWM - Arduino ESP32-core 3.x
constexpr uint32_t PWM_FREQ = 20000;
constexpr uint8_t PWM_BITS = 8;

// Drukknop: externe pull-down van 10 kOhm
constexpr uint32_t DEBOUNCE_MS = 35;

// =====================================================
// 2. STANDAARDINSTELLINGEN
// =====================================================

constexpr uint16_t DEFAULT_PWM = 160;
constexpr uint16_t DEFAULT_CYCLE_MS = 20;

constexpr uint16_t MIN_CYCLE_MS = 10;
constexpr uint16_t MAX_CYCLE_MS = 1000;

// Identificatie van opgeslagen EEPROM-gegevens
constexpr uint32_t EEPROM_MAGIC = 0x4C523031UL;

struct Settings {
  uint32_t magic;
  uint16_t pwm;
  uint16_t cycleMs;
  uint8_t debug;
  uint8_t reserved[3];
};

Settings settings = {
  EEPROM_MAGIC,
  DEFAULT_PWM,
  DEFAULT_CYCLE_MS,
  0,
  {0, 0, 0}
};

// Eigen EEPROM-namespace voor dit basisprogramma
EEPROMClass configEEPROM("linebot_cfg2");

bool storageReady = false;

// =====================================================
// 3. WIFI EN WEBSOCKET
// =====================================================

constexpr char AP_NAME[] = "Lijnrobot-PoC";
constexpr char AP_PASSWORD[] = "Lijnrobot123";

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_MASK(255, 255, 255, 0);

WebServer http(80);
WebSocketsServer socketServer(81);

bool ready = false;
bool running = false;

// =====================================================
// 4. INTERRUPT EN ONTDENDERING
// =====================================================

portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool edgePending = false;

bool debounceActive = false;
uint32_t lastEdgeMs = 0;
int stableButton = LOW;

// Hardware-interrupt
void ARDUINO_ISR_ATTR onButtonChange() {
  portENTER_CRITICAL_ISR(&buttonMux);
  edgePending = true;
  portEXIT_CRITICAL_ISR(&buttonMux);
}

// =====================================================
// 5. CYCLUS EN TIMING
// =====================================================

uint32_t lastCycleUs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastDebugMs = 0;

uint32_t cycleCounter = 0;
uint32_t missedCycles = 0;

// =====================================================
// 6. WEBINTERFACE
// =====================================================

const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="nl">
<head>
<meta charset="utf-8">
<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>Basisprogramma lijnvolgrobot</title>

<style>
body {
  font-family: Arial, sans-serif;
  max-width: 620px;
  margin: 30px auto;
  padding: 16px;
  background: #f5f6f8;
  color: #222;
}

h1 {
  font-size: 25px;
}

section {
  background: white;
  padding: 20px;
  margin-bottom: 15px;
  border-radius: 10px;
}

label {
  display: block;
  margin-bottom: 10px;
  font-weight: bold;
}

input {
  box-sizing: border-box;
  width: 100%;
  padding: 10px;
  font-size: 16px;
  margin-bottom: 10px;
}

button {
  padding: 12px 18px;
  background: #185a4a;
  color: white;
  border: 0;
  border-radius: 6px;
  cursor: pointer;
  margin-bottom: 6px;
}

button:disabled {
  background: #aaa;
  cursor: default;
}

#result {
  overflow-wrap: anywhere;
}
</style>
</head>

<body>

<h1>Basisprogramma lijnvolgrobot</h1>

<section>
  <h2>Verbinding</h2>
  <p id="connection">Verbinden...</p>
  <p id="motor-state">Status wordt geladen...</p>
  <p id="uptime"></p>
  <p id="storage"></p>
  <p>
    Start/stop gebeurt met de fysieke
    drukknop op GPIO10.
  </p>
</section>

<section>
  <h2>Motorinstellingen</h2>

  <form id="pwmForm">
    <label for="pwm">PWM (1 - 255)</label>
    <input id="pwm"
           type="number"
           min="1"
           max="255"
           required>
    <button id="savePwm" disabled>
      PWM opslaan
    </button>
  </form>
</section>

<section>
  <h2>Cyclustijd</h2>

  <form id="cycleForm">
    <label for="cycle">
      Cyclustijd (10 - 1000 ms)
    </label>
    <input id="cycle"
           type="number"
           min="10"
           max="1000"
           required>
    <button id="saveCycle" disabled>
      Cyclustijd opslaan
    </button>
  </form>
</section>

<section>
  <h2>Debugmodus</h2>
  <p id="debugState">Onbekend</p>

  <button id="debugButton" disabled>
    Debug wijzigen
  </button>
</section>

<section>
  <h2>Communicatie</h2>
  <button id="pingButton" disabled>
    PING versturen
  </button>
  <p id="result" role="status"></p>
</section>

<script>
const connection =
  document.getElementById("connection");

const motorState =
  document.getElementById("motor-state");

const uptime =
  document.getElementById("uptime");

const storage =
  document.getElementById("storage");

const pwmInput =
  document.getElementById("pwm");

const cycleInput =
  document.getElementById("cycle");

const debugState =
  document.getElementById("debugState");

const result =
  document.getElementById("result");

const savePwm =
  document.getElementById("savePwm");

const saveCycle =
  document.getElementById("saveCycle");

const debugButton =
  document.getElementById("debugButton");

const pingButton =
  document.getElementById("pingButton");

let connected = false;
let running = false;
let debugEnabled = false;

const socket = new WebSocket(
  "ws://" + location.hostname + ":81/",
  "arduino"
);

function updateButtons() {
  savePwm.disabled = !connected || running;
  saveCycle.disabled = !connected;
  debugButton.disabled = !connected;
  pingButton.disabled = !connected;
}

function sendCommand(command) {
  if (socket.readyState === WebSocket.OPEN) {
    socket.send(command);
  }
}

socket.onopen = () => {
  connected = true;
  connection.textContent = "Verbonden";
  updateButtons();
  sendCommand("STATUS");
};

socket.onclose = () => {
  connected = false;
  connection.textContent =
    "Verbinding verbroken. Herlaad de pagina.";
  motorState.textContent = "Status onbekend";
  updateButtons();
};

socket.onmessage = (event) => {
  const data = String(event.data);

  if (data.startsWith("STATUS ")) {
    const parts = data.split(" ");

    const state = parts[1];
    const pwm = parts[2];
    const cycle = parts[3];
    const debug = parts[4];
    const seconds = parts[5];
    const eeprom = parts[6];

    running = (state === "START");
    debugEnabled = (debug === "ON");

    motorState.textContent =
      "Motoren: " + state;

    uptime.textContent =
      "Looptijd: " + seconds + " s";

    storage.textContent =
      "EEPROM: " + eeprom;

    if (document.activeElement !== pwmInput) {
      pwmInput.value = pwm;
    }

    if (document.activeElement !== cycleInput) {
      cycleInput.value = cycle;
    }

    debugState.textContent =
      "Debug: " + debug;

    debugButton.textContent =
      debugEnabled ? "Debug uitschakelen"
                   : "Debug inschakelen";

    updateButtons();
  } else {
    result.textContent = data;
  }
};

document.getElementById("pwmForm")
  .addEventListener("submit", (event) => {
    event.preventDefault();
    sendCommand("SET " + pwmInput.value);
  });

document.getElementById("cycleForm")
  .addEventListener("submit", (event) => {
    event.preventDefault();
    sendCommand("CYCLE " + cycleInput.value);
  });

debugButton.onclick = () => {
  sendCommand(
    debugEnabled ? "DEBUG OFF" : "DEBUG ON"
  );
};

pingButton.onclick = () => {
  sendCommand("PING");
};
</script>

</body>
</html>
)HTML";

// =====================================================
// 7. EEPROM
// =====================================================

void loadSettings() {
  storageReady =
    configEEPROM.begin(sizeof(Settings));

  if (!storageReady) {
    Serial.println("EEPROM initialisatie mislukt.");
    return;
  }

  Settings saved = {};
  configEEPROM.get(0, saved);

  if (saved.magic == EEPROM_MAGIC &&
      saved.pwm >= 1 &&
      saved.pwm <= 255 &&
      saved.cycleMs >= MIN_CYCLE_MS &&
      saved.cycleMs <= MAX_CYCLE_MS &&
      saved.debug <= 1) {

    settings = saved;
    Serial.println("Instellingen uit EEPROM geladen.");

  } else {
    Serial.println(
      "Geen geldige instellingen. Standaardwaarden actief."
    );
  }
}

bool saveSettings(const Settings& updated) {
  if (!storageReady) {
    return false;
  }

  configEEPROM.put(0, updated);

  if (!configEEPROM.commit()) {
    return false;
  }

  settings = updated;
  return true;
}

// =====================================================
// 8. MOTORSTURING
// =====================================================

void applyMotorOutputs() {
  // IN2 blijft LOW: voor deze proef één richting.
  const uint32_t duty =
    running ? settings.pwm : 0;

  ledcWrite(LEFT_IN1, duty);
  ledcWrite(RIGHT_IN1, duty);
}

// =====================================================
// 9. STATUS EN COMMUNICATIE
// =====================================================

void createStatus(char* message, size_t size) {
  snprintf(
    message,
    size,
    "STATUS %s %u %u %s %lu %s",
    running ? "START" : "STOP",
    static_cast<unsigned>(settings.pwm),
    static_cast<unsigned>(settings.cycleMs),
    settings.debug ? "ON" : "OFF",
    static_cast<unsigned long>(millis() / 1000),
    storageReady ? "OK" : "FOUT"
  );
}

void publishStatus() {
  char message[96];
  createStatus(message, sizeof(message));
  socketServer.broadcastTXT(message);
}

void sendStatus(int client) {
  char message[96];
  createStatus(message, sizeof(message));

  if (client < 0) {
    Serial.println(message);
  } else {
    socketServer.sendTXT(
      static_cast<uint8_t>(client),
      message
    );
  }
}

void reply(int client, const char* message) {
  if (client < 0) {
    Serial.println(message);
  } else {
    socketServer.sendTXT(
      static_cast<uint8_t>(client),
      message
    );
  }
}

// Controleer getallen zonder ongeldige tekens
bool parseNumber(
  const String& text,
  uint32_t minimum,
  uint32_t maximum,
  uint32_t& value
) {
  if (text.length() == 0 || text.length() > 10) {
    return false;
  }

  uint32_t number = 0;

  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];

    if (c < '0' || c > '9') {
      return false;
    }

    number = number * 10 + (c - '0');

    if (number > maximum) {
      return false;
    }
  }

  if (number < minimum) {
    return false;
  }

  value = number;
  return true;
}

// =====================================================
// 10. INSTELBARE COMMANDO'S
// =====================================================

void handleCommand(String command, int client) {
  command.trim();
  command.toUpperCase();

  // Status opvragen
  if (command == "STATUS") {
    sendStatus(client);
    return;
  }

  // Test communicatie
  if (command == "PING") {
    reply(client, "PONG");
    return;
  }

  // PWM wijzigen
  if (command.startsWith("SET ") ||
      command.startsWith("PWM ")) {

    uint32_t requested = 0;

    int separator = command.indexOf(' ');

    if (!parseNumber(
          command.substring(separator + 1),
          1, 255, requested)) {
      reply(client, "PWM moet tussen 1 en 255 liggen.");
      return;
    }

    if (running) {
      reply(
        client,
        "Stop eerst de motoren met de drukknop."
      );
      return;
    }

    Settings updated = settings;
    updated.pwm = static_cast<uint16_t>(requested);

    if (requested != settings.pwm) {
      if (!saveSettings(updated)) {
        reply(client, "EEPROM opslaan mislukt.");
        return;
      }
      reply(client, "PWM opgeslagen in EEPROM.");
    } else {
      reply(client, "PWM ongewijzigd.");
    }

    publishStatus();
    return;
  }

  // Cyclustijd wijzigen
  if (command.startsWith("CYCLE ")) {
    uint32_t requested = 0;

    if (!parseNumber(
          command.substring(6),
          MIN_CYCLE_MS,
          MAX_CYCLE_MS,
          requested)) {
      reply(
        client,
        "Cyclustijd moet tussen 10 en 1000 ms liggen."
      );
      return;
    }

    Settings updated = settings;
    updated.cycleMs =
      static_cast<uint16_t>(requested);

    if (updated.cycleMs != settings.cycleMs) {
      if (!saveSettings(updated)) {
        reply(client, "EEPROM opslaan mislukt.");
        return;
      }

      // Planning opnieuw starten met nieuw interval
      lastCycleUs = micros();

      reply(client, "Cyclustijd opgeslagen.");
    } else {
      reply(client, "Cyclustijd ongewijzigd.");
    }

    publishStatus();
    return;
  }

  // Debugmodus wijzigen
  if (command == "DEBUG ON" ||
      command == "DEBUG OFF") {

    Settings updated = settings;
    updated.debug =
      (command == "DEBUG ON") ? 1 : 0;

    if (updated.debug != settings.debug) {
      if (!saveSettings(updated)) {
        reply(client, "EEPROM opslaan mislukt.");
        return;
      }
    }

    reply(
      client,
      settings.debug
        ? "Debugmodus ingeschakeld."
        : "Debugmodus uitgeschakeld."
    );

    publishStatus();
    return;
  }

  reply(
    client,
    "Commando's: SET 160, CYCLE 20, "
    "DEBUG ON, DEBUG OFF, STATUS, PING."
  );
}

// =====================================================
// 11. WEBSOCKET-EVENTS
// =====================================================

void onSocketEvent(
  uint8_t client,
  WStype_t type,
  uint8_t* payload,
  size_t length
) {
  if (type == WStype_CONNECTED) {
    sendStatus(client);
    return;
  }

  if (type != WStype_TEXT) {
    return;
  }

  if (length == 0 || length > 48) {
    reply(client, "Ongeldige commandolengte.");
    return;
  }

  String command;
  command.reserve(length + 1);

  for (size_t i = 0; i < length; i++) {
    if (payload[i] < 32 || payload[i] > 126) {
      reply(client, "Ongeldig commando.");
      return;
    }

    command += static_cast<char>(payload[i]);
  }

  handleCommand(command, client);
}

// =====================================================
// 12. COMMANDO'S VIA SERIELE MONITOR
// =====================================================

char serialLine[48];
size_t serialLength = 0;

void handleSerial() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      serialLine[serialLength] = '\0';

      if (serialLength > 0) {
        handleCommand(String(serialLine), -1);
      }

      serialLength = 0;
      continue;
    }

    if (serialLength < sizeof(serialLine) - 1) {
      serialLine[serialLength++] = c;
    } else {
      serialLength = 0;
      Serial.println("Commando te lang.");
    }
  }
}

// =====================================================
// 13. START/STOP MET INTERRUPT EN ONTDENDERING
// =====================================================

void handleButton() {
  bool changed = false;

  portENTER_CRITICAL(&buttonMux);
  changed = edgePending;
  edgePending = false;
  portEXIT_CRITICAL(&buttonMux);

  const uint32_t now = millis();

  if (changed) {
    lastEdgeMs = now;
    debounceActive = true;
  }

  if (!debounceActive) {
    return;
  }

  // Wacht 35 ms na de laatste gedetecteerde flank
  if (now - lastEdgeMs < DEBOUNCE_MS) {
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

  // Externe pull-down:
  // LOW = niet ingedrukt
  // HIGH = ingedrukt

  if (stableButton == HIGH) {
    running = !running;

    // Motoren onmiddellijk schakelen
    applyMotorOutputs();

    Serial.println(
      running ? "START - motoren draaien"
              : "STOP - motoren gestopt"
    );

    publishStatus();
  }
}

// =====================================================
// 14. VASTE UITVOERINGSCYCLUS
// =====================================================

void executeCycle() {
  cycleCounter++;

  // Basisprogramma:
  // motorsturing wordt in elke cyclus bijgewerkt.
  //
  // Later kan hier de sensorverwerking en
  // PID-regeling van de lijnvolgrobot komen.

  applyMotorOutputs();

  // Debug-uitvoer maximaal eenmaal per seconde
  const uint32_t now = millis();

  if (settings.debug &&
      now - lastDebugMs >= 1000) {

    lastDebugMs = now;

    Serial.printf(
      "[DEBUG] Cyclus=%lu | Gemist=%lu | "
      "Tijd=%u ms | Motor=%s | PWM=%u\n",
      static_cast<unsigned long>(cycleCounter),
      static_cast<unsigned long>(missedCycles),
      static_cast<unsigned>(settings.cycleMs),
      running ? "START" : "STOP",
      static_cast<unsigned>(settings.pwm)
    );
  }
}

void handleFixedCycle() {
  const uint32_t nowUs = micros();

  const uint32_t periodUs =
    static_cast<uint32_t>(settings.cycleMs) * 1000UL;

  const uint32_t elapsed = nowUs - lastCycleUs;

  if (elapsed < periodUs) {
    return;
  }

  // Verloren periodes overslaan bij vertraging
  // zodat de planning niet blijft achterlopen.
  const uint32_t periods = elapsed / periodUs;

  lastCycleUs += periods * periodUs;

  if (periods > 1) {
    missedCycles += periods - 1;
  }

  executeCycle();
}

// =====================================================
// 15. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("================================");
  Serial.println("Basisprogramma lijnvolgrobot");
  Serial.println("ESP32-S3 - Arduino-core 3.x");
  Serial.println("================================");

  // Motorpinnen veilig initialiseren
  for (uint8_t pin : MOTOR_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  // PWM instellen: core 3.x API
  const bool leftPwmOk =
    ledcAttach(LEFT_IN1, PWM_FREQ, PWM_BITS);

  const bool rightPwmOk =
    ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_BITS);

  if (!leftPwmOk || !rightPwmOk) {
    if (leftPwmOk) {
      ledcWrite(LEFT_IN1, 0);
    }

    if (rightPwmOk) {
      ledcWrite(RIGHT_IN1, 0);
    }

    Serial.println("PWM initialisatie mislukt.");
    return;
  }

  applyMotorOutputs();

  // Instellingen laden
  loadSettings();

  Serial.printf(
    "PWM=%u | Cyclus=%u ms | Debug=%s\n",
    static_cast<unsigned>(settings.pwm),
    static_cast<unsigned>(settings.cycleMs),
    settings.debug ? "ON" : "OFF"
  );

  // Knop met externe 10k pull-down
  pinMode(BUTTON, INPUT);

  // Huidige toestand als beginsituatie nemen
  stableButton = digitalRead(BUTTON);

  // Hardware-interrupt op beide flanken
  attachInterrupt(
    digitalPinToInterrupt(BUTTON),
    onButtonChange,
    CHANGE
  );

  // WiFi-accesspoint starten
  if (!WiFi.mode(WIFI_AP) ||
      !WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK) ||
      !WiFi.softAP(AP_NAME, AP_PASSWORD)) {

    Serial.println("WiFi starten mislukt.");
    return;
  }

  // HTTP-webpagina
  http.on("/", HTTP_GET, []() {
    http.send_P(
      200,
      "text/html; charset=utf-8",
      PAGE
    );
  });

  http.begin();

  // WebSocket-server
  socketServer.onEvent(onSocketEvent);
  socketServer.begin();

  // Start timers
  lastCycleUs = micros();
  lastStatusMs = millis();
  lastDebugMs = millis();

  ready = true;

  Serial.println("Systeem klaar.");
  Serial.print("WiFi: ");
  Serial.println(AP_NAME);

  Serial.print("Webinterface: http://");
  Serial.println(WiFi.softAPIP());

  Serial.println("Beschikbare commando's:");
  Serial.println("SET 160");
  Serial.println("CYCLE 20");
  Serial.println("DEBUG ON");
  Serial.println("DEBUG OFF");
  Serial.println("STATUS");
  Serial.println("PING");
}

// =====================================================
// 16. MAIN LOOP
// =====================================================

void loop() {
  if (!ready) {
    delay(100);
    return;
  }

  // 1. Drukknop verwerken
  handleButton();

  // 2. Communicatie verwerken
  socketServer.loop();
  http.handleClient();
  handleSerial();

  // 3. Periodieke uitvoeringscyclus
  handleFixedCycle();

  // 4. Status iedere seconde versturen
  const uint32_t now = millis();

  if (now - lastStatusMs >= 1000) {
    lastStatusMs = now;
    publishStatus();
  }

  // CPU/WiFi-taken tijd geven
  delay(1);
}
