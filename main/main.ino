/*
 * Xiaoxiang BMS to MQTT via WiFi
 * by Bas Vermulst - https://github.com/BeaverUI/ESP32-BluetoothBMS2MQTT
 * 
 * Based on original work from https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32/blob/master/README.md
 * 
 
   === configuring ===
   Using the #define parameters in the CONFIGURATION section, do the following:
   1) configure WiFi via WIFI_SSID and WIFI_PASSWORD
   2) configure MQTT broker via MQTTSERVER
   3) set unique node name via NODE_NAME
   4) ensure the BMS settings are OK. You can verify the name and address using the "BLE Scanner" app on an Android phone.
   

   === compiling ===
   1) Add ESP-WROVER to the board manager:
   - File --> Preferences, add to board manager URLs: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   - Then, Tools --> Board --> Board manager... --> install ESP32 package v2.0.3 or later.
   

   2) Install required libraries:
   - MQTT (by Joel Gaehwiler)
   Install via "Manage Libraries..." under Tools.

   3) Configure board:
   Select Tools --> Board --> "ESP32 Arduino" --> "ESP WRover module"

   4) Connect board via USB.
   
   5) Configure programming options:
   Select appropriate programming port under Tools --> Port
   Select correct partitioning: 1.9MB app (with OTA) or 2MB app with 2 MB SPIFFS - otherwise it won't fit

   6) Program and go!
 
 */
 
// ==== CONFIGURATION ====
// BMS
#define BMS_MAX_CELLS 15 // defines size of data types
#define BMS_POLLING_INTERVAL 10*60*1000 // data output interval (shorter = connect more often = more battery consumption from BMS) in ms

// BLE
#define BLE_MIN_RSSI -75 // minimum signal strength before connection is attempted
#define BLE_NAME "xiaoxiang" // name of BMS
#define BLE_ADDRESS "a4:c1:38:1a:0c:49" // address of BMS

#define BLE_SCAN_DURATION 1 // duration of scan in seconds
#define BLE_REQUEST_DELAY 500 // package request delay after connecting - make this large enough to have the connection established in ms
#define BLE_TIMEOUT 10*1000 // timeout of scan + gathering packets (too short will fail collecting all packets) in ms

#define BLE_CALLBACK_DEBUG true // send debug messages via MQTT & serial in callbacks (handy for finding your BMS address, name, RSSI, etc)

// MQTT
#define MQTTSERVER "192.168.1.11"
#define MQTT_USERNAME "" // leave empty if no credentials are needed
#define MQTT_PASSWORD "" 
#define NODE_NAME "bms2mqtt"

// WiFi
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"

// watchdog timeout
#define WATCHDOG_TIMEOUT (BLE_TIMEOUT+10*1000) // go to sleep after x seconds





// ==== MAIN CODE ====
#include "datatypes.h" // for brevity the BMS stuff is in this file
#include <WiFi.h> // for WiFi
#include <BLEDevice.h> // for BLE
#include <MQTT.h> // for MQTT
#include <WiFiClient.h> // for MQTT

#include <driver/adc.h> // to read ESP battery voltage
#include <rom/rtc.h> // to get reset reason


// Init BMS
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms service
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms rx id
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms tx id

const byte cBasicInfo = 3; //datablock 3=basic info
const byte cCellInfo = 4;  //datablock 4=individual cell info
packBasicInfoStruct packBasicInfo;
packCellInfoStruct packCellInfo;
unsigned long bms_last_update_time=0;
bool bms_status;
#define BLE_PACKETSRECEIVED_BEFORE_STANDBY 0b11 // packets to gather before disconnecting


WiFiClient wificlient_mqtt;
MQTTClient mqttclient;


// Other stuff
float battery_voltage=0; // internal battery voltage
String debug_log_string="";
hw_timer_t * wd_timer = NULL;


