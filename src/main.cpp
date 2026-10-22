#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

// ---------- Settings ----------
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#include "wifi_secrets.example.h"
#endif

const int TRIG_PIN = 25;
const int ECHO_PIN = 26;                   // via 1k / 2k voltage divider

const uint32_t PERIOD_MS = 200;            // 5 Hz

// ---------- Globals ----------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
uint32_t lastTick = 0;

// ---------- Sensor ----------
void setupSensor() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

// Returns distance in cm, or -1 if no echo came back
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);            // 10 us pulse starts a measurement
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long us = pulseIn(ECHO_PIN, HIGH, 30000UL);   // 30 ms timeout (~5 m)
  if (us == 0) return -1.0f;
  return us * 0.0343f / 2.0f;              // d = v * t / 2, v = 0.0343 cm/us at 20 C
}

// ---------- Wi-Fi: join router, fall back to own access point ----------
bool startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Station IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Router not found. Access point IP: ");
  Serial.println(WiFi.softAPIP());         // 192.168.4.1
  return false;
}

// ---------- Arduino entry points ----------
void setup() {
  Serial.begin(115200);
  setupSensor();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

  startWiFi();

  server.addHandler(&ws);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();
}

void loop() {
  ws.cleanupClients();                     // free dead browser connections

  if (millis() - lastTick >= PERIOD_MS) {
    lastTick += PERIOD_MS;                 // fixed step keeps the average rate at 5 Hz

    float d = readDistanceCm();
    String msg = "{\"cm\":" + (d < 0 ? String("null") : String(d, 1)) + "}";
    ws.textAll(msg);                       // push to every connected browser

    Serial.println(msg);
  }
}