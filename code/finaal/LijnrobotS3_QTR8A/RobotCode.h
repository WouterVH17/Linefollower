#pragma once
// Arduino-ESP32 3.3.11, ESP32-S3.
#include <Arduino.h>
#include <WiFi.h>
#include <cmath>
using std::isfinite;
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error Selecteer ESP32S3 Dev Module in Arduino IDE.
#endif

// ==================================================
// PINNEN
// ==================================================

#include "HardwareConfig.h"

#define SENSOR_COUNT 8
#define CHANNEL_COUNT SENSOR_COUNT
// Gelijkmatig verdeelde QTR-sensoren, schaal +/-5000 zoals de oude PID.
static const int SENSOR_POSITION[SENSOR_COUNT] = {
    -5000, -3571, -2143, -714, 714, 2143, 3571, 5000
};

// ==================================================
// REGELING EN VEILIGHEID
// ==================================================

#define MAX_PWM_VALUE tuning.max_pwm
#define KP tuning.kp
#define KI tuning.ki
#define KD tuning.kd

#define LINE_RECOVERY_MS tuning.recovery_ms
#define RECOVERY_PWM_VALUE tuning.recovery_pwm
#define RECOVERY_MIN_POSITION 300

#define PWM_MODE LEDC_LOW_SPEED_MODE
#define PWM_TIMER LEDC_TIMER_0
#define LEFT_CHANNEL LEDC_CHANNEL_0
#define RIGHT_CHANNEL LEDC_CHANNEL_1
#define LEFT_REVERSE_CHANNEL LEDC_CHANNEL_2
#define RIGHT_REVERSE_CHANNEL LEDC_CHANNEL_3
#define PWM_FREQUENCY_HZ 5000
#define PWM_MAX_DUTY 1023


#define REFERENCE_CAPTURE_US 2000000U
#define START_DELAY_MS 3000


#define MIN_CALIBRATION_SPAN 200
#define LINE_NOISE_THRESHOLD 100
#define LINE_MIN_PEAK 300
#define LINE_MIN_SUM 500
#define DERIVATIVE_FILTER_US 10000.0f // 0 = D-filter uit
#define JUNCTION_MIN_SENSORS 5

typedef enum {
    STATE_IDLE,
    STATE_WHITE,
    STATE_BLACK,
    STATE_COUNTDOWN,
    STATE_RUNNING
} robot_state_t;

enum PhoneCommand {
    PHONE_NONE,
    PHONE_WHITE,
    PHONE_BLACK,
    PHONE_CLEAR,
    PHONE_START
};

typedef struct {
    float kp;
    float ki;
    float kd;
    float power;
    float diff;
    uint32_t cycle_time_us;
    float max_pwm;
    float recovery_pwm;
    int recovery_ms;
} tuning_t;

static const char *TAG = "robot";
// PID, power/diff, cyclustijd, maximale PWM, herstel-PWM en herstelduur.
static tuning_t tuning = {9, 0, 0.015f, 100, 0.5f, 10000, 180, 25, 60};
static tuning_t requested = {9, 0, 0.015f, 100, 0.5f, 10000, 180, 25, 60};

static bool tuning_valid(const tuning_t *p) {
    return isfinite(p->kp) && p->kp >= 0.0f &&
        isfinite(p->ki) && p->ki >= 0.0f && p->ki <= 50.0f &&
        isfinite(p->kd) && p->kd >= 0.0f && p->kd <= 50.0f &&
        isfinite(p->power) && p->power >= 0.0f && p->power <= 255.0f &&
        isfinite(p->diff) && p->diff >= 0.0f && p->diff <= 1.0f &&
        p->cycle_time_us >= 1000 && p->cycle_time_us <= 1000000 &&
        p->cycle_time_us % 100 == 0 &&
        isfinite(p->max_pwm) && p->max_pwm >= 0.0f && p->max_pwm <= 255.0f &&
        isfinite(p->recovery_pwm) &&
        p->recovery_pwm >= 0.0f && p->recovery_pwm <= 255.0f &&
        p->recovery_ms >= 0 && p->recovery_ms <= 500 &&
        p->recovery_pwm <= p->max_pwm;
}

