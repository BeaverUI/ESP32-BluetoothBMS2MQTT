/*
 * Xiaoxiang BMS to MQTT via WiFi
 * by Bas Vermulst - https://github.com/BeaverUI/ESP32-BluetoothBMS2MQTT
 * 
 * Based on original work from https://github.com/kolins-cz/Smart-BMS-Bluetooth-ESP32/blob/master/README.md
 * 
 * Settings:
 *  Flash mode DIO
 *  Partitioning: 1.9MB app (otherwise it won't fit!)
 *  CPU clock: 80 MHz
 * 
 * Uses ESP-WROVER package from https://dl.espressif.com/dl/package_esp32_index.json
 */

#include "datatypes.h" // for brevity the BMS stuff is in this file

#include <WiFi.h> // for WiFi
#include "BLEDevice.h" // for BLE
#include <ESPmDNS.h> // for OTA updates
#include <ArduinoOTA.h> // for OTA updates
#include <MQTT.h> // for MQTT
#include <WiFiClient.h> // for HTTP push messages
#include <driver/adc.h> // to read ESP battery voltage

// BMS device config
#define BLE_NAME "xiaoxiang" // name of BMS (check the mqtt debug messages to see whether this is OK)
#define BLE_ADDRESS "a4:c1:38:1a:0c:49" // address of BMS (check the mqtt debug messages to see whether this is OK)
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms service
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms rx id
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms tx id

const byte cBasicInfo = 3; //datablock 3=basic info
const byte cCellInfo = 4;  //datablock 4=individual cell info
packBasicInfoStruct packBasicInfo;
packCellInfoStruct packCellInfo;
unsigned long bms_last_update_time=0;
bool bms_status;


// BLE config
#define BLE_SCAN_INTERVAL 30000 // interval for scanning devices when not connected in ms
#define BLE_SCAN_DURATION 2 // duration of scan in seconds

#define BLE_REQUEST_INTERVAL 2500 // package request interval - make this large enough not to overlap packet requests
#define BLE_PACKETSRECEIVED_BEFORE_STANDBY 0b11 // packets to gather before disconnecting
#define BLE_TIMEOUT_BEFORE_STANDBY 30000 // timeout before going to standby in ms
#define BLE_STANDBY_PERIOD 10*60*1000 // interval between data retrievals in ms (disconnected in-between)

boolean doScan = false; // becomes true when BLE is initialized and scanning is allowed
boolean doConnect = false; // becomes true when correct ID is found during scanning
boolean BLE_client_connected = false; // true when fully connected
unsigned int ble_packets_received = 0b00;


// MQTT config
#define MQTTSERVER "192.168.1.11"
#define NODE_NAME "ble"

// WiFi config
int status = WL_IDLE_STATUS;
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// Init mqttclient
MQTTClient mqttclient;
WiFiClient wificlient_mqtt;
int mqtt_reconnects=0;
int mqtt_interval=5; //default mqtt publishing interval in seconds

// Other stuff
//float battery_voltage=0; // internal battery voltage