void setup(){
  // use a watchdog to set the sleep timer, this avoids issues with the crashing BLE stack
  // at some point we should smash this bug, but for now this workaround ensures reliable operation
  enableWatchdogTimer();
  
  Serial.begin(115200);
  
  // connect BLE, gather data from BMS, then disconnect and unload BLE
  bleGatherPackets();
  
  
  // Start networking
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // optional: fixed IP address, it is recommended to assign a fixed IP via the DHCP server instead
  // IPAddress ip(192,168,1,31); IPAddress gateway(192,168,1,1); IPAddress subnet(255,255,0,0); WiFi.config(ip, gateway, subnet);
  Serial.print("Attempting to connect to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {Serial.print("."); delay(1000);} Serial.println("");
  Serial.println("Connected");
  Serial.println("IP address: " + IPAddressString(WiFi.localIP()));

  
  getEspBatteryVoltage();


  // Start MQTT
  mqttclient.begin(MQTTSERVER, wificlient_mqtt);
  mqttclient.onMessage(handleMQTTreceive);
  MQTTconnect();
  handleMQTT();
  
  MqttDebug("All done, disconnecting.");

  Serial.println("All done, disconnecting.");

  // reset error handling
  bool unexpected_error=false;
  if((GetResetReason(0) == String("SW_RESET")) || (GetResetReason(0) == String("SW_CPU_RESET"))) {
    MqttDebug("ERROR: Software-reset of CPU! Something messed up.");
  }

  MqttPublishDebug();

  delay(1000); // give it 1 second to flush everything
  
  mqttclient.disconnect();
  WiFi.disconnect();
  Serial.flush();

  // done, now we wait for the wdt timer interrupt that puts us in deep sleep
}


// === Main stuff ====
void loop(){

}


// enable watchdog timer -- a very ugly hack to overcome crashes of the BLE stack
// (desperate times ask for desperate measures)
void enableWatchdogTimer(){
  wd_timer = timerBegin(0, 80, true);  
  timerAttachInterrupt(wd_timer, &WatchDogTimeoutHandler, true);
  timerAlarmWrite(wd_timer, WATCHDOG_TIMEOUT*1e3, false);
  timerAlarmEnable(wd_timer);
}


// WDT handler to put ESP in deep sleep after data has been obtained
void ARDUINO_ISR_ATTR WatchDogTimeoutHandler()
{ 
  esp_sleep_enable_timer_wakeup((BMS_POLLING_INTERVAL - WATCHDOG_TIMEOUT) * 1e3); // standby period is in ms, function accepts us
  esp_deep_sleep_start(); // sweet dreams
}


// read voltage of onboard battery
void getEspBatteryVoltage(void){
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11);

  battery_voltage = ((float) 2*adc1_get_raw(ADC1_CHANNEL_7)*(3.3*1.06/4095));
}


// ===== Handles for MQTT =====
// handle connection and send messages at intervals
void handleMQTT(void){
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("WiFi disconnected. Reconnecting.");
  }
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  
  if (!mqttclient.connected()){
    MQTTconnect();
  }

  // Send MQTT message
  Serial.println("Sending MQTT update");

  mqttclient.publish(GetTopic("ip"), IPAddressString(WiFi.localIP()));
  mqttclient.publish(GetTopic("free-heap"), String(ESP.getFreeHeap()));
  mqttclient.publish(GetTopic("maxalloc-heap"), String(xPortGetMinimumEverFreeHeapSize()));
  mqttclient.publish(GetTopic("ssid"), WiFi.SSID());
  mqttclient.publish(GetTopic("rssi"), String(WiFi.RSSI()));
  
  mqttclient.publish(GetTopic("reset-reason"), String(GetResetReason(0)) + String(" | ") + String(GetResetReason(1)));
  
  mqttclient.publish(GetTopic("runtime"), String(millis()/1000));
  mqttclient.publish(GetTopic("battery-voltage"), String(battery_voltage,2));

  mqttclient.publish(GetTopic("bms-status"), String(bms_status));
  mqttclient.publish(GetTopic("bms-status-age"), String( (millis()-bms_last_update_time)/1000 ));

  if(bms_status){
    mqttclient.publish(GetTopic("number-of-cells"), String(packCellInfo.NumOfCells));
    mqttclient.publish(GetTopic("current"), String((float)packBasicInfo.Amps / 1000,2));
    mqttclient.publish(GetTopic("voltage"), String((float)packBasicInfo.Volts / 1000,2));
    if(packCellInfo.NumOfCells != 0){
      mqttclient.publish(GetTopic("cell-voltage"), String((float)packBasicInfo.Volts /(1000*packCellInfo.NumOfCells), 2));
    }
    mqttclient.publish(GetTopic("cell-diff"), String((float)packCellInfo.CellDiff, 0));
    mqttclient.publish(GetTopic("soc"), String((float)packBasicInfo.CapacityRemainPercent,1));

    mqttclient.publish(GetTopic("temperature-1"), String((float)packBasicInfo.Temp1 / 10,1));
    mqttclient.publish(GetTopic("temperature-2"), String((float)packBasicInfo.Temp2 / 10,1));
  }

  MqttPublishDebug();
  
  mqttclient.loop();
}


// handler for incoming messages
void handleMQTTreceive(String &topic, String &payload) {
}


// ===== Helper functions =====
String GetResetReason(int core)
{
  RESET_REASON reason=rtc_get_reset_reason(core);
  switch (reason)
  {
    case 1 : return String("POWERON_RESET");break;          /**<1, Vbat power on reset*/
    case 3 : return String("SW_RESET");break;               /**<3, Software reset digital core*/
    case 4 : return String("OWDT_RESET");break;             /**<4, Legacy watch dog reset digital core*/
    case 5 : return String("DEEPSLEEP_RESET");break;        /**<5, Deep Sleep reset digital core*/
    case 6 : return String("SDIO_RESET");break;             /**<6, Reset by SLC module, reset digital core*/
    case 7 : return String("TG0WDT_SYS_RESET");break;       /**<7, Timer Group0 Watch dog reset digital core*/
    case 8 : return String("TG1WDT_SYS_RESET");break;       /**<8, Timer Group1 Watch dog reset digital core*/
    case 9 : return String("RTCWDT_SYS_RESET");break;       /**<9, RTC Watch dog Reset digital core*/
    case 10 : return String("INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : return String("TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : return String("SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : return String("RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : return String("EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : return String("RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : return String("RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : return String("NO_MEAN");
  }
}