static esp_err_t tuning_load(void) {
    nvs_handle_t handle;
    esp_err_t error = nvs_open("robot_qtr8a", NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Geen opgeslagen rijinstellingen; standaardwaarden gebruiken");
        return ESP_OK;
    }
    if (error != ESP_OK) return error;

    size_t size = 0;
    error = nvs_get_blob(handle, "tuning", NULL, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        ESP_LOGI(TAG, "Geen opgeslagen rijinstellingen; standaardwaarden gebruiken");
        return ESP_OK;
    }
    if (error != ESP_OK) {
        nvs_close(handle);
        return error;
    }
    if (size != sizeof(tuning_t)) {
        nvs_close(handle);
        ESP_LOGW(TAG, "Opgeslagen rijinstellingen hebben een oude indeling; standaardwaarden gebruiken");
        return ESP_OK;
    }

    tuning_t stored;
    error = nvs_get_blob(handle, "tuning", &stored, &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;
    if (!tuning_valid(&stored)) {
        ESP_LOGW(TAG, "Opgeslagen rijinstellingen zijn ongeldig; standaardwaarden gebruiken");
        return ESP_OK;
    }

    tuning = stored;
    requested = stored;
    ESP_LOGI(TAG, "Opgeslagen rijinstellingen geladen");
    return ESP_OK;
}

static esp_err_t tuning_save(const tuning_t *p) {
    nvs_handle_t handle;
    esp_err_t error = nvs_open("robot_qtr8a", NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_set_blob(handle, "tuning", p, sizeof(*p));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

static portMUX_TYPE web_lock = portMUX_INITIALIZER_UNLOCKED;

static bool tuning_pending = false;
static bool stop_requested = false;
static bool debug_enabled = false;


static PhoneCommand pending_command = PHONE_NONE;
static uint32_t command_revision = 0;
static uint32_t command_applied = 0;
static char command_result[96] = "";

static bool web_white = false;
static bool web_black = false;
static unsigned web_samples = 0;

static char web_state[48] = "OPSTARTEN";
static char web_reason[80] = "";

static float web_left = 0;
static float web_right = 0;
static bool web_calibrated = false;
static bool web_line = false;
static int web_position = 0;
static int web_raw[SENSOR_COUNT] = {};
static int web_normalized[SENSOR_COUNT] = {};
static int web_white_reference[SENSOR_COUNT] = {};
static int web_black_reference[SENSOR_COUNT] = {};
static uint32_t web_max_calculation_us = 0;
static bool timing_reset_requested = false;
static uint32_t timing_samples = 0;
static uint32_t deadline_misses = 0;
static uint32_t junction_count = 0;
static int64_t junction_interval_us = 0;
static bool control_active = false;
static bool config_writing = false;
static TaskHandle_t robot_task_handle;
static esp_timer_handle_t cycle_timer;

// Timer callback doet uitsluitend een taakmelding; geen ADC/PID/printf hier.
static void cycle_wakeup(void *arg) {
    xTaskNotifyGive((TaskHandle_t)arg);
}

static bool begin_config_write(void) {
    portENTER_CRITICAL(&web_lock);
    bool allowed = !control_active && !config_writing;
    if (allowed) config_writing = true;
    portEXIT_CRITICAL(&web_lock);
    return allowed;
}
static void end_config_write(void) {
    portENTER_CRITICAL(&web_lock);
    config_writing = false;
    portEXIT_CRITICAL(&web_lock);
}

static esp_err_t calibration_load(void);
static esp_err_t calibration_save(const char *key, const int *values);
static esp_err_t calibration_erase(void);

static uint32_t requested_revision = 0;
static uint32_t applied_revision = 0;

// ==================================================
// WEBPAGINA
// ==================================================

static const char web_html[] = R"ROBOTHTML(
<!doctype html>
<html lang="nl">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lijnrobot S3 - QTR-8A</title>

<style>
body {
  font:18px system-ui;
  max-width:560px;
  margin:20px auto;
  padding:16px;
  background:#eef2f5;
}
button {
  font:inherit;
  padding:14px;
  border:0;
  border-radius:8px;
  background:#175cd3;
  color:white;
  margin:6px 0;
  width:100%;
}
button:disabled { opacity:.45; }
#stop { background:#b42318; }
label { display:block; margin:16px 0; }
input { float:right; width:110px; font:inherit; }
small { display:block; }
.row { display:flex; gap:10px; }
#message { min-height:48px; }
#connection { font-weight:bold; }
</style>

<h1>Lijnrobot S3 - QTR-8A</h1>
<p id="connection">Verbinden...</p>
<p id="status">Robotstatus ophalen...</p>
<small id="timing">Regelcyclus ophalen...</small>

<button id="stop" type="button">STOP ROBOT</button>

<h2>Kalibreren en starten</h2>

<p>
1. Plaats alle sensoren boven wit en druk Wit meten.
Houd de robot stil tot de meting klaar is.
</p>

<p>
2. Plaats alle sensoren boven zwart en druk Zwart meten.
Houd de robot opnieuw stil.
</p>

<div class="row">
  <button id="white" class="command">Wit meten</button>
  <button id="black" class="command">Zwart meten</button>
</div>

<p id="calibration">Kalibratie ophalen...</p>
<button id="clear" class="command">Kalibratie wissen</button>

<p>
3. Plaats de robot op de lijn.
Start geeft 3 seconden aftellen. Druk op STOP om te stoppen.
</p>

<button id="start" class="command">START ROBOT</button>
<h2>Sensoren rechtstreeks op ADC1</h2>
<small>Ruw: 0-4095. Genormaliseerd: wit=0, zwart=1000.
Sensor 1 links in rijrichting; pas SENSOR_ORDER_REVERSED aan als dat omgekeerd is.</small>
<table style="width:100%;text-align:center"><thead><tr><th>Sensor</th><th>GPIO</th><th>Ruw</th><th>Wit</th><th>Zwart</th><th>Lijn</th></tr></thead><tbody id="sensorRows"></tbody></table>

<p id="message" aria-live="polite"></p>

<h2>Instellingen</h2>

<form id="form">
<label>Kp
<input name="kp" type="number"
       min="0" step="0.1" required>
</label>

<label>Ki (per seconde)
<input name="ki" type="number"
       min="0" max="50" step="0.001" required>
</label>

<label>Kd (seconden)
<input name="kd" type="number"
       min="0" max="50" step="0.001" required>
</label>

<label>Power (0–255)
<input name="power" type="number"
       min="0" max="255" step="1" required>
</label>

<label>Diff (0–1)
<input name="diff" type="number"
       min="0" max="1" step="0.01" required>
</label>

<label>Cyclustijd (µs)
<input name="cycle_time_us" type="number"
       min="1000" max="1000000" step="100" required>
</label>

<small>Kies minimaal tweemaal de gemeten maximale rekentijd. Serieel: set cycle auto. Instelbaar per 100 µs, vanaf 1000 µs. Controleer opnieuw tijdens rijden.</small>

<label>Max PWM (0–255)
<input name="max_pwm" type="number"
       min="0" max="255" step="1" required>
</label>

<small>Bij lijnverlies blijft de robot in de laatst bekende bochtrichting draaien tot de lijn terug is of je STOP drukt.</small>
<label>Wachttijd zonder bochtrichting (ms)
<input name="recovery_ms" type="number"
       min="0" max="500" step="1" required>
</label>

<label>Herstel PWM (0–255)
<input name="recovery_pwm" type="number"
       min="0" max="255" step="1" required>
</label>

<button id="apply">Live toepassen</button>
</form>

<small>
Instellingen blijven na herstart bewaard; ook de kalibratie blijft na herstart bewaard.
Bediening via deze pagina of serieel; geen fysieke knoppen of lampen aangesloten.
Wifi: Robot-S3, http://192.168.4.1.
Serieel (115200 baud): set kp 0, set cycle 10000, debug, start, stop.
</small>

<script>
const el = id => document.getElementById(id);
const form = el('form');

let loaded = false;
let revision = 0;
let command = 0;
let last = null;
let sending = false;

async function api(url, data) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 4000);

  try {
    const response = await fetch(url, {
      cache: 'no-store',
      method: data === undefined ? 'GET' : 'POST',
      headers: data === undefined ? {} : {
        'Content-Type': 'application/x-www-form-urlencoded'
      },
      body: data === undefined
        ? undefined
        : new URLSearchParams(data).toString(),
      signal: controller.signal
    });

    if (!response.ok) {
      throw Error(await response.text());
    }

    return await response.json();
  } finally {
    clearTimeout(timer);
  }
}

function controls() {
  const idle = last && [
    'KALIBRATIE NODIG',
    'KLAAR VOOR START'
  ].includes(last.state);
  const active = last && [
    'AFTELLEN',
    'RIJDEN MET PID',
    'LIJN ZOEKEN'
  ].includes(last.state);

  document.querySelectorAll('.command').forEach(button => {
    button.disabled = !idle || sending || command !== 0;
  });

  el('start').disabled =
    !idle || !last.calibrated || sending || command !== 0;
  form.querySelectorAll('input, button').forEach(control => {
    control.disabled = !last || active || sending;
  });
}

async function poll() {
  try {
    const s = await api('/api/status');
    last = s;

    el('connection').textContent = 'Verbonden met robot';

    el('status').textContent =
      s.state +
      ' | PWM L ' + s.left.toFixed(1) +
      ' R ' + s.right.toFixed(1) +
      (s.reason ? ' | ' + s.reason : '');
    el('timing').textContent =
      'Cyclustijd: ' + s.params.cycle_time_us + ' µs' +
      ' | max. rekentijd: ' + s.max_calculation_us + ' µs';

    el('sensorRows').innerHTML = s.sensors.map((v,i) =>
      '<tr><td>'+(i+1)+'</td><td>'+v[0]+'</td><td>'+v[1]+'</td><td>'+v[2]+'</td><td>'+v[3]+'</td><td>'+v[4]+'</td></tr>'
    ).join('');

    el('calibration').textContent =
      'Wit: ' + (s.white ? 'gemeten' : 'nog nodig') +
      ' | Zwart: ' + (s.black ? 'gemeten' : 'nog nodig') +
      ' | ' +
      (
        s.calibrated
        ? 'Kalibratie geldig'
        : s.state === 'WIT METEN' || s.state === 'ZWART METEN'
          ? 'Meten: ' + Math.round(
              s.samples /
              Math.ceil(2000000 / s.params.cycle_time_us) * 100
            ) + '%'
          : 'Nog geen geldige kalibratie'
      );

    if (!loaded) {
      Object.entries(s.params).forEach(([key, value]) => {
        form.elements.namedItem(key).value = value;
      });
      loaded = true;
    }

    if (revision && s.applied_revision >= revision) {
      el('message').textContent = 'Instellingen actief.';
      revision = 0;
    }

    if (command && s.command_applied >= command) {
      el('message').textContent = s.command_result;
      command = 0;
    }

    controls();
  } catch (e) {
    el('connection').textContent =
      'Verbinding tijdelijk weg. Opnieuw proberen...';
    last = null;
    controls();
  }

  const running =
    last &&
    (last.state === 'RIJDEN MET PID' || last.state === 'LIJN ZOEKEN');
  setTimeout(poll, running ? 1000 : 500);
}

document.querySelectorAll('.command').forEach(button => {
  button.onclick = async () => {
    sending = true;
    controls();

    try {
      const result = await api('/api/' + button.id, {});
      command = result.command;
      el('message').textContent =
        'Opdracht ontvangen; wachten op robot...';
    } catch (e) {
      el('message').textContent =
        'Opdracht niet bevestigd: ' + e.message;
    } finally {
      sending = false;
      controls();
    }
  };
});

el('stop').onclick = async () => {
  try {
    await api('/api/stop', {});
    el('message').textContent =
      'Stop aangevraagd; controleer de robotstatus.';
  } catch (e) {
    el('message').textContent =
      'Stop niet bevestigd. Schakel de motorvoeding uit.';
  }
};

form.onsubmit = async e => {
  e.preventDefault();
  el('apply').disabled = true;

  try {
    const data = Object.fromEntries(
      [...new FormData(form)].map(([key, value]) => [
        key, Number(value)
      ])
    );
    const result = await api('/api/params', data);
    revision = result.revision;
    el('message').textContent = 'Instellingen ontvangen...';
  } catch (e) {
    el('message').textContent = e.message;
  } finally {
    el('apply').disabled = false;
  }
};

poll();
</script>
</html>
)ROBOTHTML";

// ==================================================
// HTTP-SERVER
// ==================================================

static esp_err_t json_reply(httpd_req_t *req, const char *data) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, data, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, web_html, sizeof(web_html) - 1);
}

