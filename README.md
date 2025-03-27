# nuki-restbridge – Control Nuki Smart Locks via REST and ESP32

![Beta Status](https://img.shields.io/badge/status-beta-yellow)
[![Project Maintenance](https://img.shields.io/maintenance/yes/2024.svg)](https://github.com/CalDymos/nuki-restbridge 'GitHub Repository')
[![License](https://img.shields.io/github/license/CalDymos/nuki-restbridge.svg)](https://github.com/CalDymos/nuki-restbridge/blob/main/LICENSE 'License')


🚧 **Beta-Status**: This project is in the **beta phase**. Most features are implemented, but there may still be bugs or incomplete parts. Use with caution.

## About

***The scope of Nuki REST Bridge is to integrate Nuki devices in a local Home Automation platform via REST.***

The Nuki Bridge software runs on a ESP32 module and acts as a bridge between Nuki devices and a Home Automation platform.<br>
<br>
It communicates with a Nuki Lock through Bluetooth (BLE) and uses REST to integrate with other systems.<br>
<br>
It exposes the lock state (and much more) through REST and allows executing commands like locking and unlocking as well as changing the Nuki Lock configuration through REST.<br>

***Nuki Bridge does not integrate with the Nuki mobile app, it <span style="color:red;">can't</span> register itself as a bridge in the official Nuki mobile app.***

## Supported devices

<b>Supported ESP32 devices:</b>
- Nuki Bridge is compiled against all ESP32 models with Wi-Fi and Bluetooh Low Energy (BLE) which are supported by ESP-IDF 5.3.2 and Arduino Core 3.1.3.

<b>Not supported ESP32 devices:</b>
- The ESP32-S2 has no built-in BLE and as such can't run Nuki Bridge.

<b>Supported Nuki devices:</b>
- Nuki Smart Lock 1.0
- Nuki Smart Lock 2.0
- Nuki Smart Lock 3.0
- Nuki Smart Lock 4.0

<b>Supported Ethernet devices:</b><br>
As an alternative to Wi-Fi (which is available on any supported ESP32), the following ESP32 modules with **built-in** wired ethernet are supported:
- [Olimex ESP32-POE-{ISO}](https://www.olimex.com/Products/IoT/ESP32/)
- [WT32-ETH01](http://en.wireless-tag.com/product-item-2.html)
- [LilyGO-T-ETH{-POE}](https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series) 
- [Waveshare ESP32-S3-{POE}-ETH](https://www.waveshare.com/esp32-s3-eth.htm)
- [ESP32-poe-dev board](https://github.com/jorticus/esp32-poe-dev)
- [wESP32](https://wesp32.com/)

In principle all ESP32 (and variants) devices with built-in ethernet port are supported, but might require additional setup in platformio.ini.<br>
If Ethernet/PoE is required: an [PoE to Ethernet and USB type B/C splitter](https://www.berrybase.de/poe-splitter-rj45-48v-usb-type-c-stecker-5v-2-5a) can be used.

## Recommended ESP32 devices

We **don't** recommend using single-core ESP32 devices (ESP32-C3, ESP32-C6, ESP32-H2, ESP32-Solo1).<br>
Although Nuki Bridge supports single-core devices, Nuki Bridge uses both CPU cores (if available) to process tasks (e.g. HTTP server/BLE scanner/BLE client) and thus runs much better on dual-core devices.<br>

## First time installation

Flash the firmware to an ESP32. 
<br>

## Initial setup (Network)

Power up the ESP32 and a new Wi-Fi access point named "NukiRestBridge" should appear.<br>
The password of the access point is "NukiBridgeESP32".<br>
Connect a client device to this access point and in a browser navigate to "http://192.168.4.1".<br>
Use the web interface to connect the ESP to your preferred network.<br>
<br>
After configuring network connection, the ESP should automatically connect to your network.<br>
<br>

## Pairing with a Nuki Lock (1.0-4.0)

Make sure "Bluetooth pairing" is enabled for the Nuki device by enabling this setting in the official Nuki App in "Settings" > "Features & Configuration" > "Button and LED".
After enabling the setting press the button on the Nuki device for a few seconds.<br>
Pairing should be automatic whenever the ESP32 is powered on and no lock is paired.<br>
<br>
When pairing is successful, the web interface should show "Paired: Yes".<br>
