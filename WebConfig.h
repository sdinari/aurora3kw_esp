#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "Config.h"

// ─────────────────────────────────────────────────────────────────────────────
// WebConfig  –  serves a self-contained HTML configuration page.
//
//   AP mode  : ESP8266 broadcasts "InverterConfig" SSID (no password).
//              Browse to http://192.168.4.1  to configure.
//   STA mode : page is also available at http://<device-ip>/config
//              once the inverter is connected to your WiFi.
// ─────────────────────────────────────────────────────────────────────────────
class WebConfig {
public:
  WebConfig(AppConfig &cfg) : _cfg(cfg), _server(80), _saved(false) {}

  void begin() {
    _server.on("/",       [this]() { handleRoot(); });
    _server.on("/config", [this]() { handleConfig(); });
    _server.on("/save",   HTTP_POST, [this]() { handleSave(); });
    _server.onNotFound(   [this]() { _server.sendHeader("Location", "/"); _server.send(302); });
    _server.begin();
    Serial.println(F("[WEB] HTTP server started"));
  }

  void handle() { _server.handleClient(); }

  bool wasSaved() const { return _saved; }

private:
  AppConfig        &_cfg;
  ESP8266WebServer  _server;
  bool              _saved;

  // ── Redirect root → /config ───────────────────────────────────────────
  void handleRoot() {
    _server.sendHeader("Location", "/config");
    _server.send(302);
  }

  // ── Serve the config form ─────────────────────────────────────────────
  void handleConfig() {
    String html = F(
      "<!DOCTYPE html><html lang='fr'><head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Inverter Config</title>"
      "<style>"
      "body{font-family:sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:20px}"
      "h1{color:#00d4aa;margin-bottom:4px}p.sub{color:#888;margin-top:0;font-size:.9em}"
      ".card{background:#16213e;border-radius:10px;padding:20px;max-width:520px;margin:20px auto}"
      "h2{color:#00d4aa;font-size:1em;text-transform:uppercase;letter-spacing:.1em;border-bottom:1px solid #333;padding-bottom:6px}"
      "label{display:block;margin-top:12px;font-size:.85em;color:#aaa}"
      "input{width:100%;box-sizing:border-box;padding:8px 10px;border-radius:6px;"
            "border:1px solid #444;background:#0f3460;color:#eee;font-size:.95em;margin-top:4px}"
      "input:focus{outline:none;border-color:#00d4aa}"
      "button{margin-top:20px;width:100%;padding:10px;background:#00d4aa;color:#000;"
             "font-weight:bold;border:none;border-radius:6px;cursor:pointer;font-size:1em}"
      "button:hover{background:#00b894}"
      ".saved{background:#0a3d2e;border:1px solid #00d4aa;color:#00d4aa;padding:10px;border-radius:6px;text-align:center;margin-bottom:10px}"
      "</style></head><body>"
      "<div class='card'>"
      "<h1>&#9728; Inverter Monitor</h1>"
      "<p class='sub'>Configuration du système</p>"
    );

    if (_saved) {
      html += F("<div class='saved'>&#10003; Paramètres enregistrés – redémarrage en cours…</div>");
    }

    html += F("<form method='POST' action='/save'>");

    // ── WiFi
    html += F("<h2>WiFi</h2>");
    html += formField("wifi_ssid",     "SSID",              _cfg.wifi_ssid,     false);
    html += formField("wifi_password", "Mot de passe WiFi", _cfg.wifi_password, true);

    // ── PVOutput
    html += F("<h2>PVOutput</h2>");
    html += formField("pvo_api_key",   "Clé API",           _cfg.pvo_api_key,   true);
    html += formField("pvo_system_id", "System ID",         _cfg.pvo_system_id, false);

    // ── Inverter
    html += F("<h2>Onduleur / RS485</h2>");
    html += numField("inverter_address", "Adresse onduleur (1-32)", _cfg.inverter_address,   1,   32);
    html += numField("baud_rate",        "Vitesse RS485 (bps)",     _cfg.baud_rate,         300, 115200);
    html += numField("utc_offset_seconds","Décalage UTC (secondes)", _cfg.utc_offset_seconds,-43200, 50400);

    // ── Upload interval
    html += F("<h2>Envoi des données</h2>");
    html += numField("upload_interval_ms", "Intervalle d'envoi (ms, min=60000)", _cfg.upload_interval_ms, 60000, 3600000);

    html += F("<button type='submit'>&#128190; Enregistrer</button>");
    html += F("</form></div></body></html>");

    _server.send(200, "text/html", html);
  }

  // ── Process the form ──────────────────────────────────────────────────
  void handleSave() {
    if (_server.hasArg("wifi_ssid"))
      strlcpy(_cfg.wifi_ssid, _server.arg("wifi_ssid").c_str(), sizeof(_cfg.wifi_ssid));
    if (_server.hasArg("wifi_password") && _server.arg("wifi_password").length() > 0)
      strlcpy(_cfg.wifi_password, _server.arg("wifi_password").c_str(), sizeof(_cfg.wifi_password));
    if (_server.hasArg("pvo_api_key") && _server.arg("pvo_api_key").length() > 0)
      strlcpy(_cfg.pvo_api_key, _server.arg("pvo_api_key").c_str(), sizeof(_cfg.pvo_api_key));
    if (_server.hasArg("pvo_system_id"))
      strlcpy(_cfg.pvo_system_id, _server.arg("pvo_system_id").c_str(), sizeof(_cfg.pvo_system_id));
    if (_server.hasArg("inverter_address"))
      _cfg.inverter_address = _server.arg("inverter_address").toInt();
    if (_server.hasArg("baud_rate"))
      _cfg.baud_rate = _server.arg("baud_rate").toInt();
    if (_server.hasArg("utc_offset_seconds"))
      _cfg.utc_offset_seconds = _server.arg("utc_offset_seconds").toInt();
    if (_server.hasArg("upload_interval_ms"))
      _cfg.upload_interval_ms = _server.arg("upload_interval_ms").toInt();

    configSave(_cfg);
    _saved = true;
    handleConfig();                   // re-render form with success banner
    delay(3000);
    ESP.restart();                    // reboot with new settings
  }

  // ── HTML helpers ──────────────────────────────────────────────────────
  static String formField(const char *name, const char *label,
                          const char *value, bool isPassword) {
    String h = "<label>" + String(label) + "</label>"
               "<input type='" + String(isPassword ? "password" : "text") +
               "' name='" + name + "' value='" + String(value) + "'>";
    return h;
  }

  template<typename T>
  static String numField(const char *name, const char *label,
                         T value, long minv, long maxv) {
    return "<label>" + String(label) + "</label>"
           "<input type='number' name='" + name +
           "' value='" + String(value) +
           "' min='" + minv + "' max='" + maxv + "'>";
  }
};
