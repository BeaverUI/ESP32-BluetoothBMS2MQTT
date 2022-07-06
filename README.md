# ESP32-BluetoothBMS2MQTT

Connects to Xiaoxiang BMS (www.lithiumbatterypcb.com) via Bluetooth and sends its status to an MQTT server over WiFi.

Use the Releases (right side of this page) to download the latest version.


This work is based on https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32, with several enhancements/fixes:
* various bugfixes and protocol improvements (fixes for memory leaks, reboots and reconnect problems)
* more advanced timeout and connection-related features, saving energy of the BMS
* added MQTT functionality
* removed the display routines (uses MQTT instead)

## Required stuff
* ESP32 (with Bluetooth and WiFi). This code was tested on TTGO-Energy (https://github.com/LilyGO/LILYGO-T-Energy) and T-Koala (https://github.com/LilyGO/T-Koala) boards, but should work with any ESP32.
* Raspberry Pi or other Linux server, running an MQTT server (e.g. mosquitto) and something to display the data (e.g. Node-RED).
* In Node-RED, I recommend to use the "node-red-dashboard" library to display status on a nice web-based dashboard.

Note: some ESP boards experience brownouts (resets) with this code due to the high consumption of WiFi + Bluetooth which causes power supply stability issues. This can be fixed by removing the diode that is in series with the USB connector, and adding a 220uF (or similar) electrolytic capacitor between +5V and GND, and another one between +3V3 and GND (these pins are typically available on the headers, so they are easy to add). When adding one, mind the polarity of the capacitor.

## Programming the ESP
* Install the Arduino IDE
* Configure the IDE (see code for more info):
	* In preferences, add the board manager URL
	* Install the required libraries
* Open main.ino
* Configure the programmer:
	* Connect the ESP32 board via USB, select the correct COM port
	* See programmer-config.png for the other settings

## Configuring
To configure the module, change the following in main.ino:
* configure your MQTT server and set the node name
* configure your WiFi (SSID + password)
* program the code, check the MQTT messages for the correct BMS device name and device address
* configure the BLE name and address to the detected BMS name and addres

## Example output in Node-RED
<img src="Example implementation Node-RED.png" width="500px"></br>
