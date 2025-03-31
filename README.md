# Nuki Rest Bridge – Control Nuki Smart Locks via REST and ESP32

![Beta Status](https://img.shields.io/badge/status-beta-yellow)
[![Project Maintenance](https://img.shields.io/maintenance/yes/2024.svg)](https://github.com/CalDymos/nuki-restbridge 'GitHub Repository')
[![License](https://img.shields.io/github/license/CalDymos/nuki-restbridge.svg)](https://github.com/CalDymos/nuki-restbridge/blob/main/LICENSE 'License')


🚧 **Beta-Status**: This project is in the **beta phase**. Most features are implemented, but there may still be bugs or incomplete parts. Use with caution.

---

## 📚 Table of Contents

- [Overview](#-overview)
- [Supported Devices](#-supported-devices)
- [Recommended ESP32 Boards](#-recommended-esp32-boards)
- [Not Recommended](#-not-recommended)
- [Getting Started](#-getting-started)
  - [Installation](#installation)
  - [Initial Network Setup](#initial-network-setup)
  - [Pairing with Nuki Lock](#pairing-with-a-nuki-lock-10-to-40)
- [Configuration](#-configuration)
  - [Network Configuration](#network-configuration)
    - [General Settings](#general-settings)
    - [IP Address assignment](#ip-address-assignment)
  - [REST API Configuration](#rest-api-configuration)
  - [HAR Configuration](#har-configuration)
    - [Connection Settings](#connection-settings)
    - [Report Settings](#report-settings)
      - [General](#general)
      - [Key Turner State](#key-turner-state)
      - [Battery Report](#battery-report)

---

## 📌 Overview

**Nuki REST Bridge** allows full local integration of Nuki Smart Locks via REST – no cloud, no MQTT broker required. It runs on an ESP32 and connects to Nuki devices over BLE while exposing a simple REST API.

> 🔗 **Fork Notice**: This project is a **disconnected fork** of [Nuki Hub](https://github.com/technyon/nuki_hub). It's not actively synchronized with upstream and has diverged significantly.

- Offers direct REST access for smart home platforms (e.g., Loxone Miniserver).
- Ideal for standalone environments without MQTT or external bridges.

---

## ✅ Supported Devices

### ESP32 Boards (Wi-Fi + BLE)

| ✅ Supported | ❌ Not Supported |
|-------------|------------------|
| All dual-core ESP32 boards with Wi-Fi + BLE supported by ESP-IDF 5.3.2 / Arduino Core 3.1.3 | ESP32-S2 (no BLE) |

### Nuki Devices

- Nuki Smart Lock 1.0 → 4.0

### Ethernet Support

The following boards with **built-in Ethernet** are supported:

- [Olimex ESP32-POE / POE-ISO](https://www.olimex.com/Products/IoT/ESP32/)
- [WT32-ETH01](http://en.wireless-tag.com/product-item-2.html)
- [LilyGO T-ETH / T-ETH-POE](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series)
- [Waveshare ESP32-S3-ETH / POE](https://www.waveshare.com/esp32-s3-eth.htm)
- [ESP32-poe-dev board](https://github.com/jorticus/esp32-poe-dev)
- [wESP32](https://wesp32.com/)

> 💡 If using PoE: A [PoE to USB/Ethernet splitter](https://www.berrybase.de/poe-splitter-rj45-48v-usb-type-c-stecker-5v-2-5a) can also be used.

---

## ✅ Recommended ESP32 Boards

We recommend using dual-core ESP32 boards with stable BLE + Wi-Fi support and sufficient RAM:

- ESP32-WROOM-32 (standard dual-core ESP32)
- ESP32-WROVER (more RAM, ideal for PSRAM and SSL)
- ESP32-DevKitC

Avoid low-RAM models if using HAR or logging features extensively.

## 🚫 Not Recommended

Avoid single-core ESP32 variants such as:

- ESP32-C3
- ESP32-C6
- ESP32-H2
- ESP32-Solo1

The bridge uses both CPU cores to handle BLE scanning, client connections, and webserver tasks. Single-core models may experience performance issues or instability.

---

## 🚀 Getting Started

### Installation

Flash the firmware to your ESP32 board using the provided binaries or by compiling via PlatformIO.

### Initial Network Setup

1. Power on the ESP32.
2. Connect to the new Wi-Fi AP: `NukiRestBridge`
   - Password: `NukiBridgeESP32`
3. Open browser: [http://192.168.4.1](http://192.168.4.1)
4. Select and connect to your home Wi-Fi.

ESP32 will then connect to the selected network.

### Pairing with a Nuki Lock (1.0 to 4.0)

1. Enable *Bluetooth pairing* in the Nuki app:
   - Settings → Features & Configuration → Button and LED
2. Hold button on Nuki device for a few seconds.
3. ESP32 will auto-pair whenever it is powered and if no lock is currently paired.
4. Web UI will confirm with: `Paired: Yes`, if successful.

---

## 🛠️ Configuration

In your browser, open the IP address of the ESP32 to access the Web Config interface.

You can configure:

### Network Configuration

#### General Settings

- *Host name*: Set the hostname for the Nuki Rest Bridge ESP
- *Network hardware*: "Wi-Fi only" by default, set to ethernet if available
- *RSSI send interval*: Set to a positive integer to specify the number of seconds between sending the current Wi-Fi RSSI; set to -1 to disable, default value 60
**(Requires Home Automation Reporting to be enabled)**
- *Restart on disconnect*: Enable to restart the Nuki Rest Bridge when disconnected from the network.
- *Find Wi-Fi AP with strongest signal*: Uses the AP with the strongest signal for the connection via Wi-Fi

#### IP Address Assignment

- *Enable DHCP*: Enable to use DHCP for obtaining an IP address, disable to use the static IP settings below
- *Static IP address*: When DHCP is disabled set to the preferred static IP address for the Nuki Bridge to use
- *Subnet*: When DHCP is disabled set to the preferred subnet for the Nuki Hub to use
- *Default gateway*: When DHCP is disabled set to the preferred gateway IP address for the Nuki Bridge to use
- *DNS Server*: When DHCP is disabled set to the preferred DNS server IP address for the Nuki Bridge to use

---

### REST API Configuration

- *Enable REST API*: Activate the Rest Web Server to receive requests
- *API Port*: Set the port number for the REST API (default: 80)
- *Access Token*: Set an access token to secure the REST API.

---

### HAR Configuration

#### Connection Settings

- *Enable Home Automation Report*: Enable to periodically send status updates (e.g. RSSI, lock state, battery) to a Home Automation system.
- *Address*: Set to the IP address of Home Automation
- *Port*: Set to the Port of Home Automation
- *Mode*: (UDP / REST)
- *User*: Optional username for authenticating with the Home Automation system. Use `#` to disable authentication.
- *Password*: Password corresponding to the username. Use `#` to disable authentication.


#### Report Settings

##### General

- *State Path*: Home Automation API path that returns the status of the HAR in response to a GET request. Used as an additional check to verify if the Home Automation system is reachable. Leave empty to disable status check.
- *Uptime Path*: URL path to report ESP system uptime to Home Automation (e.g. `/api/system/uptime`)
- *Uptime Query*: Optional query string to append to the request (e.g. `?value=`)

- *Restart Reason FW Path*: URL path to report the firmware restart reason to Home Automation (e.g. `/api/system/fwrestart`)
- *Restart Reason FW Query*: Optional query string to append to the request (e.g. `?value=`)

- *Restart Reason ESP Path*: URL path to report the ESP restart reason to Home Automation (e.g. `/api/system/esprestart`)
- *Restart Reason ESP Query*: Optional query string to append to the request (e.g. `?value=`)

- *Info Nuki Bridge Version Path*: URL path to report the Nuki Bridge firmware version (e.g. `/api/system/version`)
- *Info Nuki Bridge Version Query*: Optional query string to append to the request (e.g. `?value=`)


##### Key Turner State

- *Lock State Path*: URL path to report the current lock state (e.g. `/api/lock/state`)
- *Lock State Query*: Optional query string to append to the request (e.g. `?value=`)

- *Lock 'N' Go Path*: URL path to report if a Lock ’n’ Go action was triggered (e.g. `/api/lock/lockngo`)
- *Lock 'N' Go Query*: Optional query string to append to the request (e.g. `?value=`)

- *Lock Trigger Path*: URL path to report the last trigger source (e.g. `/api/lock/trigger`)
- *Lock Trigger Query*: Optional query string to append to the request (e.g. `?source=`)


##### Battery Report

- *Battery Voltage Path*: URL path to report the current battery voltage (e.g. `/api/battery/voltage`)
- *Battery Voltage Query*: Optional query string to append to the request (e.g. `?v=`)

- *Battery Drain Path*: URL path to report battery drain rate (e.g. `/api/battery/drain`)
- *Battery Drain Query*: Optional query string to append to the request (e.g. `?value=`)

- *Battery Max Turn Current Path*: URL path to report the peak current while turning the motor (e.g. `/api/battery/maxcurrent`)
- *Battery Max Turn Current Query**: Optional query string to append to the request (e.g. `?value=`)

- *Battery Lock Distance Path*: URL path to report motor lock distance (e.g. `/api/battery/lockdistance`)
- *Battery Lock Distance Query*: Optional query string to append to the request (e.g. `?value=`)
