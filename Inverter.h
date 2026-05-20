#pragma once
#include <Arduino.h>
#include <SoftwareSerial.h>

// ─── Pin definitions ───────────────────────────────────────────────────────
#define PIN_RS485_TX_CTRL  4   // D2 – DE/RE control
#define PIN_RS485_TX       14  // D5
#define PIN_RS485_RX       5   // D1

#define RS485_TX HIGH
#define RS485_RX LOW

// ─── Aurora protocol codes ────────────────────────────────────────────────
#define CMD_STATE          50
#define CMD_DSP            59
#define CMD_CUMULATED      78
#define CMD_SERIAL         63

#define DSP_GRID_VOLTAGE    1
#define DSP_GRID_CURRENT    2
#define DSP_GRID_POWER      3
#define DSP_BOOSTER_TEMP   22   // booster temperature (not inverter temp = 21)
#define DSP_INPUT1_VOLTAGE 23
#define DSP_INPUT1_CURRENT 25
#define DSP_POWER_PEAK_DAY 35

// ─── Result structs ───────────────────────────────────────────────────────
struct InverterState {
  uint8_t transmissionState;
  uint8_t globalState;
  uint8_t inverterState;
  uint8_t channel1State;
  uint8_t channel2State;
  uint8_t alarmState;
  bool    valid;
};

struct InverterData {
  float   energyToday_Wh;   // v1
  float   powerNow_W;       // v2
  float   temperature_C;    // v5
  float   voltage1_V;       // v6
  float   current1_A;
  float   powerPeakToday_W;
  bool    valid;
};

// ─── Class ────────────────────────────────────────────────────────────────
class Inverter {
public:
  Inverter(uint8_t address, uint32_t baudRate)
    : _address(address), _baudRate(baudRate),
      _serial(PIN_RS485_RX, PIN_RS485_TX) {}

  void begin() {
    _serial.begin(_baudRate);
    _serial.setTimeout(1000);
    pinMode(PIN_RS485_TX_CTRL, OUTPUT);
    pinMode(PIN_RS485_TX,      OUTPUT);
    pinMode(PIN_RS485_RX,      INPUT);
    digitalWrite(PIN_RS485_TX_CTRL, RS485_RX);
  }

  // ── Read inverter status register ───────────────────────────────────────
  InverterState readState() {
    InverterState s{};
    s.valid = send(CMD_STATE, 0, 0, 0, 0, 0, 0);

    if (!s.valid) {
      memset(&_rx, 0xFF, sizeof(_rx));
    }
    s.transmissionState = _rx[0];
    s.globalState       = _rx[1];
    s.inverterState     = _rx[2];
    s.channel1State     = _rx[3];
    s.channel2State     = _rx[4];
    s.alarmState        = _rx[5];
    return s;
  }

  // ── Read all measured data ───────────────────────────────────────────────
  InverterData readData() {
    InverterData d{};
    d.energyToday_Wh    = readDsp(DSP_GRID_POWER);       // cumulated today via cmd 78
    d.powerNow_W        = readDsp(DSP_GRID_POWER);
    d.temperature_C     = readDsp(DSP_BOOSTER_TEMP);
    d.voltage1_V        = readDsp(DSP_INPUT1_VOLTAGE);
    d.current1_A        = readDsp(DSP_INPUT1_CURRENT);
    d.powerPeakToday_W  = readDsp(DSP_POWER_PEAK_DAY);

    // Energy (Wh) uses a dedicated command
    if (send(CMD_CUMULATED, 0, 0, 0, 0, 0, 0)) {
      d.energyToday_Wh = (_rx[4] * 256.0f) + _rx[5];
    } else {
      d.energyToday_Wh = -1.0f;
    }

    d.valid = (d.energyToday_Wh   >= 0) &&
              (d.powerNow_W       >= 0) &&
              (d.temperature_C    >= 0) &&
              (d.voltage1_V       >= 0);
    return d;
  }

  // ── Read inverter serial number ──────────────────────────────────────────
  String readSerialNumber() {
    if (!send(CMD_SERIAL, 0, 0, 0, 0, 0, 0)) return F("N/A");
    return String((char)_rx[0]) + (char)_rx[1] + (char)_rx[2]
         + (char)_rx[3]         + (char)_rx[4] + (char)_rx[5];
  }

