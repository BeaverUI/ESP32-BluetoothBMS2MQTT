# ESP32-BluetoothBMS2MQTT

Connects to Xiaoxiang BMS (www.lithiumbatterypcb.com) via Bluetooth and sends its status to an MQTT server over WiFi.

This work is based on https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32, with several enhancements/fixes:
* various bugfixes and protocol improvements (fixes for memory leaks, reboots and reconnect problems)
* more advanced timeout and connection-related features, saving energy of the BMS
* added MQTT functionality
* removed the display routines (uses MQTT instead)

## Usage
Required stuff:
- ESP32 (with Bluetooth and WiFi). This code was tested on TTGO-Energy (https://github.com/LilyGO/LILYGO-T-Energy) and T-Koala (https://github.com/LilyGO/T-Koala) boards, but should work with any ESP32.
- Raspberry Pi or other Linux server, running an MQTT server (e.g. mosquitto) and something to display the data (e.g. Node-RED).

To configure the module, change the following in main.ino:
- configure your MQTT server and set the node name
- configure your WiFi (SSID + password)
- program the code, check the MQTT messages for the correct BMS device name and device address
- configure the BLE name and address to the detected BMS name and addres