static esp_err_t status_get(httpd_req_t *req) {
    tuning_t p;
    char s[48], reason[80], cmd_message[96];
    float l, r;
    bool cal, line, white, black;
    int sensor_values[SENSOR_COUNT][4];
    int pos;
    unsigned samples;
    uint32_t rev, cmd_rev, max_calculation_us;

    portENTER_CRITICAL(&web_lock);
    p = tuning;
    memcpy(s, web_state, sizeof(s));
    memcpy(reason, web_reason, sizeof(reason));
    memcpy(cmd_message, command_result, sizeof(cmd_message));

    l = web_left;
    r = web_right;
    cal = web_calibrated;
    line = web_line;
    pos = web_position;
    rev = applied_revision;
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        sensor_values[i][0] = web_raw[i];
        sensor_values[i][1] = web_white_reference[i];
        sensor_values[i][2] = web_black_reference[i];
        sensor_values[i][3] = web_normalized[i];
    }
    white = web_white;
    black = web_black;
    samples = web_samples;
    cmd_rev = command_applied;
    max_calculation_us = web_max_calculation_us;
    portEXIT_CRITICAL(&web_lock);

    // Teksten zijn vaste firmwareteksten zonder JSON-aanhalingstekens.
    char sensor_json[512];
    size_t used = 0;
    sensor_json[used++] = '[';
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        int n = snprintf(sensor_json + used, sizeof(sensor_json) - used,
            "%s[%d,%d,%d,%d,%d]", i ? "," : "", (int)SENSOR_PINS[i],
            sensor_values[i][0], sensor_values[i][1],
            sensor_values[i][2], sensor_values[i][3]);
        if (n < 0 || (size_t)n >= sizeof(sensor_json) - used) return ESP_FAIL;
        used += (size_t)n;
    }
    sensor_json[used++] = ']';
    sensor_json[used] = 0;
    char reply[2048];

    snprintf(
        reply, sizeof(reply),
        "{\"white\":%s,\"black\":%s,\"samples\":%u,"
        "\"command_applied\":%lu,\"command_result\":\"%s\","
        "\"sensors\":%s,\"state\":\"%s\",\"reason\":\"%s\","
        "\"left\":%.2f,\"right\":%.2f,"
        "\"calibrated\":%s,\"line\":%s,\"position\":%d,"
        "\"applied_revision\":%lu,\"max_calculation_us\":%lu,"
        "\"params\":{"
        "\"kp\":%.3f,\"ki\":%.6f,\"kd\":%.6f,"
        "\"power\":%.3f,\"diff\":%.4f,\"cycle_time_us\":%lu,"
        "\"max_pwm\":%.3f,\"recovery_pwm\":%.3f,\"recovery_ms\":%d}}",
        white ? "true" : "false",
        black ? "true" : "false",
        samples,
        (unsigned long)cmd_rev,
        cmd_message,
        sensor_json,
        s, reason,
        (double)l, (double)r,
        cal ? "true" : "false",
        line ? "true" : "false",
        pos,
        (unsigned long)rev,
        (unsigned long)max_calculation_us,
        (double)p.kp,
        (double)p.ki,
        (double)p.kd,
        (double)p.power,
        (double)p.diff,
        (unsigned long)p.cycle_time_us,
        (double)p.max_pwm,
        (double)p.recovery_pwm,
        p.recovery_ms
    );

    return json_reply(req, reply);
}

static bool parse_params(char *body, tuning_t *p) {
    const char *keys[] = {
        "kp", "ki", "kd", "power", "diff",
        "cycle_time_us", "max_pwm", "recovery_pwm", "recovery_ms"
    };
    const float lower[] = {0, 0, 0, 0, 0, 1000, 0, 0, 0};
    const float upper[] = {INFINITY, 50, 50, 255, 1, 1000000, 255, 255, 500};

    float values[9] = {0};
    unsigned seen = 0;
    char *field = body;

    while (field && *field) {
        char *next = strchr(field, '&');
        if (next) *next++ = 0;

        char *eq = strchr(field, '=');
        if (!eq) return false;
        *eq++ = 0;

        int index = -1;
        for (int i = 0; i < 9; ++i) {
            if (strcmp(keys[i], field) == 0) index = i;
        }

        if (
            index < 0 ||
            (seen & (1U << index)) ||
            !*eq ||
            strspn(eq, "0123456789.-") != strlen(eq)
        ) {
            return false;
        }

        char *end;
        float v = strtof(eq, &end);

        if (
            *end || !isfinite(v) ||
            v < lower[index] || v > upper[index]
        ) {
            return false;
        }

        values[index] = v;
        seen |= 1U << index;

        if (next && !*next) return false;
        field = next;
    }

    if (
        seen != 511 ||
        floorf(values[5]) != values[5] ||
        fmodf(values[5], 100.0f) != 0.0f ||
        floorf(values[8]) != values[8] ||
        values[7] > values[6]
    ) {
        return false;
    }

    *p = (tuning_t){
        values[0], values[1], values[2], values[3], values[4],
        (uint32_t)values[5], values[6], values[7], (int)values[8]
    };
    return true;
}

static bool robot_active_for_web(void) {
    char current_state[sizeof(web_state)];

    portENTER_CRITICAL(&web_lock);
    memcpy(current_state, web_state, sizeof(current_state));
    portEXIT_CRITICAL(&web_lock);

    return strcmp(current_state, "AFTELLEN") == 0 ||
        strcmp(current_state, "RIJDEN MET PID") == 0 ||
        strcmp(current_state, "LIJN ZOEKEN") == 0;
}

static esp_err_t params_post(httpd_req_t *req) {
    char body[384];

    if (
        req->content_len == 0 ||
        req->content_len >= sizeof(body)
    ) {
        return httpd_resp_send_err(
            req, HTTPD_400_BAD_REQUEST, "Ongeldige grootte"
        );
    }

    size_t used = 0;
    while (used < req->content_len) {
        int n = httpd_req_recv(
            req, body + used, req->content_len - used
        );
        if (n <= 0) return ESP_FAIL;
        used += n;
    }
    body[used] = 0;

    tuning_t p;
    if (!parse_params(body, &p)) {
        return httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Ongeldige instellingen: controleer de bereiken, cyclustijd (1000-1000000 us, veelvoud van 100) en herstel-PWM <= max PWM."
        );
    }

    if (robot_active_for_web()) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_send(
            req,
            "Instellingen aanpassen kan alleen als de robot stilstaat.",
            HTTPD_RESP_USE_STRLEN
        );
    }

    if (!begin_config_write()) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            "Stop eerst de robot of wacht tot opslaan klaar is.");
    }
    esp_err_t save_error = tuning_save(&p);
    if (save_error != ESP_OK) {
        end_config_write();
        ESP_LOGE(
            TAG, "Rijinstellingen opslaan mislukt: %s",
            esp_err_to_name(save_error)
        );
        return httpd_resp_send_err(
            req, HTTPD_500_INTERNAL_SERVER_ERROR,
            "Instellingen konden niet worden opgeslagen"
        );
    }

    uint32_t rev;
    portENTER_CRITICAL(&web_lock);
    requested = p;
    tuning_pending = true;
    rev = ++requested_revision;
    config_writing = false;
    portEXIT_CRITICAL(&web_lock);

    char reply[64];
    snprintf(
        reply, sizeof(reply),
        "{\"revision\":%lu}", (unsigned long)rev
    );
    return json_reply(req, reply);
}

static esp_err_t stop_post(httpd_req_t *req) {
    portENTER_CRITICAL(&web_lock);
    stop_requested = true;
    portEXIT_CRITICAL(&web_lock);

    httpd_resp_set_hdr(req, "Connection", "close");
    return json_reply(req, "{\"ok\":true}");
}

static esp_err_t command_post(httpd_req_t *req) {
    if (req->content_len != 0) {
        return httpd_resp_send_err(
            req, HTTPD_400_BAD_REQUEST, "Geen body verwacht"
        );
    }

    if (robot_active_for_web()) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_send(
            req,
            "Bediening is alleen beschikbaar als de robot stilstaat.",
            HTTPD_RESP_USE_STRLEN
        );
    }

    PhoneCommand cmd = PHONE_NONE;
    if (!strcmp(req->uri, "/api/white")) cmd = PHONE_WHITE;
    if (!strcmp(req->uri, "/api/black")) cmd = PHONE_BLACK;
    if (!strcmp(req->uri, "/api/clear")) cmd = PHONE_CLEAR;
    if (!strcmp(req->uri, "/api/start")) cmd = PHONE_START;

    bool busy;
    uint32_t id = 0;

    portENTER_CRITICAL(&web_lock);
    busy = pending_command != PHONE_NONE;
    if (!busy) {
        pending_command = cmd;
        id = ++command_revision;
    }
    portEXIT_CRITICAL(&web_lock);

    if (busy) {
        httpd_resp_set_status(req, "409 Conflict");
        return json_reply(
            req, "{\"error\":\"Vorige opdracht wordt nog verwerkt\"}"
        );
    }

    char reply[64];
    snprintf(
        reply, sizeof(reply),
        "{\"command\":%lu}", (unsigned long)id
    );
    return json_reply(req, reply);
}

