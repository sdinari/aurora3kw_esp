// ═════════════════════════════════════════════════════════════════════════════
//  Inverter Monitor – ESP8266
//  Reads Aurora/ABB inverter via RS485 and uploads to PVOutput.org
//
//  Files:
//    inverter_monitor.ino  ← this file (main loop)
//    Config.h              ← AppConfig struct + LittleFS persistence
//    Inverter.h            ← RS485 / Aurora protocol driver
//    WebConfig.h           ← HTML configuration portal
//
//  Libraries required (install via Library Manager):
//    - ArduinoJson  (Benoit Blanchon)
//    - NTPClient    (Fabrice Weinberg)  — or your NTPCentral drop-in
//
//  First boot:  no config file → AP mode "InverterConfig"
//               Connect to that WiFi → browse http://192.168.4.1
//               Fill in credentials → Save → device reboots in normal mode.
//  Later:       page stays accessible at http://<device-ip>/config
// ═════════════════════════════════════════════════════════════════════════════

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include "NTPCentral.h"   // your custom NTP wrapper

#include "Config.h"
#include "Inverter.h"
#include "WebConfig.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
AppConfig cfg;

Inverter    *inverter  = nullptr;
WebConfig   *webConfig = nullptr;

WiFiUDP   ntpUDP;
NTPCentral *timeClient = nullptr;

BearSSL::WiFiClientSecure httpsClient;

static const char PVO_HOST[] = "pvoutput.org";
static const int  PVO_PORT   = 443;

// ─── Forward declarations ─────────────────────────────────────────────────────
bool  wifiConnect();
void  startAPMode();
bool  uploadToPVOutput(const InverterData &data, const String &date, const String &time);
void  printData(const InverterData &data);

// ═════════════════════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\n[BOOT] Inverter Monitor starting"));

  // 1. Load config from flash
  configLoad(cfg);

  // 2. Instantiate objects with loaded config
  inverter   = new Inverter(cfg.inverter_address, cfg.baud_rate);
  timeClient = new NTPCentral(ntpUDP, "pool.ntp.org", cfg.utc_offset_seconds);
  webConfig  = new WebConfig(cfg);

  inverter->begin();

  // 3. Try to connect to WiFi
  bool connected = wifiConnect();

  if (!connected) {
    // No WiFi credentials or connection failed → start config AP
    Serial.println(F("[BOOT] Starting AP configuration mode"));
    startAPMode();
    return;   // everything handled in loop() via webConfig->handle()
  }

  // 4. Normal mode: start NTP + web config page
  timeClient->begin();
  webConfig->begin();
  httpsClient.setInsecure();   // fingerprints change; use setFingerprint() for stricter security
  httpsClient.setTimeout(15000);

  Serial.println(F("[BOOT] Ready"));
}

