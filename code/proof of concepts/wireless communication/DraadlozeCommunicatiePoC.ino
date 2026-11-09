#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <cstring>

constexpr char AP_NAME[] = "Lijnrobot-PoC";
constexpr char AP_PASSWORD[] = "Lijnrobot123";
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_MASK(255, 255, 255, 0);

WebServer http(80);
WebSocketsServer socketServer(81);
bool ready = false;
uint32_t lastTelemetryMs = 0;

const char PAGE[] PROGMEM = R"html(
<!doctype html>
<html lang="nl">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Draadloze proef lijnvolgrobot</title>
<h1>Draadloze proef lijnvolgrobot</h1>
<p>Verbinding <strong id="status">Bezig met verbinden</strong></p>
<p>Looptijd ESP32-S3 <strong id="uptime">Wachten op bericht</strong></p>
<button id="ping" type="button" disabled>Verstuur PING</button>
<p>PONG ontvangen <strong id="count">0</strong></p>
<p id="reply" role="status"></p>
<script>
const status = document.getElementById("status");
const uptime = document.getElementById("uptime");
const ping = document.getElementById("ping");
const reply = document.getElementById("reply");
const count = document.getElementById("count");
let replies = 0;
const ws = new WebSocket("ws://" + location.hostname + ":81/", "arduino");

ws.onopen = () => {
  status.textContent = "Verbonden";
  ping.disabled = false;
};
ws.onclose = () => {
  status.textContent = "Verbroken. Herlaad de pagina om opnieuw te verbinden.";
  uptime.textContent = "Geen verbinding";
  ping.disabled = true;
};
ws.onmessage = ({ data }) => {
  if (data.startsWith("UPTIME ")) {
    uptime.textContent = data.slice(7) + " s";
  } else if (data === "PONG") {
    count.textContent = String(++replies);
    reply.textContent = "ESP32-S3 heeft PONG teruggestuurd.";
  }
};
ping.onclick = () => {
  if (ws.readyState === WebSocket.OPEN) {
    reply.textContent = "Laptop heeft PING verstuurd.";
    ws.send("PING");
  }
};
</script>
</html>
)html";

void onSocketEvent(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT && length == 4 && std::memcmp(payload, "PING", 4) == 0) {
    Serial.println("PING ontvangen. PONG wordt teruggestuurd.");
    socketServer.sendTXT(client, "PONG");
  }
}

void setup() {
  Serial.begin(115200);
  if (!WiFi.mode(WIFI_AP) ||
      !WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK) ||
      !WiFi.softAP(AP_NAME, AP_PASSWORD)) {
    Serial.println("Wifi-toegangspunt starten mislukt.");
    return;
  }

  http.on("/", HTTP_GET, []() {
    http.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  http.begin();
  socketServer.begin();
  socketServer.onEvent(onSocketEvent);
  ready = true;

  Serial.print("Open in de browser: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  if (!ready) {
    delay(100);
    return;
  }

  socketServer.loop();
  http.handleClient();

  const uint32_t now = millis();
  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    char telemetry[24];
    snprintf(telemetry, sizeof(telemetry), "UPTIME %lu",
             static_cast<unsigned long>(now / 1000));
    socketServer.broadcastTXT(telemetry);
  }
  delay(1);
}