static esp_err_t web_init(void) {
    // Arduino initialiseert NVS en de netwerkstack al.
    ESP_RETURN_ON_ERROR(tuning_load(), TAG, "saved tuning load");
    if (!WiFi.mode(WIFI_AP) || !WiFi.softAP("Robot-S3", "lijnrobot32")) {
        Serial.println("Wifi access point starten mislukt");
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "WiFi power save");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144;
    config.max_open_sockets = 5;
    config.max_uri_handlers = 10;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 2;
    config.send_wait_timeout = 2;

    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(
        httpd_start(&server, &config), TAG, "HTTP start"
    );

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get, .user_ctx = NULL},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get, .user_ctx = NULL},
        {.uri = "/api/params", .method = HTTP_POST, .handler = params_post, .user_ctx = NULL},
        {.uri = "/api/stop", .method = HTTP_POST, .handler = stop_post, .user_ctx = NULL},
        {.uri = "/api/white", .method = HTTP_POST, .handler = command_post, .user_ctx = NULL},
        {.uri = "/api/black", .method = HTTP_POST, .handler = command_post, .user_ctx = NULL},
        {.uri = "/api/clear", .method = HTTP_POST, .handler = command_post, .user_ctx = NULL},
        {.uri = "/api/start", .method = HTTP_POST, .handler = command_post, .user_ctx = NULL}
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        ESP_RETURN_ON_ERROR(
            httpd_register_uri_handler(server, &routes[i]),
            TAG, "HTTP route"
        );
    }

    ESP_LOGI(
        TAG,
        "Wifi: Robot-S3 | wachtwoord: lijnrobot32 | http://192.168.4.1"
    );
    return ESP_OK;
}

// ==================================================
// ROBOTVARIABELEN
// ==================================================

static adc_oneshot_unit_handle_t adc_handle;
static adc_channel_t adc_channels[SENSOR_COUNT];

static robot_state_t state = STATE_IDLE;
static TickType_t state_started;

static int white_reference[SENSOR_COUNT];
static int black_reference[SENSOR_COUNT];
static int32_t reference_sum[SENSOR_COUNT];
static unsigned reference_count;
static unsigned reference_target;

static bool white_available;
static bool black_available;
static bool calibrated;

static float left_pwm;
static float right_pwm;


static float previous_error;
static float integral_term;
static float ki_per_cycle;
static float kd_per_cycle;
static float derivative_alpha;
static float filtered_derivative;

static void update_pid_coefficients(void) {
    const float dt = (float)tuning.cycle_time_us * 0.000001f;
    ki_per_cycle = tuning.ki * dt;
    kd_per_cycle = tuning.kd / dt;
    derivative_alpha = (float)tuning.cycle_time_us /
        (DERIVATIVE_FILTER_US + (float)tuning.cycle_time_us);
}
static bool derivative_initialized;

// -1 = links, +1 = rechts, 0 = nog geen duidelijke bocht gezien.
static int last_turn_direction;
static TickType_t last_line_seen;
static bool line_recovering;

static void calibration_validate(void);

static esp_err_t calibration_load_one(
    nvs_handle_t handle,
    const char *key,
    int *values,
    bool *available
) {
    size_t size = 0;
    esp_err_t error = nvs_get_blob(handle, key, NULL, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        *available = false;
        return ESP_OK;
    }
    if (error != ESP_OK) return error;

    if (size != sizeof(int) * SENSOR_COUNT) {
        ESP_LOGW(TAG, "Opgeslagen %s-kalibratie heeft een ongeldige lengte", key);
        *available = false;
        return ESP_OK;
    }

    error = nvs_get_blob(handle, key, values, &size);
    if (error != ESP_OK) return error;

    for (int i = 0; i < SENSOR_COUNT; ++i) {
        if (values[i] < 0 || values[i] > 4095) {
            ESP_LOGW(TAG, "Opgeslagen %s-kalibratie Q%d buiten ADC-bereik", key, i + 1);
            *available = false;
            return ESP_OK;
        }
    }

    *available = true;
    return ESP_OK;
}

static esp_err_t calibration_load(void) {
    nvs_handle_t handle;
    esp_err_t error = nvs_open("robot_qtr8a", NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;

    error = calibration_load_one(
        handle, "white_ref", white_reference, &white_available
    );
    if (error == ESP_OK) {
        error = calibration_load_one(
            handle, "black_ref", black_reference, &black_available
        );
    }
    nvs_close(handle);

    if (error == ESP_OK) {
        calibration_validate();
        ESP_LOGI(
            TAG, "Kalibratie geladen: wit=%s zwart=%s",
            white_available ? "ja" : "nee",
            black_available ? "ja" : "nee"
        );
    }
    return error;
}

static esp_err_t calibration_save(const char *key, const int *values) {
    nvs_handle_t handle;
    esp_err_t error = nvs_open("robot_qtr8a", NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_set_blob(
        handle, key, values, sizeof(int) * SENSOR_COUNT
    );
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

static esp_err_t calibration_erase(void) {
    nvs_handle_t handle;
    esp_err_t error = nvs_open("robot_qtr8a", NVS_READWRITE, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;

    esp_err_t white_error = nvs_erase_key(handle, "white_ref");
    esp_err_t black_error = nvs_erase_key(handle, "black_ref");
    if (white_error == ESP_ERR_NVS_NOT_FOUND) white_error = ESP_OK;
    if (black_error == ESP_ERR_NVS_NOT_FOUND) black_error = ESP_OK;

    error = white_error != ESP_OK ? white_error : black_error;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

// ==================================================
// MOTOREN
// ==================================================

static void pwm_set(ledc_channel_t channel, float power) {
    if (power < 0.0f) power = 0.0f;
    if (power > 255.0f) power = 255.0f;

    uint32_t duty = (uint32_t)(
        PWM_MAX_DUTY * power / 255.0f + 0.5f
    );

    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, channel));
}

static void motor_side_set(
    float power,
    ledc_channel_t forward_channel,
    ledc_channel_t reverse_channel
) {
    float magnitude = power < 0.0f ? -power : power;
    if (power < 0.0f) {
        pwm_set(forward_channel, 0.0f);
        pwm_set(reverse_channel, magnitude);
    } else {
        pwm_set(reverse_channel, 0.0f);
        pwm_set(forward_channel, magnitude);
    }
}

static void motors_set(float left, float right) {
    if (left > MAX_PWM_VALUE) left = MAX_PWM_VALUE;
    if (left < -MAX_PWM_VALUE) left = -MAX_PWM_VALUE;
    if (right > MAX_PWM_VALUE) right = MAX_PWM_VALUE;
    if (right < -MAX_PWM_VALUE) right = -MAX_PWM_VALUE;

    if (left != left_pwm) {
        motor_side_set(LEFT_INVERT ? -left : left, LEFT_CHANNEL, LEFT_REVERSE_CHANNEL);
    }
    if (right != right_pwm) {
        motor_side_set(RIGHT_INVERT ? -right : right, RIGHT_CHANNEL, RIGHT_REVERSE_CHANNEL);
    }

    left_pwm = left;
    right_pwm = right;
}

static float constrain_motor_power(float power) {
    if (power > tuning.max_pwm) return tuning.max_pwm;
    if (power < -tuning.max_pwm) return -tuning.max_pwm;
    return power;
}

static void motors_mix(float base, float correction) {
    if (!isfinite(correction)) correction = 0.0f;
    const float limit = 2.0f * tuning.max_pwm;
    if (correction > limit) correction = limit;
    if (correction < -limit) correction = -limit;
    if (base > tuning.max_pwm) base = tuning.max_pwm;
    if (base < 0.0f) base = 0.0f;

    float left;
    float right;
    if (correction >= 0.0f) {
        left = constrain_motor_power(
            base + tuning.diff * correction
        );
        right = constrain_motor_power(left - correction);
        left = right + correction;
    } else {
        right = constrain_motor_power(
            base - tuning.diff * correction
        );
        left = constrain_motor_power(right + correction);
        right = left - correction;
    }

    motors_set(left, right);
}

static void motors_init(void) {
    const gpio_config_t config = {
        .pin_bit_mask =
            (1ULL << LEFT_IN1) |
            (1ULL << LEFT_IN2) |
            (1ULL << RIGHT_IN1) |
            (1ULL << RIGHT_IN2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&config));

    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN1, 0);
    gpio_set_level(RIGHT_IN2, 0);

    const ledc_timer_config_t timer_config = {
        .speed_mode = PWM_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    const ledc_channel_config_t left_config = {
        .gpio_num = LEFT_IN1,
        .speed_mode = PWM_MODE,
        .channel = LEFT_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {.output_invert = 0},
    };

    const ledc_channel_config_t right_config = {
        .gpio_num = RIGHT_IN1,
        .speed_mode = PWM_MODE,
        .channel = RIGHT_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {.output_invert = 0},
    };

    const ledc_channel_config_t left_reverse_config = {
        .gpio_num = LEFT_IN2,
        .speed_mode = PWM_MODE,
        .channel = LEFT_REVERSE_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {.output_invert = 0},
    };

    const ledc_channel_config_t right_reverse_config = {
        .gpio_num = RIGHT_IN2,
        .speed_mode = PWM_MODE,
        .channel = RIGHT_REVERSE_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {.output_invert = 0},
    };

    ESP_ERROR_CHECK(ledc_channel_config(&left_config));
    ESP_ERROR_CHECK(ledc_channel_config(&right_config));
    ESP_ERROR_CHECK(ledc_channel_config(&left_reverse_config));
    ESP_ERROR_CHECK(ledc_channel_config(&right_reverse_config));

    left_pwm = 0.0f;
    right_pwm = 0.0f;
    ESP_LOGI(TAG, "Motoren ingesteld");
}

// ==================================================
// QTR-8A: ACHT RECHTSTREEKSE ADC1-INGANGEN, GEEN MULTIPLEXER
// ==================================================

static void sensors_init(void) {
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle));
    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        adc_unit_t unit;
        ESP_ERROR_CHECK(adc_oneshot_io_to_channel(SENSOR_PINS[i], &unit, &adc_channels[i]));
        ESP_ERROR_CHECK(unit == ADC_UNIT_1 ? ESP_OK : ESP_ERR_INVALID_ARG);
        ESP_ERROR_CHECK(gpio_reset_pin(SENSOR_PINS[i]));
        ESP_ERROR_CHECK(gpio_set_direction(SENSOR_PINS[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(SENSOR_PINS[i], GPIO_FLOATING));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channels[i], &channel_config));
    }
    ESP_LOGI(TAG, "QTR-8A: 8 analoge ADC1-ingangen klaar");
}

