# ESP32-BluetoothBMS2MQTT

Use an ESP32 to read the status of a Xiaoxiang BMS (www.lithiumbatterypcb.com) via Bluetooth and send its status to an MQTT server. Coded with Arduino IDE.

This work is based on https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32 by Miroslav Kolinsky, with several enhancements/fixes:
* various bugfixes and improved stability (fixed memory leaks, reboots and reconnect problems)
* connection-related improvements such as timeouts, saving energy of the BMS
* added MQTT functionality
* removed the display stuff (uses MQTT instead)

Needs an ESP32 with bluetooth and WiFi. This code is tested on a TTGO-Energy (https://github.com/LilyGO/LILYGO-T-Energy).

To configure the program, change the following in main.ino:
- configure your MQTT server and set the node name
- configure your WiFi (SSID + password)
- program the code, check the MQTT messages for the correct BMS device name and device address
- configure the BLE name and address to the detected BMS name and addres
