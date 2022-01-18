BLEScan* pBLEScan = nullptr;
BLEClient* pClient = nullptr;
BLEAdvertisedDevice* pRemoteDevice = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLERemoteService* pRemoteService = nullptr;

// 0000ff01-0000-1000-8000-00805f9b34fb
// Notifications from this characteristic is received data from BMS
// NOTIFY, READ

// 0000ff02-0000-1000-8000-00805f9b34fb
// Write this characteristic to send data to BMS
// READ, WRITE, WRITE NO RESPONSE



// ======= CALLBACKS =========
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks{
  // called for each advertising BLE server

  void onResult(BLEAdvertisedDevice advertisedDevice){
    // found a device
    
    mqttdebug_string(
        String("BLE: found ") +
        String(advertisedDevice.getName().c_str()) +
        String(" with address ") +
        String(advertisedDevice.getAddress().toString().c_str())        
      );
    
    Serial.print("BLE: found ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if found device is the one we are looking for
    if(
        strcmp(advertisedDevice.getName().c_str(), BLE_NAME)==0 &&
        strcmp(advertisedDevice.getAddress().toString().c_str(), BLE_ADDRESS)==0 &&
        advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(serviceUUID)
      ){
      mqttdebug("BLE: target device found");
      
      pBLEScan->stop();

      // avoid memory leaks
      if(pRemoteDevice != nullptr)
        delete pRemoteDevice;
      
      pRemoteDevice = new BLEAdvertisedDevice(advertisedDevice);

      doConnect = true;
    }
  }
};

class MyClientCallback : public BLEClientCallbacks{
  // called on connect/disconnect
  void onConnect(BLEClient* pclient){
    mqttdebug_string(String("BLE: connecting to ") + String(pRemoteDevice->getAddress().toString().c_str()));
  }

  void onDisconnect(BLEClient* pclient){
    BLE_client_connected = false;
    doConnect = false;
    
    mqttdebug_string(String("BLE: disconnected from ") + String(pRemoteDevice->getAddress().toString().c_str()));    
  }
};

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify){
  //this is called when BLE server sents data via notification
  //hexDump((char*)pData, length);
  bleCollectPacket((char *)pData, length);
}




// ======= OTHERS =========

void handleBLE(){
  static unsigned long prev_millis_update = 0;
  static unsigned long prev_millis_scan = 30000;
  static unsigned long prev_millis_standby=0;
  static bool toggleInfoPacket = false;

  if(((millis()>prev_millis_standby+BLE_STANDBY_PERIOD)||(millis()<prev_millis_standby)) && (!doScan)){
    prev_millis_standby=millis();
    mqttdebug("BLE: continuing");
    bleContinue();
  }
  
  if( ( (ble_packets_received == BLE_PACKETSRECEIVED_BEFORE_STANDBY) || ((millis()>prev_millis_standby+BLE_TIMEOUT_BEFORE_STANDBY)&&(BLE_client_connected)) ) && (doScan)){    
    if(ble_packets_received == BLE_PACKETSRECEIVED_BEFORE_STANDBY)
      mqttdebug("BLE: all packets received, going to standby");
    else
      mqttdebug("BLE: connection timeout, going to standby");
      
    blePause();
  }
  
  if (doConnect){
    // found the desired BLE server, now connect to it
    if (connectToServer()){
      BLE_client_connected = true;
      mqttdebug("BLE: connected");
    }else{
      BLE_client_connected = false;
      mqttdebug("BLE: failed to connect");
    }
    
    doConnect = false;
  }

  if (BLE_client_connected){        
    // if connected to BLE server, send commands at interval
    if ((millis() > prev_millis_update + BLE_REQUEST_INTERVAL) || (millis() < prev_millis_update)){ 
      prev_millis_update = millis();
      toggleInfoPacket = !toggleInfoPacket;  // alternate basic info and cell info
      if (toggleInfoPacket){
        bmsRequestBasicInfo();
      }else{
        bmsRequestCellInfo();
      }
    }

  }else if ((!doConnect)&&(doScan)){
    // we are not connected, so we can scan for devices if we like
    if ((millis() > prev_millis_scan + BLE_SCAN_INTERVAL) || (millis() < prev_millis_scan)){ // scanning period
      prev_millis_scan=millis();
      
      mqttdebug("BLE: not connected, scannig for devices");
      Serial.print("BLE is not connected, scannig for devices... ");

      // Disconnect client
      if(pClient->isConnected()){    
        pClient->disconnect();
      }

      // clear previous devices and start the scan
      pBLEScan->clearResults();
      pBLEScan->start(BLE_SCAN_DURATION, false);
      
      mqttdebug("BLE: scan finished");

      Serial.println("finished.");
    }
  }

  bms_status=((!doScan)||(BLE_client_connected)); // status is true the BMS is connected, or BLE is paused (not scanning), meaning the BMS was connected recently
}

void bleStartup(){
  Serial.print("Starting BLE... ");

  BLEDevice::init("");
  
  // Retrieve a BLE client
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  // Retrieve a BLE scanner
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  bleContinue();
  Serial.println("done");
}

void blePause(){
  // stop scanning and disconnect from all devices
  doScan=false;

  // Disconnect client
  if(pClient->isConnected()){    
      pClient->disconnect();
  }
  
  pBLEScan->stop();
  pBLEScan->clearResults();
  
  BLE_client_connected=false;
  doConnect=false;
  ble_packets_received=0;
}

void bleContinue(){
  // Prepare for scanning
  BLE_client_connected=false;
  doConnect=false;
  ble_packets_received=0;
  
  doScan=true; // start scanning for new devices
}


bool connectToServer(){
  Serial.print("Forming a connection to ");
  Serial.println(pRemoteDevice->getAddress().toString().c_str());
  
  // Connect to the remote BLE Server.
  pClient->connect(pRemoteDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  pClient->getServices(); // clear services and get a new list from server

  pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr){
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
  if (pRemoteCharacteristic == nullptr){
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID_rx.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  
  // Read the value of the characteristic.
  if (pRemoteCharacteristic->canRead()){
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value is: ");
      Serial.println(value.c_str());
  }

  if (pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

  return true;
}

void sendCommand(uint8_t *data, uint32_t dataLen){
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);

  if (pRemoteCharacteristic){
    pRemoteCharacteristic->writeValue(data, dataLen);
  }else{
    Serial.println("Remote TX characteristic not found");
  }
}