static esp_err_t read_channels(int *raw) {
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        int ignored;
        // Eerste conversie na kanaalwissel weggooien; vier metingen middelen.
        esp_err_t error = adc_oneshot_read(adc_handle, adc_channels[i], &ignored);
        if (error != ESP_OK) return error;
        int sum = 0;
        for (int sample = 0; sample < 4; ++sample) {
            int value;
            error = adc_oneshot_read(adc_handle, adc_channels[i], &value);
            if (error != ESP_OK) return error;
            sum += value;
        }
        raw[i] = (sum + 2) / 4;
    }
    return ESP_OK;
}

// ==================================================
// STOP EN KALIBRATIE
// ==================================================

static void pd_reset(void) {
    previous_error = 0.0f;
    integral_term = 0.0f;
    derivative_initialized = false;
    filtered_derivative = 0.0f;
}

static void robot_stop(const char *reason) {
    motors_set(0.0f, 0.0f);
    state = STATE_IDLE;
    portENTER_CRITICAL(&web_lock);
    control_active = false;
    portEXIT_CRITICAL(&web_lock);
    line_recovering = false;
    last_turn_direction = 0;

    pd_reset();

    portENTER_CRITICAL(&web_lock);
    snprintf(web_reason, sizeof(web_reason), "%s", reason);
    portEXIT_CRITICAL(&web_lock);

    ESP_LOGW(TAG, "STOP: %s", reason);
}

static void calibration_clear(void) {
    motors_set(0.0f, 0.0f);

    esp_err_t error = calibration_erase();
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG, "Kalibratie uit NVS wissen mislukt: %s",
            esp_err_to_name(error)
        );
    }

    calibrated = false;
    white_available = false;
    black_available = false;

    state = STATE_IDLE;
    portENTER_CRITICAL(&web_lock);
    control_active = false;
    portEXIT_CRITICAL(&web_lock);
    line_recovering = false;

    pd_reset();

    ESP_LOGI(TAG, "Kalibratie gewist: meet wit en zwart via de app");
}

static void capture_begin(robot_state_t mode) {
    motors_set(0.0f, 0.0f);

    state = mode;
    calibrated = false;
    reference_count = 0;
    reference_target =
        (REFERENCE_CAPTURE_US + tuning.cycle_time_us - 1) /
        tuning.cycle_time_us;

    for (int i = 0; i < SENSOR_COUNT; i++) {
        reference_sum[i] = 0;
    }

    if (mode == STATE_WHITE) {
        white_available = false;
    } else {
        black_available = false;
    }

    ESP_LOGI(
        TAG,
        "%smeting gestart: circa 2 seconden stilhouden",
        mode == STATE_WHITE ? "Wit" : "Zwart"
    );
}

// Kalibratiefix: controle met absolute verschilwaarde.
// Zwart mag hoger OF lager meten dan wit.
static void calibration_validate(void) {
    calibrated = false;
    char message[80];

    if (!white_available || !black_available) {
        snprintf(
            message, sizeof(message),
            "Nog nodig: %s",
            !white_available ? "wit meten" : "zwart meten"
        );
    } else {
        bool valid = true;
        int failed = -1;

        for (int i = 0; i < SENSOR_COUNT; ++i) {
            int span = black_reference[i] - white_reference[i];

            ESP_LOGI(
                TAG,
                "Q%d wit=%d zwart=%d verschil=%d",
                i + 1,
                white_reference[i],
                black_reference[i],
                span
            );

            if (
                white_reference[i] < 0 ||
                white_reference[i] > 4095 ||
                black_reference[i] < 0 ||
                black_reference[i] > 4095
            ) {
                valid = false;
                if (failed < 0) failed = i;
                ESP_LOGW(
                    TAG,
                    "Q%d: ongeldige ADC-waarde buiten 0..4095",
                    i + 1
                );
            } else if (abs(span) < MIN_CALIBRATION_SPAN) {
                valid = false;
                if (failed < 0) failed = i;
            }
        }

        calibrated = valid;

        if (valid) {
            snprintf(
                message, sizeof(message), "Kalibratie geldig"
            );
        } else {
            snprintf(
                message, sizeof(message),
                "Q%d: wit=%d zwart=%d (min verschil=%d)",
                failed + 1,
                white_reference[failed],
                black_reference[failed],
                MIN_CALIBRATION_SPAN
            );
        }
    }

    portENTER_CRITICAL(&web_lock);
    snprintf(web_reason, sizeof(web_reason), "%s", message);
    portEXIT_CRITICAL(&web_lock);

    ESP_LOGI(TAG, "%s", message);
}

static void capture_update(const int *raw) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        reference_sum[i] += raw[i];
    }

    reference_count++;
    if (reference_count < reference_target) return;

    bool measuring_white = state == STATE_WHITE;
    int *reference = measuring_white
        ? white_reference
        : black_reference;

    for (int i = 0; i < SENSOR_COUNT; i++) {
        reference[i] =
            (reference_sum[i] + reference_target / 2)
            / reference_target;
    }

    if (measuring_white) {
        white_available = true;
    } else {
        black_available = true;
    }

    const char *key = measuring_white ? "white_ref" : "black_ref";
    esp_err_t save_error = calibration_save(key, reference);
    if (save_error != ESP_OK) {
        ESP_LOGE(
            TAG, "%s-kalibratie opslaan mislukt: %s",
            measuring_white ? "Wit" : "Zwart",
            esp_err_to_name(save_error)
        );
    }

    ESP_LOGI(
        TAG, "%smeting klaar",
        measuring_white ? "Wit" : "Zwart"
    );

    state = STATE_IDLE;
    calibration_validate();
}

// ==================================================
// LIJNDETECTIE
// ==================================================

static int normalize(int index, int raw) {
    int span = black_reference[index] - white_reference[index];

    // Alleen de controle gebruikt abs().
    // Het getekende verschil blijft in de deling behouden.
    if (abs(span) < MIN_CALIBRATION_SPAN) return 0;

    int value =
        (raw - white_reference[index]) * 1000 / span;

    if (value < 0) value = 0;
    if (value > 1000) value = 1000;
    return value;
}

