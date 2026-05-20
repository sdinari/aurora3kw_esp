#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ─── Default values (used on first boot) ───────────────────────────────────
#define DEFAULT_WIFI_SSID        ""
#define DEFAULT_WIFI_PASSWORD    ""
#define DEFAULT_PVO_API_KEY      ""
#define DEFAULT_PVO_SYSTEM_ID    ""
#define DEFAULT_BAUD_RATE        19200
#define DEFAULT_INVERTER_ADDR    2
#define DEFAULT_UTC_OFFSET       3600      // UTC+1
#define DEFAULT_UPLOAD_INTERVAL  240000    // 4 min in ms
#define CONFIG_FILE              "/config.json"

struct AppConfig {
  char     wifi_ssid[64];
  char     wifi_password[64];
  char     pvo_api_key[64];
  char     pvo_system_id[16];
  uint8_t  inverter_address;
  uint32_t baud_rate;
  int32_t  utc_offset_seconds;
  uint32_t upload_interval_ms;
};

// ─── Fill with factory defaults ───────────────────────────────────────────
inline void configDefault(AppConfig &cfg) {
  strlcpy(cfg.wifi_ssid,       DEFAULT_WIFI_SSID,      sizeof(cfg.wifi_ssid));
  strlcpy(cfg.wifi_password,   DEFAULT_WIFI_PASSWORD,  sizeof(cfg.wifi_password));
  strlcpy(cfg.pvo_api_key,     DEFAULT_PVO_API_KEY,    sizeof(cfg.pvo_api_key));
  strlcpy(cfg.pvo_system_id,   DEFAULT_PVO_SYSTEM_ID,  sizeof(cfg.pvo_system_id));
  cfg.inverter_address   = DEFAULT_INVERTER_ADDR;
  cfg.baud_rate          = DEFAULT_BAUD_RATE;
  cfg.utc_offset_seconds = DEFAULT_UTC_OFFSET;
  cfg.upload_interval_ms = DEFAULT_UPLOAD_INTERVAL;
}

// ─── Load from LittleFS ───────────────────────────────────────────────────
inline bool configLoad(AppConfig &cfg) {
  configDefault(cfg); // always start from defaults

  if (!LittleFS.begin()) {
    Serial.println(F("[CFG] LittleFS mount failed"));
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    Serial.println(F("[CFG] No config file, using defaults"));
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("[CFG] JSON parse error: %s\n", err.c_str());
    return false;
  }

  strlcpy(cfg.wifi_ssid,       doc["wifi_ssid"]       | DEFAULT_WIFI_SSID,      sizeof(cfg.wifi_ssid));
  strlcpy(cfg.wifi_password,   doc["wifi_password"]   | DEFAULT_WIFI_PASSWORD,  sizeof(cfg.wifi_password));
  strlcpy(cfg.pvo_api_key,     doc["pvo_api_key"]     | DEFAULT_PVO_API_KEY,    sizeof(cfg.pvo_api_key));
  strlcpy(cfg.pvo_system_id,   doc["pvo_system_id"]   | DEFAULT_PVO_SYSTEM_ID,  sizeof(cfg.pvo_system_id));
  cfg.inverter_address   = doc["inverter_address"]   | DEFAULT_INVERTER_ADDR;
  cfg.baud_rate          = doc["baud_rate"]          | DEFAULT_BAUD_RATE;
  cfg.utc_offset_seconds = doc["utc_offset_seconds"] | DEFAULT_UTC_OFFSET;
  cfg.upload_interval_ms = doc["upload_interval_ms"] | DEFAULT_UPLOAD_INTERVAL;

  Serial.println(F("[CFG] Config loaded OK"));
  return true;
}

// ─── Save to LittleFS ─────────────────────────────────────────────────────
inline bool configSave(const AppConfig &cfg) {
  if (!LittleFS.begin()) return false;

  StaticJsonDocument<512> doc;
  doc["wifi_ssid"]           = cfg.wifi_ssid;
  doc["wifi_password"]       = cfg.wifi_password;
  doc["pvo_api_key"]         = cfg.pvo_api_key;
  doc["pvo_system_id"]       = cfg.pvo_system_id;
  doc["inverter_address"]    = cfg.inverter_address;
  doc["baud_rate"]           = cfg.baud_rate;
  doc["utc_offset_seconds"]  = cfg.utc_offset_seconds;
  doc["upload_interval_ms"]  = cfg.upload_interval_ms;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    Serial.println(F("[CFG] Failed to open config for writing"));
    return false;
  }

  serializeJson(doc, f);
  f.close();
  Serial.println(F("[CFG] Config saved OK"));
  return true;
}