  // ── Human-readable state labels ──────────────────────────────────────────
  static const __FlashStringHelper* globalStateLabel(uint8_t id);
  static const __FlashStringHelper* alarmLabel(uint8_t id);

private:
  uint8_t        _address;
  uint32_t       _baudRate;
  SoftwareSerial _serial;
  uint8_t        _rx[8];

  // ── CRC-16 (Modbus-like) ─────────────────────────────────────────────────
  static uint16_t crc16(const uint8_t *data, uint8_t len) {
    uint8_t lo = 0xFF, hi = 0xFF;
    for (uint8_t i = 0; i < len; i++) {       // ← fixed: was data[offset+i]
      uint8_t n   = data[i] ^ lo;
      uint8_t tmp = n << 4;
      n   = tmp ^ n;
      tmp = n >> 5;
      lo  = hi;
      hi  = n ^ tmp;
      tmp = n << 3;
      lo ^= tmp;
      tmp = n >> 4;
      lo ^= tmp;
    }
    return (uint16_t)(~hi) << 8 | (uint8_t)(~lo);
  }

  // ── Low-level send / receive ─────────────────────────────────────────────
  bool send(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3,
            uint8_t p4, uint8_t p5, uint8_t p6) {
    uint8_t frame[10];
    frame[0] = _address;
    frame[1] = cmd;
    frame[2] = p1; frame[3] = p2; frame[4] = p3;
    frame[5] = p4; frame[6] = p5; frame[7] = p6;
    uint16_t crc = crc16(frame, 8);
    frame[8]  = lowByte(crc);
    frame[9]  = highByte(crc);

    memset(_rx, 0, sizeof(_rx));

    digitalWrite(PIN_RS485_TX_CTRL, RS485_TX);
    delay(50);
    _serial.write(frame, sizeof(frame));
    _serial.flush();
    digitalWrite(PIN_RS485_TX_CTRL, RS485_RX);
    delay(200);

    if (!_serial.available()) return false;
    if (_serial.readBytes(_rx, sizeof(_rx)) < 8) return false;

    uint16_t rxCrc  = (uint16_t)_rx[7] << 8 | _rx[6];
    uint16_t calcCrc = crc16(_rx, 6);
    bool ok = (rxCrc == calcCrc);
    _serial.flush();
    return ok;
  }

  // ── Read one DSP value (float, 4 bytes) ──────────────────────────────────
  float readDsp(uint8_t code) {
    if (!send(CMD_DSP, code, 0, 0, 0, 0, 0)) return -1.0f;
    delay(50);
    union { float f; uint8_t b[4]; } u;
    u.b[3] = _rx[2]; u.b[2] = _rx[3];
    u.b[1] = _rx[4]; u.b[0] = _rx[5];
    return u.f;
  }
};

// ─── State/alarm label implementations (PROGMEM) ─────────────────────────
const __FlashStringHelper* Inverter::globalStateLabel(uint8_t id) {
  switch (id) {
    case 0:  return F("Sending Parameters");
    case 1:  return F("Wait Sun/Grid");
    case 2:  return F("Checking Grid");
    case 3:  return F("Measuring Riso");
    case 4:  return F("DcDc Start");
    case 5:  return F("Inverter Start");
    case 6:  return F("Run");
    case 7:  return F("Recovery");
    case 8:  return F("Pause");
    case 9:  return F("Ground Fault");
    case 10: return F("OTH Fault");
    case 12: return F("Self Test");
    case 13: return F("Self Test Fail");
    case 15: return F("Leak Fault");
    case 16: return F("Waiting Manual Reset");
    case 25: return F("Remote OFF");
    case 30: return F("Waiting Sun");
    case 31: return F("Temperature Fault");
    case 32: return F("Fan Stucked");
    case 35: return F("DC Switch Open");
    default: return F("Unknown");
  }
}

const __FlashStringHelper* Inverter::alarmLabel(uint8_t id) {
  switch (id) {
    case 0:  return F("No Alarm");
    case 1:  return F("Sun Low");
    case 2:  return F("Input OC");
    case 3:  return F("Input UV");
    case 4:  return F("Input OV");
    case 7:  return F("Bulk OV");
    case 9:  return F("Output OC");
    case 13: return F("Grid Fail");
    case 19: return F("Over Temp");
    case 32: return F("Grid OV");
    case 33: return F("Grid UV");
    case 47: return F("Fan Fail");
    case 50: return F("Remote Off");
    default: return F("Unknown");
  }
}