static bool calculate_line(
    const int *normalized,
    int *position,
    int *peak,
    int *strength
) {
    int peak_index = 0;
    *peak = 0;
    *strength = 0;

    for (int i = 0; i < SENSOR_COUNT; i++) {
        int value = normalized[i];
        if (value > *peak) {
            *peak = value;
            peak_index = i;
        }
        if (value < LINE_NOISE_THRESHOLD) continue;

        *strength += value;
    }

    if (*peak < LINE_MIN_PEAK || *strength < LINE_MIN_SUM) {
        return false;
    }

    int center_index = peak_index;
    if (center_index == 0) {
        center_index = 1;
    } else if (center_index == SENSOR_COUNT - 1) {
        center_index = SENSOR_COUNT - 2;
    }

    int left_index = center_index - 1;
    int right_index = center_index + 1;
    float x_left = (float)SENSOR_POSITION[left_index];
    float x_center = (float)SENSOR_POSITION[center_index];
    float x_right = (float)SENSOR_POSITION[right_index];
    float y_left = (float)normalized[left_index];
    float y_center = (float)normalized[center_index];
    float y_right = (float)normalized[right_index];

    float left_slope = (y_center - y_left) / (x_center - x_left);
    float right_slope = (y_right - y_center) / (x_right - x_center);
    float a = (right_slope - left_slope) / (x_right - x_left);
    float interpolated_position = x_center;

    if (a < 0.0f) {
        float b = left_slope + a * (x_center - x_left);
        float offset = -b / (2.0f * a);
        float min_offset = x_left - x_center;
        float max_offset = x_right - x_center;

        if (offset < min_offset) offset = min_offset;
        if (offset > max_offset) offset = max_offset;
        interpolated_position += offset;
    }

    *position = (int)lroundf(interpolated_position);
    if (SENSOR_ORDER_REVERSED) *position = -*position;
    return true;
}

// Referenties zijn gemiddelden, geen harde grenzen voor afzonderlijke metingen.
// normalize() begrenst normale overschrijdingen tot 0..1000.

static float controller_limit(void) { return 2.0f * tuning.max_pwm; }

static float constrain_controller_output(float output) {
    if (!isfinite(output)) return 0.0f;
    const float limit = controller_limit();
    if (output > limit) return limit;
    if (output < -limit) return -limit;
    return output;
}

static void follow_line(int position) {
    // Een gecentreerde meting wist de laatst bekende bocht niet.
    if (position < -RECOVERY_MIN_POSITION) last_turn_direction = -1;
    else if (position > RECOVERY_MIN_POSITION) last_turn_direction = 1;
    // Zelfde stuurrichting als de oorspronkelijke robot.
    // position = lijn t.o.v. robot; input = robot t.o.v. lijn.
    const float input = -(float)position * 0.001f;
    const float error = 0.0f - input; // setpoint - input
    float error_delta = 0.0f;

    if (derivative_initialized) {
        error_delta = error - previous_error;
    } else {
        derivative_initialized = true;
    }
    previous_error = error;

    integral_term += ki_per_cycle * error;
    integral_term = constrain_controller_output(integral_term);

    float output = error * KP;
    output += integral_term;
    const float derivative = kd_per_cycle * error_delta;
    filtered_derivative += derivative_alpha * (derivative - filtered_derivative);
    output += filtered_derivative;
    output = constrain_controller_output(output);

    motors_mix(tuning.power, output);
}

static void recover_line(int direction) {
    if (direction < 0) {
        motors_set(0.0f, RECOVERY_PWM_VALUE);
    } else if (direction > 0) {
        motors_set(RECOVERY_PWM_VALUE, 0.0f);
    } else {
        motors_set(0.0f, 0.0f);
    }
}

// ==================================================
// STATUS EN DEBUG
// ==================================================

static const char *state_text(void) {
    switch (state) {
        case STATE_IDLE:
            return calibrated
                ? "KLAAR VOOR START"
                : "KALIBRATIE NODIG";
        case STATE_WHITE:
            return "WIT METEN";
        case STATE_BLACK:
            return "ZWART METEN";
        case STATE_COUNTDOWN:
            return "AFTELLEN";
        case STATE_RUNNING:
            return line_recovering
                ? "LIJN ZOEKEN"
                : "RIJDEN MET PID";
    }
    return "ONBEKEND";
}

// ==================================================
// SERIELE COMMANDO'S (SerialCommand-achtige CLI)
// ==================================================

