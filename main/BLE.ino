BLEScan* pBLEScan = nullptr;
BLEClient* pClient = nullptr;
BLEAdvertisedDevice* pRemoteDevice = nullptr;
BLERemoteService* pRemoteService = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic_rx = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic_tx = nullptr;

// 0000ff01-0000-1000-8000-00805f9b34fb
// Notifications from this characteristic is received data from BMS
// NOTIFY, READ

// 0000ff02-0000-1000-8000-00805f9b34fb
// Write this characteristic to send data to BMS
// READ, WRITE, WRITE NO RESPONSE



// ======= CALLBACKS =========

void MyEndOfScanCallback(BLEScanResults pBLEScanResult){
    if(BLE_DEBUG_OVER_MQTT){
      MqttDebug("BLE: scan finished");
    }
    
    Serial.println("Scan finished.");
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks{
  // called for each advertising BLE server

  void onResult(BLEAdvertisedDevice advertisedDevice){
    // found a device


    if(BLE_DEBUG_OVER_MQTT){
      MqttDebug(
          String("BLE: found ") +
          String(advertisedDevice.getName().c_str()) +
          String(" with address ") +
          String(advertisedDevice.getAddress().toString().c_str()) + 
          String(" and RSSI ") +
          String(advertisedDevice.getRSSI())        
        );
    }
    
    Serial.print("BLE: found ");
    Serial.println(advertisedDevice.toString().c_str());
    

    // Check if found device is the one we are looking for
    if(
        strcmp(advertisedDevice.getName().c_str(), BLE_NAME)==0 &&
        strcmp(advertisedDevice.getAddress().toString().c_str(), BLE_ADDRESS)==0 &&
        advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(serviceUUID)
      ){
    
      if(BLE_DEBUG_OVER_MQTT){      
        MqttDebug("BLE: target device found");
      }
      
      pBLEScan->stop();

      if(advertisedDevice.getRSSI() >= BLE_MIN_RSSI){
 
        // delete old remote device, create new one
        if(pRemoteDevice != nullptr){
          delete pRemoteDevice;
        }
        pRemoteDevice = new BLEAdvertisedDevice(advertisedDevice);
  
        doConnect = true;        
      }else{
        if(BLE_DEBUG_OVER_MQTT){
          MqttDebug("BLE: RSSI of target device below minimum");
        }
      }
    }
  }
};

class MyClientCallback : public BLEClientCallbacks{
  // called on connect/disconnect
  void onConnect(BLEClient* pclient){
    if(BLE_DEBUG_OVER_MQTT){
      MqttDebug(String("BLE: connecting to ") + String(pclient->getPeerAddress().toString().c_str()));
    }
  }

  void onDisconnect(BLEClient* pclient){
    ble_client_connected = false;
    doConnect = false;

    if(BLE_DEBUG_OVER_MQTT){
      MqttDebug(String("BLE: disconnected from ") + String(pclient->getPeerAddress().toString().c_str()));
    }
  }
};

static void MyNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify){
  //this is called when BLE server sents data via notification
  //hexDump((char*)pData, length);
  bleCollectPacket((char *)pData, length);
}





// ======= OTHERS =========