// ═════════════════════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════════════════════
void loop() {
  // Always serve the config portal
  webConfig->handle();

  // If we're in AP mode (no WiFi), just handle config and return
  if (WiFi.status() != WL_CONNECTED) {
    webConfig->handle();
    delay(10);
    return;
  }

  // ── Update NTP time ────────────────────────────────────────────────────
  timeClient->update();

  // ── Build date / time strings for PVOutput ────────────────────────────
  String formattedDate = timeClient->getFormattedDate();   // "2025-05-20T10:30:00Z"
  int    splitT        = formattedDate.indexOf('T');
  String dateStr       = formattedDate.substring(0, splitT);  // "2025-05-20"
  dateStr.replace("-", "");                                    // "20250520"
  String timeStr = String(timeClient->getHours()) + ":" + timeClient->getMinutes();

  // ── Read inverter serial (debug) ──────────────────────────────────────
  Serial.println(F("\n[INV] Reading inverter..."));
  Serial.print(F("  Serial : ")); Serial.println(inverter->readSerialNumber());

  // ── Read all power data ───────────────────────────────────────────────
  InverterData data = inverter->readData();
  printData(data);

  // ── Upload to PVOutput if data is valid ───────────────────────────────
  if (data.valid) {
    uploadToPVOutput(data, dateStr, timeStr);
  } else {
    Serial.println(F("[INV] Data invalid – skipping upload, retry in 20 s"));
    delay(20000);
    return;
  }

  // ── Wait for next upload interval ─────────────────────────────────────
  Serial.printf("[MAIN] Next upload in %lu ms\n", cfg.upload_interval_ms);
  // Use a non-blocking wait so the web server stays responsive
  unsigned long t0 = millis();
  while (millis() - t0 < cfg.upload_interval_ms) {
    webConfig->handle();
    delay(50);
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  WiFi helpers
// ═════════════════════════════════════════════════════════════════════════════
bool wifiConnect() {
  if (strlen(cfg.wifi_ssid) == 0) {
    Serial.println(F("[WiFi] No SSID configured"));
    return false;
  }

  Serial.printf("[WiFi] Connecting to %s ", cfg.wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);

  const uint8_t maxAttempts = 30;
  for (uint8_t i = 0; i < maxAttempts; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
    Serial.print('.');
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F(" FAILED"));
    return false;
  }

  Serial.println(F(" OK"));
  Serial.print(F("[WiFi] IP: ")); Serial.println(WiFi.localIP());
  return true;
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("InverterConfig");   // open AP, no password
  Serial.print(F("[AP] IP: ")); Serial.println(WiFi.softAPIP());
  webConfig->begin();
  // loop() will keep calling webConfig->handle()
}

// ═════════════════════════════════════════════════════════════════════════════
//  PVOutput upload
// ═════════════════════════════════════════════════════════════════════════════
bool uploadToPVOutput(const InverterData &data, const String &date, const String &time) {
  Serial.println(F("[PVO] Connecting..."));

  uint8_t attempts = 0;
  while (!httpsClient.connect(PVO_HOST, PVO_PORT) && attempts < 15) {
    delay(200);
    attempts++;
  }
  if (attempts >= 15) {
    Serial.println(F("[PVO] Connection failed"));
    return false;
  }

  // Build query string
  // v1=energy today (Wh), v2=power now (W), v5=temperature, v6=voltage
  String path = "/service/r2/addstatus.jsp"
                "?key="  + String(cfg.pvo_api_key) +
                "&sid="  + String(cfg.pvo_system_id) +
                "&d="    + date +
                "&t="    + time +
                "&v1="   + String(data.energyToday_Wh, 0) +
                "&v2="   + String(data.powerNow_W,     0) +
                "&v5="   + String(data.temperature_C,  1) +
                "&v6="   + String(data.voltage1_V,     1);

  Serial.print(F("[PVO] GET ")); Serial.println(path);

  httpsClient.print(
    String("GET ") + path + " HTTP/1.1\r\n"
    "Host: " + PVO_HOST + "\r\n"
    "Connection: close\r\n\r\n"
  );

  // Read response status line
  String statusLine = httpsClient.readStringUntil('\n');
  Serial.print(F("[PVO] Response: ")); Serial.println(statusLine);

  // Drain headers
  while (httpsClient.connected()) {
    if (httpsClient.readStringUntil('\n') == "\r") break;
  }

  // Print body
  while (httpsClient.available()) {
    Serial.println(httpsClient.readStringUntil('\n'));
  }
  httpsClient.stop();

  bool ok = statusLine.indexOf("200") > 0;
  Serial.println(ok ? F("[PVO] Upload OK") : F("[PVO] Upload FAILED"));
  return ok;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Debug print
// ═════════════════════════════════════════════════════════════════════════════
void printData(const InverterData &data) {
  Serial.printf("  Energy today   : %.0f Wh\n",   data.energyToday_Wh);
  Serial.printf("  Power now      : %.0f W\n",    data.powerNow_W);
  Serial.printf("  Temperature    : %.1f °C\n",   data.temperature_C);
  Serial.printf("  Voltage (in1)  : %.1f V\n",    data.voltage1_V);
  Serial.printf("  Current (in1)  : %.2f A\n",    data.current1_A);
  Serial.printf("  Power peak/day : %.0f W\n",    data.powerPeakToday_W);
  Serial.printf("  Valid          : %s\n",         data.valid ? "YES" : "NO");
}