static bool serial_parse_float(const char *text, float *value) {
    if (!text || !*text) return false;
    char *end;
    float parsed = strtof(text, &end);
    if (*end || !isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

static void serial_print_help(void) {
    Serial.printf(
        "Commando's: set <debug|cycle|kp|ki|kd|power|diff|max_pwm|"
        "recovery_pwm|recovery_ms> <waarde>, debug, start, stop, "
        "white, black, clear, help\r\n"
    );
}

static void serial_print_debug(void) {
    tuning_t p;
    uint32_t calculation_us;
    portENTER_CRITICAL(&web_lock);
    p = tuning;
    calculation_us = web_max_calculation_us;
    timing_reset_requested = true;
    const uint32_t misses = deadline_misses;
    const uint32_t crossings = junction_count;
    const int64_t interval = junction_interval_us;
    portEXIT_CRITICAL(&web_lock);

    Serial.printf("cycle time: %lu us\r\n", (unsigned long)p.cycle_time_us);
    Serial.printf("Cyclusadvies (2x rekentijd, afgerond): %lu us\r\n",
        (unsigned long)(((2U * calculation_us + 99U) / 100U < 10U)
            ? 1000U : ((2U * calculation_us + 99U) / 100U) * 100U));
    Serial.printf("Timingoverschrijdingen: %lu; kruispunten: %lu; interval: %lld ms\r\n",
        (unsigned long)misses, (unsigned long)crossings, (long long)(interval / 1000));
    Serial.printf("Kp: %.4f\r\n", (double)p.kp);
    Serial.printf("Ki: %.6f\r\n", (double)p.ki);
    Serial.printf("Kd: %.6f\r\n", (double)p.kd);
    Serial.printf("power: %.2f\r\n", (double)p.power);
    Serial.printf("diff: %.3f\r\n", (double)p.diff);
    Serial.printf("max PWM: %.2f / 255\r\n", (double)p.max_pwm);
    Serial.printf("recovery PWM: %.2f / 255\r\n", (double)p.recovery_pwm);
    Serial.printf("recovery time: %d ms\r\n", p.recovery_ms);
    Serial.printf("calculation time: %lu us\r\n",
        (unsigned long)calculation_us);
}

static bool serial_set_parameter(const char *name, const char *value) {
    if (strcmp(name, "debug") == 0) {
        if (strcmp(value, "on") != 0 && strcmp(value, "off") != 0) {
            Serial.printf("Gebruik: set debug on|off\r\n");
            return false;
        }
        portENTER_CRITICAL(&web_lock);
        debug_enabled = strcmp(value, "on") == 0;
        portEXIT_CRITICAL(&web_lock);
        Serial.printf("debug %s\r\n", value);
        return true;
    }

    tuning_t p;
    portENTER_CRITICAL(&web_lock);
    p = requested;
    portEXIT_CRITICAL(&web_lock);

    if (strcmp(name, "cycle") == 0) {
        char *end;
        unsigned long cycle;
        if (strcmp(value, "auto") == 0) {
            portENTER_CRITICAL(&web_lock);
            const uint32_t measured = web_max_calculation_us;
            const uint32_t samples = timing_samples;
            portEXIT_CRITICAL(&web_lock);
            if (samples < 100 || measured == 0) {
                Serial.printf("Wacht minstens 100 meetcycli en probeer opnieuw\r\n");
                return false;
            }
            cycle = ((2UL * measured + 99UL) / 100UL) * 100UL;
            if (cycle < 1000) cycle = 1000;
            end = (char *)"";
            Serial.printf("Gemeten cyclusadvies: %lu us; controleer ook rijdend\r\n", cycle);
        } else {
            cycle = strtoul(value, &end, 10);
        }
        if (!*value || *end || cycle > UINT32_MAX) {
            Serial.printf("Ongeldige cyclustijd\r\n");
            return false;
        }
        p.cycle_time_us = (uint32_t)cycle;
    } else {
        float parsed;
        if (!serial_parse_float(value, &parsed)) {
            Serial.printf("Ongeldige numerieke waarde\r\n");
            return false;
        }
        if (strcmp(name, "kp") == 0) p.kp = parsed;
        else if (strcmp(name, "ki") == 0) p.ki = parsed;
        else if (strcmp(name, "kd") == 0) p.kd = parsed;
        else if (strcmp(name, "power") == 0) p.power = parsed;
        else if (strcmp(name, "diff") == 0) p.diff = parsed;
        else if (strcmp(name, "max_pwm") == 0) p.max_pwm = parsed;
        else if (strcmp(name, "recovery_pwm") == 0) p.recovery_pwm = parsed;
        else if (strcmp(name, "recovery_ms") == 0) {
            if (floorf(parsed) != parsed || parsed < 0.0f || parsed > 500.0f) {
                Serial.printf("recovery_ms moet een geheel getal tussen 0 en 500 zijn\r\n");
                return false;
            }
            p.recovery_ms = (int)parsed;
        } else {
            Serial.printf("Onbekende parameter: %s\r\n", name);
            return false;
        }
    }

    if (!tuning_valid(&p)) {
        Serial.printf("Waarde buiten bereik of ongeldige combinatie\r\n");
        return false;
    }

    if (!begin_config_write()) {
        Serial.printf("Stop eerst de robot of wacht tot opslaan klaar is\r\n");
        return false;
    }
    esp_err_t error = tuning_save(&p);
    if (error != ESP_OK) {
        end_config_write();
        Serial.printf("Instellingen opslaan mislukt: %s\r\n",
            esp_err_to_name(error));
        return false;
    }

    portENTER_CRITICAL(&web_lock);
    requested = p;
    tuning_pending = true;
    ++requested_revision;
    config_writing = false;
    portEXIT_CRITICAL(&web_lock);

    Serial.printf("Instelling opgeslagen en wordt toegepast\r\n");
    return true;
}

static void serial_queue_command(PhoneCommand command) {
    portENTER_CRITICAL(&web_lock);
    bool busy = pending_command != PHONE_NONE;
    if (!busy) {
        pending_command = command;
        ++command_revision;
    }
    portEXIT_CRITICAL(&web_lock);

    Serial.printf(busy
        ? "Bezig: vorige opdracht wordt nog verwerkt\r\n"
        : "Opdracht wordt verwerkt\r\n");
}

static void serial_process_line(char *line) {
    char *save = NULL;
    char *command = strtok_r(line, " \t", &save);
    if (!command) return;

    if (strcmp(command, "set") == 0) {
        char *name = strtok_r(NULL, " \t", &save);
        char *value = strtok_r(NULL, " \t", &save);
        char *extra = strtok_r(NULL, " \t", &save);
        if (!name || !value || extra) {
            Serial.printf("Gebruik: set <parameter> <waarde>\r\n");
            return;
        }
        serial_set_parameter(name, value);
    } else if (strcmp(command, "debug") == 0) {
        if (strtok_r(NULL, " \t", &save)) {
            Serial.printf("Gebruik: debug\r\n");
            return;
        }
        serial_print_debug();
    } else if (strcmp(command, "start") == 0) {
        if (strtok_r(NULL, " \t", &save)) {
            Serial.printf("Gebruik: start\r\n");
            return;
        }
        serial_queue_command(PHONE_START);
    } else if (strcmp(command, "white") == 0) {
        if (strtok_r(NULL, " \t", &save)) {
            Serial.printf("Gebruik: white\r\n");
            return;
        }
        serial_queue_command(PHONE_WHITE);
    } else if (strcmp(command, "black") == 0) {
        if (strtok_r(NULL, " \t", &save)) {
            Serial.printf("Gebruik: black\r\n");
            return;
        }
        serial_queue_command(PHONE_BLACK);
    } else if (strcmp(command, "clear") == 0) {
        if (strtok_r(NULL, " \t", &save)) {
            Serial.printf("Gebruik: clear\r\n");
            return;
        }
        serial_queue_command(PHONE_CLEAR);
    } else if (strcmp(command, "stop") == 0) {
        if (strtok_r(NULL, " \t", &save)) {
            Serial.printf("Gebruik: stop\r\n");
            return;
        }
        portENTER_CRITICAL(&web_lock);
        stop_requested = true;
        portEXIT_CRITICAL(&web_lock);
        Serial.printf("STOP aangevraagd\r\n");
    } else if (strcmp(command, "help") == 0) {
        serial_print_help();
    } else {
        Serial.printf("unknown command: \"%s\"\r\n", command);
    }
}

static void serial_command_task(void *unused) {
    char line[128];
    size_t used = 0;
    bool overflow = false;
    Serial.println("ready");
    serial_print_help();
    while (true) {
        // Begrens het werk, ook bij een continue stroom binnenkomende bytes.
        for (int budget = 0; budget < 64 && Serial.available(); ++budget) {
            const int c = Serial.read();
            if (c < 0) break;
            if (c == '\r' || c == '\n') {
                if (overflow) {
                    Serial.println("Commando te lang; opnieuw invoeren");
                } else if (used != 0) {
                    line[used] = '\0';
                    serial_process_line(line);
                }
                used = 0;
                overflow = false;
            } else if (!overflow) {
                if (used < sizeof(line) - 1) line[used++] = (char)c;
                else overflow = true;
            }
        }
        vTaskDelay(1);
    }
}

static void serial_debug_task(void *unused) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        bool enabled;
        portENTER_CRITICAL(&web_lock);
        enabled = debug_enabled;
        portEXIT_CRITICAL(&web_lock);
        if (enabled) {
            Serial.printf("loop still running\r\n");
                }
    }
}

// ==================================================
// ROBOTTAAK
// ==================================================

