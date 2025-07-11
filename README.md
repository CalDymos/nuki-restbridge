# Nuki Rest Bridge – Control Nuki Smart Locks via REST and ESP32

![Release](https://img.shields.io/badge/status--release-green)
[![Project Maintenance](https://img.shields.io/maintenance/yes/2025.svg)](https://github.com/CalDymos/nuki-restbridge 'GitHub Repository')
[![License](https://img.shields.io/github/license/CalDymos/nuki-restbridge.svg)](https://github.com/CalDymos/nuki-restbridge/blob/main/LICENSE 'License')

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
    - [Example: Sending HAR values to a **Loxone** Virtual Input](#example-sending-har-values-to-a-loxone-virtual-input)
      - [Sending String values (Virtual Text Input)](#sending-string-values-virtual-text-input)
      - [Sending Integer values (Virtual Input)](#sending-integer-values-virtual-input)
    - [Example: Sending Values to a **Comexio** Device (WebIO or API)](#example-sending-har-values-to-a-comexio-device-webio-or-api)
      - [Using the Comexio API](#using-the-comexio-api)
      - [Using Comexio Marker Variables](#using-comexio-marker-variables)
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
  - [Advanced Configuartion](#advanced-configuration)
- [REST API Enpoints](#rest-api-endpoints) 
  - [Authentication](#authentication)
  - [Bridge Control](#bridge-control)
  - [Nuki Lock](#nuki-lock)
    - [Lock Keypad Command (JSON)](#lock-keypad-command-json)
    - [Lock Config Action (JSON)](#lock-config-action-json)
    - [Lock Timecontrol Action (JSON)](#lock-timecontrol-action-json)
    - [Lock Authorization action (JSON)](#lock-authorization-action-json)
- [Recommended LAN Setup](#recommended-lan-setup)

---

## 📌 Overview

**Nuki REST Bridge** allows full local integration of Nuki Smart Locks via REST – no cloud, no MQTT broker required. It runs on an ESP32 and connects to Nuki devices over BLE while exposing a simple REST API.

> 🔗 **Fork Notice**: This project is a **disconnected fork** of [Nuki Hub](https://github.com/technyon/nuki_hub). It's not actively synchronized with upstream and has diverged significantly.

- Offers direct REST access for smart home platforms (e.g., Loxone Miniserver).
- Ideal for autonomous environments without MQTT broker or additional original NukiBridge.

---

## ✅ Supported Devices

### ESP32 Boards (Wi-Fi + BLE)

| ✅ Supported | ❌ Not Supported |
|-------------|------------------|
| All dual-core ESP32 boards with Wi-Fi + BLE supported by ESP-IDF 5.4.1 / Arduino Core 3.2.0 | ESP32-S2 (no BLE) |

---

### Nuki Devices

- Nuki Smart Lock 1.0 → 4.0
- Nuki Keypad 1.0 -> 2.0

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

Avoid low-RAM models if using HAR (Home Automation Reporting) or logging features extensively.

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

Flash the firmware to your ESP32 board by compiling it via PlatformIO.

---

### Initial Network Setup

1. Power on the ESP32.
2. Connect to the new Wi-Fi AP: `NukiRestBridge`
   - Password: `NukiBridgeESP32`
3. Open browser: [http://192.168.4.1](http://192.168.4.1)
4. Select and connect to your home Wi-Fi.

ESP32 will then connect to the selected network.

---

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

---

### Network Configuration

#### General Settings

- **Host name**: Set the hostname for the Nuki Rest Bridge ESP (needs to be unique)
- **Network hardware**: "Wi-Fi only" by default, set to ethernet if available
- **RSSI send interval**: Set to a positive integer to specify the number of seconds between sending the current Wi-Fi RSSI; set to -1 to disable, default value 60
  > 📘 **Note:** Requires Home Automation Reporting to be enabled
- **Restart on disconnect**: Enable to restart the Nuki Rest Bridge when disconnected from the network.
- **Find Wi-Fi AP with strongest signal**: Uses the AP with the strongest signal for the connection via Wi-Fi

---

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
  >📘 **Note:** In UDP mode, the fields marked as Path are ignored. Only the Param values are used. In REST mode, both Path and Query fields are required.
- **REST Request Method**: Select Methode for Rest Request GET/POST.
  >📘 **Note:** only available if REST Mode is selected
- **User**: Optional username for authenticating with the Home Automation system. Use `#` to disable authentication.
  >📘 **Note:** only available if REST Mode is selected
- **Password**: Password corresponding to the username. Use `#` to disable authentication.
  >📘 **Note:** only available if REST Mode is selected

---

#### Report Settings

> ⚠️ **Note:**  
> In **REST mode**, the **Query** field may optionally include a full query string, such as `?action=set&ext=Extension1&io=Q1&value=`.  
> The actual value will be appended automatically during transmission. This allows compatibility with systems like **Loxone** and **Comexio** without special handling.  
>
> In **UDP mode**, the **Path** field is not available — instead, the **Param** field is **required**.  
>
> In **REST mode**, leaving the **Key** field empty disables sending the corresponding report.  
> In **UDP mode**, leaving the **Param** field empty does the same.

---

##### General

- **HA State Path**: Home Automation API path that returns the status of the HAR in response to a GET request. Used as an additional check to verify if the Home Automation system is reachable. Leave empty to disable status check.

- **Uptime Path**: URL path for the report to Home Automation on the operating time of the ESP system since the last start. (e.g. `api/system/uptime`)
- **Uptime (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Seconds as Integer (e.g. 27546)

- **FW Restart Reason Path**: URL path to report the firmware restart reason to Home Automation (e.g. `api/system/fwrestart`)
- **FW Restart Reason (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Reason as String (e.g "NetworkTimeoutWatchdog")

- **ESP Restart Reason Path**: URL path to report the ESP restart reason to Home Automation (e.g. `api/system/esprestart`)
- **ESP Restart Reason (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Reason as String (e.g "ESP_RST_PANIC: Software reset due to exception/panic.")

- **Bridge Version Path**: URL path to report the Nuki Bridge firmware version (e.g. `api/system/version`)
- **Bridge Version (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Version as String (e.g. "0.7.3")

- **Bridge Build Path**: URL path to report the build number or date of the Nuki Bridge (e.g. `api/system/build`)
- **Bridge Build (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Number/Id as String (e.g. "main" )

- **Free Heap Path**: URL path to report available free heap memory(e.g. `api/system/freeheap`)
- **Free Heap (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Bytes as integer (e.g. 77000)

- **Wi-Fi RSSI Path**: URL path to report current Wi-Fi signal strength (RSSI) (e.g. `api/system/wifi_rssi`)
- **Wi-Fi RSSI (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = dBm as Integer (e.g. -15)

- **BLE Address Path**: URL path to report the BLE MAC address of the Nuki device (e.g. `api/system/ble_address`)
- **BLE Address (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = MAC as String (e.g. "F1:D2:34:AB:12:CD")

- **BLE RSSI Path**: URL path to report the signal strength (RSSI) of the BLE connection (e.g. `api/system/ble_rssi`)
- **BLE RSSI (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = dBm as Integer (e.g. -40)

---

##### Key Turner State

- **Lock State Path**: URL path to report the current lock state (e.g. `api/lock/state`)
- **Lock State (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = State as Integer:

  | Value | Meaning         |
  |-------|------------------|
  | 0     | Uncalibrated     |
  | 1     | Locked           |
  | 2     | Unlocking        |
  | 3     | Unlocked         |
  | 4     | Locking          |
  | 5     | Unlatched        |
  | 6     | Unlocked (Lock ’n’ Go) |
  | 7     | Unlatching       |
  | 252   | Calibration      |
  | 253   | Boot Run         |
  | 254   | Motor Blocked    |
  | 255   | Undefined        |

- **Lock 'N' Go State Path**: URL path to report if a Lock ’n’ Go action was triggered (e.g. `api/lock/lockngo`)
- **Lock 'N' Go State (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Remaining seconds until Lock 'N' Go is executed as Integer.  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Value is `0` if no Lock 'N' Go action is active.

- **Lock Trigger Path**: URL path to report the last trigger source (e.g. `api/lock/trigger`)
- **Lock Trigger (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = trigger source as Integer:

  | Value | Meaning   |
  |-------|-----------|
  | 0     | System    |
  | 1     | Manual    |
  | 2     | Button    |
  | 3     | Automatic |
  | 6     | AutoLock  |
  | 171   | HomeKit   |
  | 172   | MQTT      |
  | 255   | Undefined |

- **Night Mode Path**: URL path to report whether night mode is active (e.g. `api/lock/nightmode`)
- **Night Mode (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Boolean as Integer (1 = active, 0 = inactive)

- **Process Status Path**: URL path to report the lock completion status (e.g. `api/lock/completionstatus`)
- **Process Status (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Status As Integer:

  | Value | Meaning              |
  |-------|-----------------------|
  | 0     | Success               |
  | 1     | Motor Blocked         |
  | 2     | Canceled              |
  | 3     | Too Recent            |
  | 4     | Busy                  |
  | 5     | Low Motor Voltage     |
  | 6     | Clutch Failure        |
  | 7     | Motor Power Failure   |
  | 8     | Incomplete Failure    |
  | 11    | Failure               |
  | 224   | Invalid Code          |
  | 254   | Other Error           |
  | 255   | Unknown               |

- **Battery Critical Path**: URL path to report whether the lock battery is critical (e.g. `api/lock/batterycritical`)
- **Battery Critical (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Boolean as Integer (1 = battery low, 0 = OK)

- **Battery Level Path**: URL path to report the battery level percentage (e.g. `api/lock/batterylevel`)
- **Battery Level (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = percent as Integer (e.g. 50)

- **Battery Charging Path**: URL path to report if the battery is currently charging (e.g. `api/lock/batterycharging`)
- **Battery Charging (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Boolean as Integer (1 = charging, 0 = not charging)

- **Door Sensor State Path**: URL path to report the door sensor state (e.g. `api/lock/doorsensorstate`)
- **Door Sensor State (Query/Param)**: Query to report the value (e.g. `?value=`)
Transmitted Value = State as Integer:

  | Value | Meaning             |
  |-------|----------------------|
  | 0     | Unavailable          |
  | 1     | Deactivated          |
  | 2     | Door Closed          |
  | 3     | Door Opened          |
  | 4     | Door State Unknown   |
  | 5     | Calibrating          |
  | 16    | Uncalibrated         |
  | 240   | Tampered             |
  | 255   | Unknown              |

- **Door Sensor Critical Path**: URL path to report whether door sensor battery is critical (e.g. `api/lock/doorsensorcritical`)
- **Door Sensor Critical (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Boolean as Integer (1 = battery low, 0 = OK)

- **Keypad Critical Path**: URL path to report whether keypad battery is critical (e.g. `api/lock/keypadcritical`)
- **Keypad Critical (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Boolean as Integer (1 = battery low, 0 = OK)

> 📘 Only supported by Smart Lock 4th Generation and Ultra.
> - **Remote Access State Path**: URL path to report the current remote access state (e.g. `api/lock/remoteaccess`)
> - **Remote Access State (Query/Param)**: Query to report the value (e.g. `?value=`).  
> Transmitted Value = Bitmask as Integer. Each bit represents a specific remote access state:
> 
>  | Bit | Mask | Meaning                                |
>  |-----|------|----------------------------------------|
>  | 0   | 1    | Remote access enabled                  |
>  | 1   | 2    | Bridge paired                          |
>  | 2   | 4    | SSE connected via Wi-Fi                |
>  | 3   | 8    | SSE connection established             |
>  | 4   | 16   | SSE connected via Thread               |
>  | 5   | 32   | Thread SSE uplink enabled by user      |
>  | 6   | 64   | NAT64 available via Thread             |
>
>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;To extract specific state use bitwise operations, e.g. `value & 2` to check if the Bridge is paired.

> 📘 Only supported by Smart Lock 4th Generation and Ultra.
> - **BLE Strength Path**: URL path to report BLE signal strength between Nuki  Bridge and Lock (e.g. `api/lock/blestrength`)
> - **BLE Strength (Query/Param)**: Query to report the value (e.g. `?value=`).  
  Transmitted Value = RSSI in dBm or status code as Integer.
>
>  | Value     | Meaning                                                        |
>  |-----------|----------------------------------------------------------------|
>  | `< 0`     | Raw RSSI value in dBm (e.g. `-75`)                             |
>  | `0`       | Invalid RSSI value                                             |
>  | `1`       | Not supported (only Smart Lock 4.0 and Ultra return RSSI)     |

---

##### Battery Report

- **Voltage Path**: URL path to report the current battery voltage (e.g. `api/battery/voltage`)
- **Voltage (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Voltage in mV as Integer

- **Drain Path**: URL path to report battery drain rate of last lock action (e.g. `api/battery/drain`)
- **Drain (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = drain in mWs as Integer

- **Max Turn Current Path**: URL path to report the peak current while turning the motor (e.g. `api/battery/maxcurrent`)
- **Max Turn Current (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Peak current in mA as Integer

- **Lock Distance Path**: URL path to report motor lock distance (e.g. `api/battery/lockdistance`)
- **Lock Distance (Query/Param)**: Query to report the value (e.g. `?value=`).  
Transmitted Value = Distance in degrees as Integer

---

#### Example: Sending HAR values to a **Loxone** Virtual Input

You can use **LOXONE** ***Virtual Inputs*** or ***Virtual Text Inputs*** to receive HAR status updates directly from the Nuki REST Bridge via **REST**.

##### Sending String values (Virtual Text Input)

Loxone expects the following URL format:  
`http://<User>:<Password>@<MiniserverIP>:<Port>/dev/sps/io/<ConnectionName>/<Text>`

- **Connection Name**: Typically starts with `VTI`, e.g. `VTI13`
- **Text**: Any string (e.g. `Watchdog`, `Boot`, etc.)

**HAR Configuration:**

- **Path**: `dev/sps/io`
- **Query / Param**: `VTI13/`

> 📘 The actual value (e.g. `Watchdog`) will be appended automatically to the end of the query.

##### Sending Integer values (Virtual Input)

Loxone expects the following URL format:
`http://<User>:<Password>@<MiniserverIP>:<Port>/dev/sps/io/<ConnectionName>/<IntValue>`

- **Connection Name**: Typically starts with `VI`, e.g. `VI5`
- **IntValue**: A numeric value like `0`, `1`, `75`, etc.

**HAR Configuration:**

- **Path**: `dev/sps/io`
- **Query / Param**: `VI5/`

> 📘 The numeric value (e.g. `1`, `255`, etc.) is appended automatically.

#### Example: Sending HAR Values to a **Comexio** Device (WebIO or API)

##### Using the Comexio API

Comexio supports standard HTTP GET/POST requests with clear parameterized URLs.

- **Example URL** (Integer or Boolean values):  
  `http://<User>:<Password>@<ComexioIP>/api/?action=set&ext=<ExtensionName>&io=<IO_ID>&value=<VALUE>`

- **REST Mode Configuration:**
  - **Path** = `api`
  - **Query** = `?action=set&ext=Extension1&io=Q1&value=`

> 💡 **Info:** Comexio expects `Qx` (Q1, Q2, etc.) for outputs and `Ix` (I1, I2, etc.) for inputs.  
> The `value` can be `0` or `1` for digital outputs or any number for analog values.

---

##### Using Comexio Marker Variables

You can set markers via the API as follows:

- **Example URL** (Analog value):  
  `http://<ComexioIP>/api/?action=set&marker=M1&value=12`

- **REST Mode Configuration:**
  - **Path** = `api`
  - **Query** = `?action=set&marker=M1&value=`

---

### Nuki Configuration

#### Basic Nuki Configuration

- **Nuki Smartlock enabled**: Enable if you want Nuki Bridge to connect to a Nuki Lock (1.0-4.0)
- **New Nuki Bluetooth connection mode**: Enable to use the latest Nuki BLE connection mode (recommended). 
    > 📘 **Note:** Disable if you have issues communicating with the lock

---

#### Advanced Nuki Configuration

- **Query interval lock state**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current lock state, default 1800.
- **Query interval configuration**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current configuration, default 3600.
- **Query interval battery**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current battery state, default 1800.
- **Query interval keypad**: Set to a positive integer to set the maximum amount of seconds between actively querying the Nuki device for the current keypad state, default 1800.
  >📘 **Note:** Only available when a Keypad is detected
- **Number of retries if command failed**: Set to a positive integer to define the amount of times the Nuki Bridge retries sending commands to the Nuki Lock when commands are not acknowledged by the device, default 3.
- **Delay between retries**: Set to the amount of milliseconds the Nuki Bridge waits between resending not acknowledged commands, default 100.
- **Restart if bluetooth beacons not received**: Set to a positive integer to restart the Nuki Bridge after the set amount of seconds has passed without receiving a bluetooth beacon from the Nuki device, set to -1 to disable, default 60. Because the bluetooth stack of the ESP32 can silently fail it is not recommended to disable this setting.
- **BLE transmit power in dB**: Set to a integer between -12 and 9 (ESP32) or -12 and 20 (All newer ESP32 variants) to set the Bluetooth transmit power, default 9.
- **Update Nuki Bridge and Lock time using NTP**: Enable to update the ESP32 time and Nuki Lock time every 12 hours using a NTP time server.
  >📘 **Note:** Updating the Nuki device time requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN](#nuki-lock-pin)" below.
- **NTP server**: Set to the NTP server you want to use, defaults to "`pool.ntp.org`". If DHCP is used and NTP servers are provided using DHCP these will take precedence over the specified NTP server.
- **Enable encryption**: Enable Keypad Code Encryption (REST API & HAR), see "[Keypad Code Encryption](#keypad-code-encryption).
- **Encryption multiplier**: Integer used to scramble the input. Must be coprime to the modulus. Should be > 1 and < (modulus - 1)
- **Encryption offset**: Fixed number added to the scrambled value. Should be > 0 and < (modulus - 1)
- **Encryption modulus**: Final result is calculated modulo this value. Should be > 100000 and < 2147483647

---

### Access Level Configuration

#### Nuki General Access Control
- **Modify Nuki Bridge configuration over REST API**: Allow changing Nuki Bridge settings using REST API.
  > 🚨 **Important:** For security reasons, not all configurations can be changed via the REST API.

---

#### Nuki Lock Access Control
- **Enable or disable executing each available lock action for the Nuki Lock through REST API**

---

#### Nuki Lock Config Control
- **Enable or disable changing each available configuration setting for the Nuki Lock through REST API**
  > 📘 **Note:** Changing configuration settings requires the Nuki security code / PIN to be set, see "[Nuki Lock PIN](#nuki-lock-pin)" below.

---

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

---

#### Unpair Nuki Lock

- **Type [4 DIGIT CODE] to confirm unpair**: Set to the shown randomly generated code to unpair the Nuki Lock from the Nuki Bridge.

---

#### Factory reset Nuki Bridge

- **Type [4 DIGIT CODE] to confirm factory reset**: Set to the shown randomly generated code to reset all Nuki Hub settings to default and unpair Nuki Lock and/or Opener. Optionally also reset Wi-Fi settings to default (and reopen the Wi-Fi configurator) by enabling the checkbox.

---

### Advanced Configuration

The advanced configuration menu is not reachable from the main menu of the web configurator by default.<br>
You can reach the menu directly by browsing to http://NUKIHUBIP/get?page=advanced or enable showing it in the main menu by browsing to http://NUKIHUBIP/get?page=debugon once (http://NUKIHUBIP/get?page=debugoff to disable).

>⚠️ **Warning:** that the following options can break Nuki Hub and cause bootloops that will require you to erase your ESP and reflash following the instructions for first-time flashing.

- **Enable Bootloop prevention**: Enable to reset the following stack size and max entry settings to default if Nuki Hub detects a bootloop.
- **Char buffer size (min 4096, max 65536)**: Set the character buffer size, needs to be enlarged to support large amounts of auth/keypad/timecontrol/authorization entries. Default 4096.
- **Task size Network (min 6144, max 65536)**: Set the Network task stack size, needs to be enlarged to support large amounts of auth/keypad/timecontrol/authorization entries. Default 6144.
- **Task size Nuki (min 6144, max 65536)**: Set the Nuki task stack size. Default 6144.
- **Max auth log entries (min 1, max 100)**: The maximum amount of log entries that will be requested from the lock, default 5.
- **Max keypad entries (min 1, max 200)**: The maximum amount of keypad codes that will be requested from the lock, default 10.
- **Max timecontrol entries (min 1, max 100)**: The maximum amount of timecontrol entries that will be requested from the lock, default 10.
- **Max authorization entries (min 1, max 100)**: The maximum amount of authorization entries that will be requested from the lock, default 10.
- **Show Pairing secrets on Info page**: Enable to show the pairing secrets on the info page. Will be disabled on reboot.
- **Manually set lock pairing data**: Enable to save the pairing data fields and manually set pairing info for the lock.
- **Force Lock ID to current ID**: Enable to force the current Lock ID, irrespective of the config received from the lock.
- **Force Lock Keypad connected**: Enable to force Nuki Hub to function as if a keypad was connected, irrespective of the config received from the lock.
- **Force Lock Doorsensor connected**: Enable to force Nuki Hub to function as if a doorsensor was connected, irrespective of the config received from the lock.
- **Enable Nuki connect debug logging**: Enable to log debug information regarding Nuki BLE connection to Home Automation and/or Serial.
- **Enable Nuki communication debug logging**: Enable to log debug information regarding Nuki BLE communication to Home Automation and/or Serial.
- **Enable Nuki readable data debug logging**: Enable to log human readable debug information regarding Nuki BLE to Home Automation and/or Serial.
- **Enable Nuki hex data debug logging**: Enable to log hex debug information regarding Nuki BLE to Home Automation and/or Serial.
- **Enable Nuki command debug logging**: Enable to log debug information regarding Nuki BLE commands to Home Automation and/or Serial.
- **Send free heap to Home Automation**: Enable to send free heap to Home Automation.

---

## REST API Endpoints

All REST API endpoints are accessible via the configured port (default: `8080`).  

> 🧩 Format:  
> `http://<bridge-ip>:<port>/<endpoint>?token=<api-token>&<parameter>=<value>&...`

### Authentication

Every request must include the access token as HTTP query parameter: `?token=<api-token>`

> ⚠️ The only exception is the `/shutdown` endpoint, which does **not** require a token.

---

### Bridge Control

| Endpoint           | Paramter | Value | Method | Description                                   |
|--------------------|----------|-------|--------|-----------------------------------------------|
| `bridge/disableApi`      |    -     |   -   |   GET  | deactivates the REST API <br> (if the API is deactivated, it can only be activated via the web configurator)|
| `bridge/shutdown`        |    -     |   -   |   GET  | Powers down the ESP32 (no token required).    |
| `bridge/reboot`          |    -     |   -   |   GET  | Restarts the ESP32 immediately.               |
| `bridge/enableWebServer` |   val    |  0/1  |   GET  | 0 = Deactivate the web configurator / 1 = Activate the web configurator <br> (reboot is performed after the action)|
---

### Nuki Lock

| Endpoint                      | Parameter | Value        | Method | Description |
|-------------------------------|-----------|--------------|--------|-------------|
| `lock/action`                | "unlock", "lock", "unlatch", "lockNgo", "lockNgoUnlatch", "fullLock", "fobAction1", "fobAction2", "fobAction3" |      1       | GET | Executes a lock action.|
| `lock/keypad/command`         |   JSON<br>(see [Lock Keypad command](#lock-keypad-command-json))| various<br>(see [Lock Keypad command](#lock-keypad-command-json))|  GET| Submits a complete keypad command as json string.|
| `lock/query/config`               | val       |  1           | GET    | Requests configuration state from the lock. (Triggers a HAR report for this query) |
| `lock/query/lockstate`            | val       |  1           | GET    | Requests current lock state from lock. (Triggers a HAR report for this query)|
| `lock/query/keypad`               | val       |  1           | GET    | Requests keypad status / entries from lock. (Triggers a HAR report for this query)|
| `lock/query/battery`              | val       |  1           | GET    | Requests battery status from lock. (Triggers a HAR report for this query)|
| `lock/config/action`              | JSON<br>(see [Lock config action](#lock-config-action-json))      | various<br>(see [Lock config action](#lock-config-action-json))     | GET    | Updates lock configuration values.<br> See [Nuki Bluetooh API](https://developer.nuki.io/t/bluetooth-api/27) for more information on the available settings.<br> Changing settings has to enabled first in the configuration portal.<br> Check the settings you want to be able to change under "Nuki Lock Config Control" in "Access Level Configuration" and save the configuration.|
| `lock/timecontrol/action`         | JSON      | various      | GET    | Sends time control schedule commands.<br>See [Lock Timecontrol Action (JSON)](#lock-timecontrol-action-json) |
| `lock/authorization/action`       | JSON      | various      | GET    | Sends authorization updates (e.g. new users).<br>See [Lock Authorization action (JSON)](#lock-authorization-action-json) |


#### Lock Keypad Command (JSON)

If a keypad is connected to the lock, keypad codes can be added, updated and removed. This has to enabled first in the configuration portal. Check "Add, modify and delete keypad codes" under "Access Level Configuration" and save the configuration.

To change the Nuki Lock keypad settings via JSON string, the JSON-formatted string must contain the following nodes.

| Node             | Delete   | Add      | Update   |  Check   | Usage                                                                                                            | Possible values                        |
|------------------|----------|----------|----------|----------|------------------------------------------------------------------------------------------------------------------|----------------------------------------|
| action           | Required | Required | Required | Required | The action to execute                                                                                            | "delete", "add", "update"     |
| codeId           | Required | Not used | Required | Required | The code ID of the existing code to delete or update found in Web Configurator                                                            | Integer                                |
| code             | Not used | Required | Optional | Required | The code to create or update                                                                       | 6-digit Integer without zero's, can't start with "12"|
| enabled          | Not used | Not used | Optional | Not used | Enable or disable the code, always enabled on add                                                                | 1 = enabled, 0 = disabled              |
| name             | Not used | Required | Optional | Not used | The name of the code to create or update                                                                         | String, max 20 chars                   |


Examples:
- Delete: `{ "action": "delete", "codeId": "1234" }`
- Add: `{ "action": "add", "code": "589472", "name": "Test", "enabled": "0" }`
- Update: `{ "action": "update", "codeId": "1234", "enabled": "1", "name": "Test" }`

The result of the last keypad change action will be returned.<br>

##### Keypad Code Encryption

The Nuki REST Bridge supports encryption of keypad PIN codes to allow safe transmission over REST.

You can configure the encryption logic under: [Advanced Nuki Configuration](#advanced-nuki-configuration)

The following formulas are used for

- **Encryption:**  
  `encrypted = (code * multiplier + offset) % modulus`

- **Decryption:**  
  `decrypted = ((encrypted + modulus - offset) * inverseMultiplier) % modulus`

> ℹ️ `inverseMultiplier` means the **modular inverse** of the multiplier modulo the modulus.

---

**example for the values**:
- multiplier = 73
- offset = 12345
- modulus = 1000000
- inverse multiplier = 410959 (is calculated automatically)
- kepad Code = 123456

```text
encrypted = ((123456 * 73 + 12345) % 1000000) + 1000000 = 1929553
decrypted = ((1929553 - 12345) * 410959) % 1000000 = 123456
```

---

#### Lock Config Action (JSON)

| Setting                                 | Usage                                                                                            | Possible values                                                   | Example                            |
|-----------------------------------------|--------------------------------------------------------------------------------------------------|-------------------------------------------------------------------|------------------------------------|
| name                                    | The name of the Smart Lock.                                                                      | Alphanumeric string, max length 32 chars                          |`{ "name": "Frontdoor" }`           |
| latitude                                | The latitude of the Smart Locks geoposition.                                                     | Float                                                             |`{ "latitude": "48.858093" }`       |
| longitude                               | The longitude of the Smart Locks geoposition                                                     | Float                                                             |`{ "longitude": "2.294694" }`       |
| autoUnlatch                             | Whether or not the door shall be unlatched by manually operating a door handle from the outside. | 1 = enabled, 0 = disabled                                         |`{ "autoUnlatch": "1" }`            |
| pairingEnabled                          | Whether or not activating the pairing mode via button should be enabled.                         | 1 = enabled, 0 = disabled                                         |`{ "pairingEnabled": "0" }`         |
| buttonEnabled                           | Whether or not the button should be enabled.                                                     | 1 = enabled, 0 = disabled                                         |`{ "buttonEnabled": "1" }`          |
| ledEnabled                              | Whether or not the flashing LED should be enabled to signal an unlocked door.                    | 1 = enabled, 0 = disabled                                         |`{ "ledEnabled": "1" }`             |
| ledBrightness                           | The LED brightness level                                                                         | 0 = off, …, 5 = max                                               |`{ "ledBrightness": "2" }`          |
| timeZoneOffset                          | The timezone offset (UTC) in minutes                                                             | Integer between 0 and 60                                          |`{ "timeZoneOffset": "0" }`         |
| dstMode                                 | The desired daylight saving time mode.                                                           | 0 = disabled, 1 = European                                        |`{ "dstMode": "0" }`                |
| fobAction1                              | The desired action, if a Nuki Fob is pressed once.                                               | "No Action", "Unlock", "Lock", "Lock n Go", "Intelligent"         |`{ "fobAction1": "Lock n Go" }`     |
| fobAction2                              | The desired action, if a Nuki Fob is pressed twice.                                              | "No Action", "Unlock", "Lock", "Lock n Go", "Intelligent"         |`{ "fobAction2": "Intelligent" }`   |
| fobAction3                              | The desired action, if a Nuki Fob is pressed three times.                                        | "No Action", "Unlock", "Lock", "Lock n Go", "Intelligent"         |`{ "fobAction3": "Unlock" }`        |
| singleLock                              | Whether only a single lock or double lock should be performed                                    | 0 = double lock, 1 = single lock                                  |`{ "singleLock": "0" }`             |
| advertisingMode                         | The desired advertising mode.                                                                    | "Automatic", "Normal", "Slow", "Slowest"                          |`{ "advertisingMode": "Normal" }`   |
| timeZone                                | The current timezone or "None" if timezones are not supported                                    | "None" or one of the timezones from [Nuki Bluetooh API](https://developer.nuki.io/t/bluetooth-api/27)                                                                                                                                                                              |`{ "timeZone": "Europe/Berlin" }`   |
| unlockedPositionOffsetDegrees           | Offset that alters the unlocked position in degrees.                                             | Integer between -90 and 180                              |`{ "unlockedPositionOffsetDegrees": "-90" }` |
| lockedPositionOffsetDegrees             | Offset that alters the locked position in degrees.                                               | Integer between -180 and 90                                 |`{ "lockedPositionOffsetDegrees": "80" }` |
| singleLockedPositionOffsetDegrees       | Offset that alters the single locked position in degrees.                                        | Integer between -180 and 180                         |`{ "singleLockedPositionOffsetDegrees": "120" }` |
| unlockedToLockedTransitionOffsetDegrees | Offset that alters the position where transition from unlocked to locked happens in degrees.     | Integer between -180 and 180                   |`{ "unlockedToLockedTransitionOffsetDegrees": "180" }` |
| lockNgoTimeout                          | Timeout for lock ‘n’ go in seconds                                                               | Integer between 5 and 60                                          |`{ "lockNgoTimeout": "60" }`        |
| singleButtonPressAction                 | The desired action, if the button is pressed once.                  | "No Action", "Intelligent", "Unlock", "Lock", "Unlatch", "Lock n Go", "Show Status"   |`{ "singleButtonPressAction": "Lock n Go" }` |
| doubleButtonPressAction                 | The desired action, if the button is pressed twice.                 | "No Action", "Intelligent", "Unlock", "Lock", "Unlatch", "Lock n Go", "Show Status" |`{ "doubleButtonPressAction": "Show Status" }` |
| detachedCylinder                        | Wheter the inner side of the used cylinder is detached from the outer side.                      | 0 = not detached, 1 = detached                                    |`{ "detachedCylinder": "1" }`       |
| batteryType                             | The type of the batteries present in the smart lock.                                             | "Alkali", "Accumulators", "Lithium"                               |`{ "batteryType": "Accumulators" }` |
| automaticBatteryTypeDetection           | Whether the automatic detection of the battery type is enabled.                                  | 1 = enabled, 0 = disabled                          |`{ "automaticBatteryTypeDetection": "Lock n Go" }` |
| unlatchDuration                         | Duration in seconds for holding the latch in unlatched position.                                 | Integer between 1 and 30                                          |`{ "unlatchDuration": "3" }`        |
| autoLockTimeOut                         | Seconds until the smart lock relocks itself after it has been unlocked.                          | Integer between 30 and 1800                                       |`{ "autoLockTimeOut": "60" }`       |
| autoUnLockDisabled                      | Whether auto unlock should be disabled in general.                                               | 1 = auto unlock disabled, 0 = auto unlock enabled                 |`{ "autoUnLockDisabled": "1" }`     |
| nightModeEnabled                        | Whether nightmode is enabled.                                                                    | 1 = enabled, 0 = disabled                                         |`{ "nightModeEnabled": "1" }`       |
| nightModeStartTime                      | Start time for nightmode if enabled.                                                             | Time in "HH:MM" format                                            |`{ "nightModeStartTime": "22:00" }` |
| nightModeEndTime                        | End time for nightmode if enabled.                                                               | Time in "HH:MM" format                                            |`{ "nightModeEndTime": "07:00" }`   |
| nightModeAutoLockEnabled                | Whether auto lock should be enabled during nightmode.                                            | 1 = enabled, 0 = disabled                                        |`{ "nightModeAutoLockEnabled": "1" }`|
| nightModeAutoUnlockDisabled             | Whether auto unlock should be disabled during nightmode.                                         | 1 = auto unlock disabled, 0 = auto unlock enabled             |`{ "nightModeAutoUnlockDisabled": "1" }`|
| nightModeImmediateLockOnStart           | Whether the door should be immediately locked on nightmode start.                                | 1 = enabled, 0 = disabled                                   |`{ "nightModeImmediateLockOnStart": "1" }`|
| autoLockEnabled                         | Whether auto lock is enabled.                                                                    | 1 = enabled, 0 = disabled                                         |`{ "autoLockEnabled": "1" }`        |
| immediateAutoLockEnabled                | Whether auto lock should be performed immediately after the door has been closed.                | 1 = enabled, 0 = disabled                                        |`{ "immediateAutoLockEnabled": "1" }`|
| autoUpdateEnabled                       | Whether automatic firmware updates should be enabled.                                            | 1 = enabled, 0 = disabled                                         |`{ "autoUpdateEnabled": "1" }`      |
| motorSpeed                              | The desired motor speed (Ultra/5th gen Pro only)                                                 | "Standard", "Insane", "Gentle"                                    |`{ "motorSpeed": "Standard" }`      |
| enableSlowSpeedDuringNightMode          | Whether the slow speed should be applied during Night Mode (Ultra/5th gen Pro only)              | 1 = enabled, 0 = disabled                            |`{ "enableSlowSpeedDuringNightMode": "1" }`      |
| rebootNuki                              | Reboot the Nuki device immediately                                                               | 1 = reboot nuki                                                   |`{ "rebootNuki": "1" }`             |

#### Lock Timecontrol Action (JSON)

Timecontrol entries can be added, updated and removed. This has to enabled first in the configuration portal. Check "Add, modify and delete timecontrol entries" under "Access Level Configuration" and save the configuration.

To change Nuki Lock timecontrol settings send to `lock/timecontrol/actionJson` a JSON formatted string containing the following nodes.

| Node             | Delete   | Add      | Update   | Usage                                                                                    | Possible values                                                |
|------------------|----------|----------|----------|------------------------------------------------------------------------------------------|----------------------------------------------------------------|
| action           | Required | Required | Required | The action to execute                                                                    | "delete", "add", "update"                                      |
| entryId          | Required | Not used | Required | The entry ID of the existing entry to delete or update                                   | Integer                                                        |
| enabled          | Not used | Not used | Optional | Enable or disable the entry, always enabled on add                                       | 1 = enabled, 0 = disabled                                      |
| weekdays         | Not used | Optional | Optional | Weekdays on which the chosen lock action should be exectued (requires enabled = 1)       | Array of days: "mon", "tue", "wed", "thu" , "fri" "sat", "sun" |
| time             | Not used | Required | Optional | The time on which the chosen lock action should be executed (requires enabled = 1)       | "HH:MM"                                                        |
| lockAction       | Not used | Required | Optional | The lock action that should be executed on the chosen weekdays at the chosen time (requires enabled = 1) | For the Nuki lock: "Unlock", "Lock", "Unlatch", "LockNgo", "LockNgoUnlatch", "FullLock". For the Nuki Opener: "ActivateRTO", "DeactivateRTO", "ElectricStrikeActuation", "ActivateCM", "DeactivateCM |

Examples:
- Delete: `{ "action": "delete", "entryId": "1234" }`
- Add: `{ "action": "add", "weekdays": [ "wed", "thu", "fri" ], "time": "08:00", "lockAction": "Unlock" }`
- Update: `{ "action": "update", "entryId": "1234", "enabled": "1", "weekdays": [ "mon", "tue", "sat", "sun" ], "time": "08:00", "lockAction": "Lock" }`

#### Lock Authorization action (JSON)

Authorization entries can be updated and removed. This has to enabled first in the configuration portal. Check "Modify and delete authorization entries" under "Access Level Configuration" and save the configuration.
It is currently not (yet) possible to add authorization entries this way.

To change Nuki Lock authorization settings send to `lock/authorization/action` a JSON formatted string containing the following nodes.

| Node             | Delete   | Add      | Update   | Usage                                                                                                            | Possible values                        |
|------------------|----------|----------|----------|------------------------------------------------------------------------------------------------------------------|----------------------------------------|
| action           | Required | Required | Required | The action to execute                                                                                            | "delete", "add", "update"              |
| authId           | Required | Not used | Required | The auth ID of the existing entry to delete or update                                                            | Integer                                |
| enabled          | Not used | Not used | Optional | Enable or disable the authorization, always enabled on add                                                       | 1 = enabled, 0 = disabled              |
| name             | Not used | Required | Optional | The name of the authorization to create or update                                                                | String, max 20 chars                   |
| remoteAllowed    | Not used | Optional | Optional | If this authorization is allowed remote access, requires enabled = 1                                             | 1 = enabled, 0 = disabled              |
| timeLimited      | Not used | Optional | Optional | If this authorization is restricted to access only at certain times, requires enabled = 1                        | 1 = enabled, 0 = disabled              |
| allowedFrom      | Not used | Optional | Optional | The start timestamp from which access should be allowed (requires enabled = 1 and timeLimited = 1)               | "YYYY-MM-DD HH:MM:SS"                  |
| allowedUntil     | Not used | Optional | Optional | The end timestamp until access should be allowed (requires enabled = 1 and timeLimited = 1)                      | "YYYY-MM-DD HH:MM:SS"                  |
| allowedWeekdays  | Not used | Optional | Optional | Weekdays on which access should be allowed (requires enabled = 1 and timeLimited = 1)     | Array of days: "mon", "tue", "wed", "thu" , "fri" "sat", "sun"|
| allowedFromTime  | Not used | Optional | Optional | The start time per day from which access should be allowed (requires enabled = 1 and timeLimited = 1)            | "HH:MM"                                |
| allowedUntilTime | Not used | Optional | Optional | The end time per day until access should be allowed (requires enabled = 1 and timeLimited = 1)                   | "HH:MM"                                |

Examples:
- Delete: `{ "action": "delete", "authId": "1234" }`
- Update: `{ "action": "update", "authId": "1234", "enabled": "1", "name": "Test", "timeLimited": "1", "allowedFrom": "2024-04-12 10:00:00", "allowedUntil": "2034-04-12 10:00:00", "allowedWeekdays": [ "mon", "tue", "sat", "sun" ], "allowedFromTime": "08:00", "allowedUntilTime": "16:00" }`

---


## Recommended LAN Setup

> 🔐 **Note:** The Nuki REST Bridge uses HTTP for communication, which means that data is transmitted in plain text within the local network — unlike HTTPS.  
> While this poses only a minimal risk in secured home environments, proper precautions should still be taken.  
> The following steps help minimize that risk when HTTP is the only available method. They do not replace encryption but reduce exposure within a trusted LAN.


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

