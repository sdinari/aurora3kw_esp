# aurora3kw_esp

An ESP8266-based monitoring bridge that reads operational data from an **ABB / Power-One Aurora 3kW inverter** via RS485 and uploads it automatically to **PVOutput.org**.

The project features a modern, responsive local web interface for easy initial configuration (Wi-Fi, API keys, inverter parameters) and persistent storage using LittleFS.

---

## 🚀 Features

* **Non-blocking Execution:** The portal remains highly responsive at all times; data reading and uploads occur on an asynchronous timer.
* **Captive Portal / AP Mode:** On the first boot (or if Wi-Fi connection fails), the device spawns an open Access Point (`InverterConfig`) for seamless onboarding at `http://192.168.4.1`.
* **Web Dashboard Configuration:** Once connected to your local network, settings can be updated on-the-fly at `http://<device-ip>/config`.
* **Robust Error Management:** If the inverter fails to reply (e.g., at night), the system safely backs off and schedules a fast retry (20 seconds) without stalling the system.
* **Secure Communication:** Data transmission to PVOutput.org is performed over a secure HTTPS client layer (`BearSSL`).

---

## 🛠️ Hardware & Schematics

The project uses an **ESP8266 (NodeMCU)**, a **TTL to RS485 converter**, and a **Logic Level Shifter (5V ↔ 3.3V)** to ensure safe signaling between the ESP's 3.3V lines and the RS485 transceiver.

### Wiring Diagram
![Alt text](./aurora_esp2_schematic.png?raw=true "Title")

* **ESP8266 TX/RX Pins:** Configured dynamically via SoftwareSerial within the driver.
* **Inverter side:** Connect `A+`, `B-`, and `GND` to the corresponding RS485 terminal strip inside your Aurora inverter.

---

## 🖥️ Web Configuration Portal

No hardcoded credentials needed. Everything is handled through a clean, native web interface:

<img width="1059" height="939" alt="image" src="https://github.com/user-attachments/assets/5ef65c90-d211-47bc-8506-9f2b42645d8d" />


### Parameters Breakdown:
* **WIFI:** Target network SSID and Password.
* **PVOUTPUT:** Your personal API Key and System ID.
* **ONDULEUR / RS485:** Inverter address (Default: `2`) and communication baud rate (Default: `19200`).
* **ENVOI DES DONNÉES:** Upload interval in milliseconds (e.g., `240000` for 4 minutes).

---

## 📂 Architecture & Files

The codebase is organized modularly to simplify testing and future evolution:

* **`inverter_monitor.ino`**: Main application file handling the state machine, NTP time retrieval, and the main non-blocking upload loop.
* **`Config.h`**: Standard configuration structure (`AppConfig`) backed by `LittleFS` for reading and writing parameters to flash.
* **`Inverter.h`**: Low-level protocol abstraction layer dedicated to building and parsing the serial RS485 query frames for the Aurora inverter.
* **`WebConfig.h`**: Embedded HTTP server serving the HTML forms and processing the incoming configuration payloads.

---

## 📦 Requirements & Libraries

Make sure you have installed the following dependencies in your Arduino IDE (via **Library Manager**):

1.  **ArduinoJson** *(by Benoit Blanchon)* – Used for config parsing and structural data saving.
2.  **NTPClient** *(by Fabrice Weinberg)* – Or your custom drop-in library variant (`NTPCentral.h`).

---

## 📝 License

This project is open-source and available under the MIT License. Feel free to fork, adapt, and improve it for your own solar installations.