static void robot_task(void *unused) {
    sensors_init();
    pd_reset();

    line_recovering = false;

    esp_err_t web_error = web_init();
    if (web_error != ESP_OK) {
        robot_stop("wifi/web opstartfout");
        ESP_LOGE(
            TAG, "Web init: %s",
            esp_err_to_name(web_error)
        );
        vTaskDelete(NULL);
        return;
    }
    esp_err_t calibration_error = calibration_load();
    if (calibration_error != ESP_OK) {
        white_available = false;
        black_available = false;
        calibrated = false;
        ESP_LOGE(
            TAG, "Kalibratie uit NVS laden mislukt: %s",
            esp_err_to_name(calibration_error)
        );
    }
    update_pid_coefficients();

    if (xTaskCreate(serial_command_task, "serial_cmd", 4096, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Seriele commandotaak niet gestart");
    }
    if (xTaskCreate(serial_debug_task, "serial_debug", 2048, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Seriele debugtaak niet gestart");
    }

    robot_task_handle = xTaskGetCurrentTaskHandle();
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = cycle_wakeup;
    timer_args.arg = robot_task_handle;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "regelcyclus";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &cycle_timer));

    ESP_LOGI(TAG, "Robot gestart - PID-regeling");
    ESP_LOGI(
        TAG,
    "Power:%.1f diff:%.2f max PWM:%.1f/255 KP:%.2f KI:%.4f KD:%.4f cycle:%lu us",
    (double)tuning.power,
    (double)tuning.diff,
    (double)MAX_PWM_VALUE,
    (double)KP,
    (double)KI,
    (double)KD,
    (unsigned long)tuning.cycle_time_us
    );
    ESP_LOGI(
        TAG, "Lijnherstel: %d ms op %.1f/255 PWM",
        LINE_RECOVERY_MS,
        (double)RECOVERY_PWM_VALUE
    );
    ESP_LOGI(TAG, "Wit, zwart, start en stop via wifi of serieel");

    bool adc_fault = false;
    TickType_t last_web_update = xTaskGetTickCount();
    uint32_t max_calculation_us = 0;
    bool junction_active = false;
    int64_t last_junction_us = 0;

    uint32_t timer_period_us = tuning.cycle_time_us;
    ESP_ERROR_CHECK(esp_timer_start_periodic(cycle_timer, timer_period_us));
    while (1) {
        // Wachten blokkeert alleen deze taak; andere taken blijven werken.
        const uint32_t ticks = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (ticks > 1) {
            portENTER_CRITICAL(&web_lock);
            deadline_misses += ticks - 1;
            portEXIT_CRITICAL(&web_lock);
            if (state == STATE_RUNNING) robot_stop("Cycli gemist: verhoog cyclustijd");
        }
        portENTER_CRITICAL(&web_lock);
        if (timing_reset_requested) {
            max_calculation_us = 0;
            web_max_calculation_us = 0;
            timing_samples = 0;
            deadline_misses = 0;
            timing_reset_requested = false;
        }
        portEXIT_CRITICAL(&web_lock);
        const int64_t calculation_start_us = esp_timer_get_time();
        int raw[CHANNEL_COUNT];
        int normalized[SENSOR_COUNT] = {0};

        if (!adc_fault) {
            esp_err_t error = read_channels(raw);

            if (error != ESP_OK) {
                adc_fault = true;
                robot_stop("ADC-leesfout: herstart nodig");
                calibrated = false;
                ESP_LOGE(
                    TAG, "ADC fout: %s",
                    esp_err_to_name(error)
                );
            }
        }

        // Bedienings- en statustaak blijft bij ADC-fout draaien.
        // Motoren blijven geblokkeerd tot herstart.
        if (adc_fault) {
            for (int i = 0; i < CHANNEL_COUNT; ++i) {
                raw[i] = 4095;
            }
        }

        TickType_t now = xTaskGetTickCount();
        int position = 0;
        int peak = 0;
        int strength = 0;
        bool line_found = false;
        if (calibrated && !adc_fault) {
            for (int i = 0; i < SENSOR_COUNT; i++) normalized[i] = normalize(i, raw[i]);
            line_found = calculate_line(normalized, &position, &peak, &strength);
        }
        bool web_stop, changed;
        PhoneCommand phone_command;
        uint32_t phone_id;
        portENTER_CRITICAL(&web_lock);

        web_stop = stop_requested;
        stop_requested = false;

        phone_command = pending_command;
        phone_id = command_revision;
        pending_command = PHONE_NONE;


        changed = tuning_pending;
        if (changed) {
            tuning = requested;
            update_pid_coefficients();
            tuning_pending = false;
            applied_revision = requested_revision;
        }

        portEXIT_CRITICAL(&web_lock);

        if (changed) {
            motors_set(left_pwm, right_pwm);
        }

        if (web_stop) robot_stop("stop via gsm");

        // Telefoonopdrachten worden door de robottaak uitgevoerd.
        if (phone_command != PHONE_NONE) {
            const char *result;

            if (web_stop) {
                result = "Opdracht geannuleerd door STOP";

            } else if (adc_fault) {
                result = "ADC-fout: herstart de robot";
            } else if (state != STATE_IDLE) {
                result = "Robot bezig: druk eerst STOP";
            } else if (phone_command == PHONE_WHITE) {
                capture_begin(STATE_WHITE);
                result =
                    "Witmeting gestart: houd circa 2 seconden stil";
            } else if (phone_command == PHONE_BLACK) {
                capture_begin(STATE_BLACK);
                result =
                    "Zwartmeting gestart: houd circa 2 seconden stil";
            } else if (phone_command == PHONE_CLEAR) {
                calibration_clear();
                result = "Kalibratie gewist";
            } else if (!calibrated) {
                result =
                    "Start geweigerd: kalibreer eerst wit en zwart";

            } else {
                portENTER_CRITICAL(&web_lock);
                const bool can_start = !config_writing && !tuning_pending;
                if (can_start) control_active = true;
                portEXIT_CRITICAL(&web_lock);
                if (can_start) {
                    state = STATE_COUNTDOWN;
                    state_started = now;
                    pd_reset();
                }

                portENTER_CRITICAL(&web_lock);
                web_reason[0] = 0;
                portEXIT_CRITICAL(&web_lock);

                result = can_start
                    ? "Start over 3 seconden: plaats de robot op de lijn"
                    : "Instellingen worden verwerkt: probeer start opnieuw";
            }


            portENTER_CRITICAL(&web_lock);
            snprintf(
                command_result, sizeof(command_result),
                "%s", result
            );
            command_applied = phone_id;
            portEXIT_CRITICAL(&web_lock);
        }

        if (!web_stop && !adc_fault) {
            switch (state) {
                case STATE_IDLE:
                    break;

                case STATE_WHITE:
                case STATE_BLACK:
                    capture_update(raw);
                    break;

                case STATE_COUNTDOWN:
                    if (
                        (TickType_t)(now - state_started) >=
                        pdMS_TO_TICKS(START_DELAY_MS)
                    ) {
                        if (!line_found) {
                            robot_stop("geen lijn bij start");
                        } else {
                            state = STATE_RUNNING;

                            portENTER_CRITICAL(&web_lock);
                            web_reason[0] = 0;
                            web_max_calculation_us = 0;
                            portEXIT_CRITICAL(&web_lock);
                            max_calculation_us = 0;

                            state_started = now;
                            last_line_seen = now;
                            last_turn_direction = 0;
                            line_recovering = false;
                            junction_active = false;
                            last_junction_us = 0;

                            pd_reset();
                            follow_line(position);

                        }
                    }
                    break;

                case STATE_RUNNING:
                    if (line_found) {
                        if (line_recovering) {
                            pd_reset();
                        }

                        line_recovering = false;
                        last_line_seen = now;
                        follow_line(position);

                        int dark_sensor_count = 0;
                        for (int i = 0; i < SENSOR_COUNT; i++) {
                            if (normalized[i] >= LINE_MIN_PEAK) {
                                dark_sensor_count++;
                            }
                        }

                        if (
                            dark_sensor_count >= JUNCTION_MIN_SENSORS
                        ) {
                            if (!junction_active) {
                                int64_t junction_us =
                                    esp_timer_get_time();
                                portENTER_CRITICAL(&web_lock);
                                ++junction_count;
                                junction_interval_us = last_junction_us == 0
                                    ? 0 : junction_us - last_junction_us;
                                portEXIT_CRITICAL(&web_lock);
                                last_junction_us = junction_us;
                                junction_active = true;
                            }
                        } else {
                            junction_active = false;
                        }
                    } else {
                        junction_active = false;
                        TickType_t lost_duration =
                            (TickType_t)(now - last_line_seen);

                        if (!line_recovering) {
                            line_recovering = true;
                            pd_reset();
                        }

                        if (
                            last_turn_direction != 0 ||
                            lost_duration < pdMS_TO_TICKS(LINE_RECOVERY_MS)
                        ) {
                            recover_line(last_turn_direction);
                        } else {
                            robot_stop("lijn niet teruggevonden");
                        }
                    }
                    break;
            }
        }

        if ((TickType_t)(now - last_web_update) >= pdMS_TO_TICKS(100)) {
            last_web_update = now;
            portENTER_CRITICAL(&web_lock);

            snprintf(
                web_state, sizeof(web_state), "%s",
                adc_fault
                    ? "ADC FOUT - HERSTART NODIG"
                    : state_text()
            );

            web_left = left_pwm;
            web_right = right_pwm;
            web_calibrated = calibrated;
            web_line = line_found;
            web_position = position;
            for (int i = 0; i < SENSOR_COUNT; ++i) {
                web_raw[i] = raw[i];
                web_normalized[i] = normalized[i];
                web_white_reference[i] = white_reference[i];
                web_black_reference[i] = black_reference[i];
            }
            web_white = white_available;
            web_black = black_available;

            web_samples =
                (state == STATE_WHITE || state == STATE_BLACK)
                    ? reference_count
                    : 0;
            web_max_calculation_us = max_calculation_us;

            portEXIT_CRITICAL(&web_lock);
        }

        const int64_t calculation_end_us = esp_timer_get_time();
        const uint32_t calculation_us =
            (uint32_t)(calculation_end_us - calculation_start_us);
        if (calculation_us > max_calculation_us) {
            max_calculation_us = calculation_us;
        }

        // Publiceer iedere cyclus; debug vraagt een reset aan bij de eigenaar.
        portENTER_CRITICAL(&web_lock);
        web_max_calculation_us = max_calculation_us;
        ++timing_samples;
        portEXIT_CRITICAL(&web_lock);

        if (calculation_us >= timer_period_us) {
            portENTER_CRITICAL(&web_lock);
            ++deadline_misses;
            portEXIT_CRITICAL(&web_lock);
            if (state == STATE_RUNNING) robot_stop("Rekentijd te lang: verhoog cyclustijd");
        }
        if (timer_period_us != tuning.cycle_time_us) {
            ESP_ERROR_CHECK(esp_timer_stop(cycle_timer));
            ulTaskNotifyTake(pdTRUE, 0);
            timer_period_us = tuning.cycle_time_us;
            ESP_ERROR_CHECK(esp_timer_start_periodic(cycle_timer, timer_period_us));
        }
    }
}

static void robot_setup(void) {
    motors_init();

    if (
        xTaskCreatePinnedToCore(
            robot_task, "robot", 8192,
            NULL, 5, NULL, 1
        ) != pdPASS
    ) {
        ESP_LOGE(
            TAG, "Robottaak niet gestart; motoren blijven uit."
        );
    }
}