void setup(){
  delay(1000);
  Serial.begin(115200);  

  // Start networking
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // optional: fixed IP address, it is recommended to assign a fixed IP via the DHCP server instead
  // IPAddress ip(192,168,1,31); IPAddress gateway(192,168,1,1); IPAddress subnet(255,255,0,0); WiFi.config(ip, gateway, subnet);
  Serial.print("Attempting to connect to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {Serial.print("."); delay(1000);} Serial.println("");
  Serial.println("Connected");
  Serial.println("IP address: " + IPAddressString(WiFi.localIP()));

  // Init OTA updates
  ArduinoOTA.setPort(8266);    // Port defaults to 8266
  ArduinoOTA.setHostname(NODE_NAME);  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();


  // Start MQTT
  mqttclient.begin(MQTTSERVER, wificlient_mqtt);
  mqttclient.onMessage(handleMQTTreceive);
  MQTTconnect();
  
  // Start BLE
	bleStartup();
}


// === Main stuff ====
void loop(){
  ArduinoOTA.handle();

  //getEspBatteryVoltage();
  handleMQTT();
  handleBLE(); // in BLE
}


/*
// read voltage of onboard battery (can be used with TTGO-Energy board)
void getEspBatteryVoltage(void){
  static unsigned long prev_millis=0;

  if((millis()>prev_millis+mqtt_interval*1000)||(millis()<prev_millis)){
    prev_millis=millis();

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11);

    battery_voltage = ((float) 2*adc1_get_raw(ADC1_CHANNEL_7)*(3.3/4095));
  }
}
*/

// ===== Handles for MQTT =====
// handle connection and send messages at intervals
void handleMQTT(void){
  static unsigned long prev_millis=0;

  if (!mqttclient.connected()){
    MQTTconnect();
  }else{
    mqttclient.loop();
    
    if((millis()>prev_millis+mqtt_interval*1000)||(millis()<prev_millis)){
      prev_millis=millis();
      
      // Send MQTT messages at interval
      Serial.println("Sending MQTT update");

      mqttclient.publish(GetTopic("ip"), IPAddressString(WiFi.localIP()));
      mqttclient.publish(GetTopic("free-heap"), String(ESP.getFreeHeap()));
      mqttclient.publish(GetTopic("ssid"), WiFi.SSID());
      mqttclient.publish(GetTopic("rssi"), String(WiFi.RSSI()));
      
      mqttclient.publish(GetTopic("interval"), String(mqtt_interval));
      mqttclient.publish(GetTopic("runtime"), String(millis()/1000));
      mqttclient.publish(GetTopic("reconnects"), String(mqtt_reconnects));
      //mqttclient.publish(GetTopic("battery-voltage"), String(battery_voltage,2));

      mqttclient.publish(GetTopic("bms-status"), String(bms_status));
      mqttclient.publish(GetTopic("bms-last-updated"), String( (millis()-bms_last_update_time)/1000 ));

      if(bms_status){
        mqttclient.publish(GetTopic("number-of-cells"), String(packCellInfo.NumOfCells));
        mqttclient.publish(GetTopic("current"), String((float)packBasicInfo.Amps / 1000,2));
        mqttclient.publish(GetTopic("voltage"), String((float)packBasicInfo.Volts / 1000,2));
        mqttclient.publish(GetTopic("cell-voltage"), String((float)packBasicInfo.Volts /(1000*packCellInfo.NumOfCells), 2));
        mqttclient.publish(GetTopic("soc"), String((float)packBasicInfo.CapacityRemainPercent,1));
    
        mqttclient.publish(GetTopic("temperature-1"), String((float)packBasicInfo.Temp1 / 10,1));
        mqttclient.publish(GetTopic("temperature-2"), String((float)packBasicInfo.Temp2 / 10,1));
      }
    }
  }
}

// received something on subscribed topics
void handleMQTTreceive(String &topic, String &payload) {

  if(topic.indexOf(GetTopic("interval-ref")) >=0){
    mqtt_interval = payload.toInt();
  }
}

// connect and subscribe
void MQTTconnect(void) {
  // Retry until connected
  static unsigned long prev_millis=0;

  if((!mqttclient.connected()) && ((millis()>prev_millis+5*1000)||(millis()<prev_millis))){
    prev_millis=millis();
    
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    
    if (mqttclient.connect(NODE_NAME)) {
      Serial.println("connected");
      mqttclient.publish(GetTopic("ip"), IPAddressString(WiFi.localIP()));
      
      mqttclient.subscribe(GetTopic("interval-ref"));
            
      mqtt_reconnects++;
    } else {
      Serial.print("failed");
    }
  }
}


// ===== Helper functions =====
String IPAddressString(IPAddress address){
  return String(address[0]) + "." + String(address[1]) + "." + String(address[2]) + "." + String(address[3]);
}

void mqttdebug(char* msg){
  if (mqttclient.connected()){
    mqttclient.publish(GetTopic("debug"), String(msg));
  }
}

void mqttdebug_string(String msg){
  if (mqttclient.connected()){
    mqttclient.publish(GetTopic("debug"), msg);
  }
}

String GetTopic(String topic){
  return String(NODE_NAME) + String("/") + String(topic);
}

String Float2SciStr(float number, int digits){
  char char_buffer[40];
  sprintf(char_buffer,"%.*E", digits, number);
  return String(char_buffer);
}
