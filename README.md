# Nuki Rest Bridge – Control Nuki Smart Locks via REST and ESP32

![Pre-Release](https://img.shields.io/badge/status-pre--release-orange)
[![Project Maintenance](https://img.shields.io/maintenance/yes/2025.svg)](https://github.com/CalDymos/nuki-restbridge 'GitHub Repository')
[![License](https://img.shields.io/github/license/CalDymos/nuki-restbridge.svg)](https://github.com/CalDymos/nuki-restbridge/blob/main/LICENSE 'License')


🚧 **Pre-Release Status**: This project is in a **pre-release phase**. Most features are implemented and functional, but some components may still be incomplete, unstable, or subject to change. Use with caution.


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
  - [Nuki Configuration](#nuki-configuration)
    - [Basic Nuki Configuration](#basic-configuration)
    - [Advanced Nuki Configuration](#advanced-nuki-configuration)
  - [Access Level Configuration](#access-level-configuration)
    - [Nuki General Access Control](#nuki-general-access-control)
  - [Credentials](#credentials)
    - [Web Configurator Credentials](#web-configurator-credentials)
    - [Nuki Lock PIN](#nuki-lock-pin)
    - [Unpair Nuki Lock](#unpair-nuki-lock)
    - [Factory reset Nuki Bridge](#factory-reset-nuki-bridge)
- [Recommended LAN Setup](#recommended-lan-setup)

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

- **Host name**: Set the hostname for the Nuki Rest Bridge ESP
- **Network hardware**: "Wi-Fi only" by default, set to ethernet if available
- **RSSI send interval**: Set to a positive integer to specify the number of seconds between sending the current Wi-Fi RSSI; set to -1 to disable, default value 60
  > 📘: Requires Home Automation Reporting to be enabled
- **Restart on disconnect**: Enable to restart the Nuki Rest Bridge when disconnected from the network.
- **Find Wi-Fi AP with strongest signal**: Uses the AP with the strongest signal for the connection via Wi-Fi

#### IP Address Assignment

- **Enable DHCP**: Enable to use DHCP for obtaining an IP address, disable to use the static IP settings below
- **Static IP address**: When DHCP is disabled set to the preferred static IP address for the Nuki Bridge to use
- **Subnet**: When DHCP is disabled set to the preferred subnet for the Nuki Bridge to use
- **Default gateway**: When DHCP is disabled set to the preferred gateway IP address for the Nuki Bridge to use
- **DNS Server**: When DHCP is disabled set to the preferred DNS server IP address for the Nuki Bridge to use

---

### REST API Configuration

- **Enable REST API**: Activate the Rest Web Server to receive requests
- **API Port**: Set the port number for the REST API (default: 80)
- **Access Token**: Set an access token to secure the REST API.

---

### HAR Configuration

#### Connection Settings

- **Enable Home Automation Report**: Enable to periodically send status updates (e.g. RSSI, lock state, battery) to a Home Automation system.
- **Address**: Set to the IP address of Home Automation
- **Port**: Set to the Port of Home Automation
- **Mode**: Select either UDP or REST.
  >📘 In UDP mode, the fields marked as Path are ignored. Only the Param values are used. In REST mode, both Path and Query fields are required.
- **REST Request Method**: Select Methode for Rest Request GET/POST.
  >📘 only available if REST Mode is selected
- **User**: Optional username for authenticating with the Home Automation system. Use `#` to disable authentication.
  >📘 only available if REST Mode is selected
- **Password**: Password corresponding to the username. Use `#` to disable authentication.
  >📘 only available if REST Mode is selected


#### Report Settings

>⚠️ In REST Mode the Query field can optionally contain full query strings like ?action=set&ext=Extension1&io=Q1&value=.
The actual value is appended to the end of this string during transmission.
This allows compatibility with systems like Loxone and Comexio without special handling.
In UDP Mode the Path field is not available.
In UDP Mode the Param field is required.
In REST mode, an empty key field means that the corresponding report is not sent.
In UDP mode, an empty param field means that the corresponding report is not sent.

##### General

- **HA State Path**: Home Automation API path that returns the status of the HAR in response to a GET request. Used as an additional check to verify if the Home Automation system is reachable. Leave empty to disable status check.
- **Uptime Path**: URL path to report ESP system uptime to Home Automation (e.g. `/api/system/uptime`)
- **Uptime (Query/Param)**: Query to report the value (e.g. `?value=`)

- **FW Restart Reason Path**: URL path to report the firmware restart reason to Home Automation (e.g. `/api/system/fwrestart`)
- **FW Restart Reason (Query/Param)**: Query to report the value (e.g. `?value=`)

- **ESP Restart Reason Path**: URL path to report the ESP restart reason to Home Automation (e.g. `/api/system/esprestart`)
- **ESP Restart Reason (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Bridge Version Path**: URL path to report the Nuki Bridge firmware version (e.g. `/api/system/version`)
- **Bridge Version (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Bridge Build Path**: URL path to report the build number or date of the Nuki Bridge (e.g. `/api/system/build`)
- **Bridge Build (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Free Heap Path**: URL path to report available free heap memory (e.g. `/api/system/freeheap`)
- **Free Heap (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Wi-Fi RSSI Path**: URL path to report current Wi-Fi signal strength (RSSI) (e.g. `/api/system/wifi_rssi`)
- **Wi-Fi RSSI (Query/Param)**: Query to report the value (e.g. `?rssi=`)

- **BLE Address Path**: URL path to report the BLE MAC address of the Nuki device (e.g. `/api/system/ble_address`)
- **BLE Address (Query/Param)**: Query to report the value (e.g. `?addr=`)

- **BLE RSSI Path**: URL path to report the signal strength (RSSI) of the BLE connection (e.g. `/api/system/ble_rssi`)
- **BLE RSSI (Query/Param)**: Query to report the value (e.g. `?rssi=`)


##### Key Turner State

- **Lock State Path**: URL path to report the current lock state (e.g. `/api/lock/state`)
- **Lock State (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Lock 'N' Go State Path**: URL path to report if a Lock ’n’ Go action was triggered (e.g. `/api/lock/lockngo`)
- **Lock 'N' Go State (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Lock Trigger Path**: URL path to report the last trigger source (e.g. `/api/lock/trigger`)
- **Lock Trigger (Query/Param)**: Query to report the value (e.g. `?source=`)

- **Night Mode Path**: URL path to report whether night mode is active (e.g. `/api/lock/nightmode`)
- **Night Mode (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Process Status Path**: URL path to report the lock completion status (e.g. `/api/lock/completionstatus`)
- **Process Status (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Battery Critical Path**: URL path to report whether the lock battery is critical (e.g. `/api/lock/batterycritical`)
- **Battery Critical (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Battery Level Path**: URL path to report the battery level percentage (e.g. `/api/lock/batterylevel`)
- **Battery Level (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Battery Charging Path**: URL path to report if the battery is currently charging (e.g. `/api/lock/batterycharging`)
- **Battery Charging (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Door Sensor State Path**: URL path to report the door sensor state (e.g. `/api/lock/doorsensorstate`)
- **Door Sensor State (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Door Sensor Critical Path**: URL path to report door sensor error state (e.g. `/api/lock/doorsensorcritical`)
- **Door Sensor Critical (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Keypad Critical Path**: URL path to report keypad error state (e.g. `/api/lock/keypadcritical`)
- **Keypad Critical (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Remote Access State Path**: URL path to report the current remote access state (e.g. `/api/lock/remoteaccess`)
- **Remote Access State (Query/Param)**: Query to report the value (e.g. `?value=`)

- **BLE Strength Path**: URL path to report BLE signal strength (e.g. `/api/lock/blestrength`)
- **BLE Strength (Query/Param)**: Query to report the value (e.g. `?value=`)

##### Battery Report

- **Voltage Path**: URL path to report the current battery voltage (e.g. `/api/battery/voltage`)
- **Voltage (Query/Param)**: Query to report the value (e.g. `?v=`)

- **Drain Path**: URL path to report battery drain rate (e.g. `/api/battery/drain`)
- **Drain (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Max Turn Current Path**: URL path to report the peak current while turning the motor (e.g. `/api/battery/maxcurrent`)
- **Max Turn Current (Query/Param)**: Query to report the value (e.g. `?value=`)

- **Lock Distance Path**: URL path to report motor lock distance (e.g. `/api/battery/lockdistance`)
- **Lock Distance (Query/Param)**: Query to report the value (e.g. `?value=`)

---

### Nuki Configuration

#### Basic Nuki Configuration

- **Nuki Smartlock enabled**: Enable if you want Nuki Bridge to connect to a Nuki Lock (1.0-4.0)
- **New Nuki Bluetooth connection mode**: Enable to use the latest Nuki BLE connection mode (recommended). 
    > 📘: Disable if you have issues communicating with the lock

#### Advanced Nuki Configuration

- **Query interval lock state**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current lock state, default 1800.
- **Query interval configuration**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current configuration, default 3600.
- **Query interval battery**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current battery state, default 1800.
- **Query interval keypad**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current keypad state, default 1800.
  >📘: Only available when a Keypad is detected
- **Number of retries if command failed**: Set to a positive integer to define the amount of times the Nuki Bridge retries sending commands to the Nuki Lock when commands are not acknowledged by the device, default 3.
- **Delay between retries**: Set to the amount of milliseconds the Nuki Bridge waits between resending not acknowledged commands, default 100.
- **Restart if bluetooth beacons not received**: Set to a positive integer to restart the Nuki Bridge after the set amount of seconds has passed without receiving a bluetooth beacon from the Nuki device, set to -1 to disable, default 60. Because the bluetooth stack of the ESP32 can silently fail it is not recommended to disable this setting.
- **BLE transmit power in dB**: Set to a integer between -12 and 9 (ESP32) or -12 and 20 (All newer ESP32 variants) to set the Bluetooth transmit power, default 9.
- **Update Nuki Bridge and Lock time using NTP**: Enable to update the ESP32 time and Nuki Lock time every 12 hours using a NTP time server.
  >📘: Updating the Nuki device time requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN](#nuki-lock-pin)" below.
- **NTP server**: Set to the NTP server you want to use, defaults to "`pool.ntp.org`". If DHCP is used and NTP servers are provided using DHCP these will take precedence over the specified NTP server.

### Access Level Configuration

#### Nuki General Access Control
- **Modify Nuki Bridge configuration over REST API**: Allow changing Nuki Bridge settings using REST API.
  > 🚨: For security reasons, not all configurations can be changed via the REST API.

#### Nuki Lock Access Control
- **Enable or disable executing each available lock action for the Nuki Lock through REST API**

#### Nuki Lock Config Control
- **Enable or disable changing each available configuration setting for the Nuki Lock through REST API**
  > 📘: Changing configuration settings requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN](#nuki-lock-pin)" below.

### Credentials

#### Web Configurator Credentials

- **User**: Pick a username to enable HTTP authentication for the Web Configuration, Set to "#" to disable authentication.
- **Password/Retype password**: Pick a password to enable HTTP authentication for the Web Configuration.
- **HTTP Authentication type**: Select from Basic, Digest or Form based authentication. Digest authentication is more secure than Basic or Form based authentication, especially over unencrypted (HTTP) connections. Form based authentication works best with password managers. Note: Firefox seems to have issues with basic authentication.
- **Bypass authentication for reverse proxy with IP**: IP for which authentication is bypassed. Use in conjunction with a reverse proxy server with separate authentication.
- **Admin key**: Set a 32 character long alphanumeric string that can be used for some settings without needing to log in (for use with automated systems).
- **Session validity (in seconds)**: Session validity to use with form authentication when the "Remember me" checkbox is disabled, default 3600 seconds.
- **Session validity remember (in hours)**: Session validity to use with form authentication when the "Remember me" checkbox is enabled, default 720 hours.

#### Nuki Lock PIN

- **PIN Code**: Fill with the Nuki Security Code of the Nuki Lock. Required for functions that require the security code to be sent to the lock such as setting lock permissions/adding keypad codes, viewing the activity log or changing the Nuki device configuration. Set to "#" to remove the security code from the Nuki Bridge configuration.

#### Unpair Nuki Lock

- **Type [4 DIGIT CODE] to confirm unpair**: Set to the shown randomly generated code to unpair the Nuki Lock from the Nuki Bridge.

#### Factory reset Nuki Bridge

- **Type [4 DIGIT CODE] to confirm factory reset**: Set to the shown randomly generated code to reset all Nuki Bridge settings to default and unpair Nuki Lock. Optionally also reset Wi-Fi settings to default (and reopen the Wi-Fi configurator) by enabling the checkbox.

---

## Recommended LAN Setup

> 🔐 Among other things, the Nuki REST Bridge uses HTTP for communication. This means that data is transmitted in plain text within the local network, unlike HTTPS. However, the security risk can be considered minimal if the recommended precautions are followed and should not pose a problem for private users in a secured home network.


These settings are intended for environments where secure HTTPS is not available and devices communicate via plain HTTP (e.g., with legacy home automation systems or embedded APIs).

| Setting                                | Device / Layer                | Recommended Action                                                                 |
|----------------------------------------|-------------------------------|------------------------------------------------------------------------------------|
| **Prefer wired connections (LAN)**     | Network design                | Use Ethernet wherever possible for improved stability and security                |
|**Enable secure WiFi (WPA2/WPA3) if WLAN is used**      | Router / Access Point         | Use WPA3 if supported, otherwise WPA2 with a strong password (no open WiFi)       |
| **Use static IP addresses**            | Router / DHCP Server          | Assign fixed IPs for ESP32 devices and the Home Automation server                 |
| **Restrict internet access for ESP32** | Router / Firewall             | Block outbound internet for ESP32 devices (via IP/MAC filtering or ACLs)          |
| **Isolate ESP32 in its own VLAN**      | Managed Switch / Router       | Place ESP32 devices and the HA server in a dedicated VLAN                         |
| **Restrict inter-VLAN traffic**        | Router / Layer 3 Switch       | Only allow ESP32 ↔ HA server communication, block access to other devices         |
| **Assign dedicated VLAN ports**        | Managed Switch                | Use port-based VLANs if tagging is unsupported or complex                         |
| **Limit API permissions**              | Home Automation System        | Create a separate user with minimal privileges for each ESP32 device              |
| **Avoid hardcoding credentials in URL**| ESP32 firmware                | Use POST requests where possible; avoid GET with sensitive data in query strings  |
| **Use VPN for remote access**          | Router / Gateway              | Avoid exposing devices to the internet – connect remotely via VPN only            |
| **Set up a separate Guest LAN**        | Router / Access Point         | Create an isolated guest WiFi/LAN for visitors to prevent access to internal devices |
| **Monitor network traffic**            | Firewall / Network Monitor    | Log or alert on unusual traffic from/to ESP32 IPs                                 |

⚠️ **Note:** These steps are aimed at minimizing the risk when HTTP is the only available communication method. They do not replace proper encryption, but help to minimize the risk within a trusted LAN.
