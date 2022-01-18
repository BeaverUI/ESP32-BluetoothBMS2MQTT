# ESP32-BluetoothBMS2MQTT

Connects to Xiaoxiang BMS (www.lithiumbatterypcb.com) via Bluetooth and sends its status to an MQTT server over WiFi.

This work is based on https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32 by Miroslav Kolinsky, with several enhancements/fixes:
* various bugfixes (fixed memory leaks, reboots and reconnect problems)
* more advanced timeout and connection-related features, saving energy of the BMS
* added MQTT functionality
* removed the display stuff (uses MQTT instead)

Needs ESP32 with bluetooth and WiFi.
Tested on TTGO-Energy (https://github.com/LilyGO/LILYGO-T-Energy) and T-Koala (https://github.com/LilyGO/T-Koala) boards.

To configure the module, change the following in main.ino:
- configure your MQTT server and set the node name
- configure your WiFi (SSID + password)
- program the code, check the MQTT messages for the correct BMS device name and device address
- configure the BLE name and address to the detected BMS name and addres