void handleBLE(){
  static unsigned long prev_millis_update = 0;
  static unsigned long prev_millis_scan = 30000;
  static unsigned long prev_millis_standby=0;

  if(((millis()>=prev_millis_standby+BLE_STANDBY_PERIOD)||(millis()<prev_millis_standby)) && (!doScan)){
    prev_millis_standby=millis();
    MqttDebug("BLE: continuing");
    bleContinue();
  }
  
  if( ( (ble_packets_received == BLE_PACKETSRECEIVED_BEFORE_STANDBY) || ((millis()>prev_millis_standby+BLE_TIMEOUT_BEFORE_STANDBY)&&(ble_client_connected)) ) && (doScan)){    
    if(ble_packets_received == BLE_PACKETSRECEIVED_BEFORE_STANDBY){
      MqttDebug("BLE: all packets received, going to standby");
    }else{
      MqttDebug("BLE: connection timeout, going to standby");
    }
          
    blePause();
  }
  
  if (doConnect){
    // found the desired BLE server, now connect to it
    if (connectToServer()){
      ble_client_connected = true;
      ble_packets_received=0;
      
      MqttDebug("BLE: connected");
    }else{
      ble_client_connected = false;
      MqttDebug("BLE: failed to connect");
    }
    
    doConnect = false;
  }

  if (ble_client_connected){        
    // if connected to BLE server, send commands at interval
    if ((millis()>=prev_millis_update+BLE_REQUEST_INTERVAL) || (millis() < prev_millis_update)){ 
      prev_millis_update = millis();
      
      if ((ble_packets_received & 0b01) == 0){
        bmsRequestBasicInfo(); // request packet 0b01
      }else if((ble_packets_received & 0b10) == 0){
        bmsRequestCellInfo(); // request packet 0b10
      }
    }

  }else if ((!doConnect)&&(doScan)){
    // we are not connected, so we can scan for devices if we like
    if ((millis()>=prev_millis_scan+BLE_SCAN_INTERVAL) || (millis() < prev_millis_scan)){ // scanning period
      prev_millis_scan=millis();
      
      MqttDebug("BLE: not connected, starting scan");
      Serial.print("BLE is not connected, starting scan");

      // Disconnect client
      if((pClient != nullptr)&&(pClient->isConnected())){
        pClient->disconnect();
      }

      // stop scan (if running) and start a new one
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(1 << 8); // 160 ms
      pBLEScan->setWindow(1 << 7); // 80 ms
      pBLEScan->start(BLE_SCAN_DURATION, MyEndOfScanCallback, false); // non-blocking, use a callback
      
      MqttDebug("BLE: scan started");      
    }
  }
  
  // if BMS is connected or BLE is paused (not scanning), the BMS was connected recently
  bms_status=((!doScan)||(ble_client_connected));
}

void bleStart(){
  Serial.print("Starting BLE... ");

  BLEDevice::init("");
  
  // Retrieve a BLE client
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  // Retrieve a BLE scanner
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());

  bleContinue();
  Serial.println("done");
}

void blePause(){
  // stop scanning and disconnect from all devices
  doScan=false;

  // Disconnect client
  if((pClient != nullptr)&&(pClient->isConnected())){
    pClient->disconnect();
  }
  
  pBLEScan->stop();
  
  ble_client_connected=false;
  doConnect=false;
  ble_packets_received=0;
}


void bleContinue(){
  // Prepare for scanning
  ble_client_connected=false;
  doConnect=false;
  ble_packets_received=0;
  
  doScan=true; // start scanning for new devices
}


bool connectToServer(){
  if(pRemoteDevice==nullptr){
    Serial.println("Invalid remote device, can't connect");
    return false;
  }
  
  Serial.print("Forming a connection to ");
  Serial.println(pRemoteDevice->getAddress().toString().c_str());
  
  // Connect to the remote BLE Server.
  if(pClient->connect(pRemoteDevice)){
    Serial.println(" - Connected to server");
  }else{
    Serial.println(" - Failed to connect to server");
    return false;   
  }

  // Get remote service
  pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr){
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");
  
  // Get BMS receive characteristic
  pRemoteCharacteristic_rx = pRemoteService->getCharacteristic(charUUID_rx);
  if (pRemoteCharacteristic_rx == nullptr){
    Serial.print("Failed to find rx UUID: ");
    Serial.println(charUUID_rx.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found RX characteristic");

  if (pRemoteCharacteristic_rx->canNotify())
      pRemoteCharacteristic_rx->registerForNotify(MyNotifyCallback);


  // Get BMS transmit characteristic
  pRemoteCharacteristic_tx = pRemoteService->getCharacteristic(charUUID_tx);
  if (pRemoteCharacteristic_tx == nullptr){
    Serial.print("Failed to find tx UUID: ");
    Serial.println(charUUID_tx.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found TX characteristic");

  
  return true;
}

void sendCommand(uint8_t *data, uint32_t dataLen){
  if((pClient!=nullptr)&&(pClient->isConnected())){
    pRemoteCharacteristic_tx->writeValue(data, dataLen, false);
  }
}
